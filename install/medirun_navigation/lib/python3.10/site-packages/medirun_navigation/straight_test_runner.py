import math
from typing import List

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node

from .angle_math import shortest_angular_distance, yaw_from_quaternion


def compute_linear_speed(
    remaining: float,
    linear_speed: float,
    min_linear_speed: float,
    slowdown_distance: float,
) -> float:
    if remaining <= 0.0:
        return 0.0

    commanded_speed = linear_speed
    if remaining < slowdown_distance:
        commanded_speed = linear_speed * (remaining / slowdown_distance)
    return max(min_linear_speed, min(linear_speed, commanded_speed))


def compute_progress_errors(
    start_x: float,
    start_y: float,
    current_x: float,
    current_y: float,
    heading: float,
) -> tuple[float, float]:
    dx = current_x - start_x
    dy = current_y - start_y
    forward_progress = dx * math.cos(heading) + dy * math.sin(heading)
    lateral_error = -dx * math.sin(heading) + dy * math.cos(heading)
    return forward_progress, lateral_error


class StraightTestRunner(Node):
    def __init__(self) -> None:
        super().__init__("straight_test_runner")

        self.declare_parameter("odom_topic", "/odom")
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter("distances_m", [4.5])
        self.declare_parameter("repeat_count", 1)
        self.declare_parameter("linear_speed", 0.05)
        self.declare_parameter("min_linear_speed", 0.02)
        self.declare_parameter("slowdown_distance_m", 0.18)
        self.declare_parameter("position_tolerance_m", 0.008)
        self.declare_parameter("heading_kp", 0.35)
        self.declare_parameter("max_angular_speed", 0.03)
        self.declare_parameter("heading_deadband_deg", 3.0)
        self.declare_parameter("heading_tolerance_deg", 4.0)
        self.declare_parameter("lateral_tolerance_m", 0.02)
        self.declare_parameter("settle_time_sec", 0.5)
        self.declare_parameter("repeat_delay_sec", 0.0)
        self.declare_parameter("loop_hz", 20.0)

        odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        cmd_vel_topic = (
            self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        )
        self.distance_sequence_m = self._read_distance_sequence()
        self.repeat_count = (
            self.get_parameter("repeat_count").get_parameter_value().integer_value
        )
        self.linear_speed = (
            self.get_parameter("linear_speed").get_parameter_value().double_value
        )
        self.min_linear_speed = (
            self.get_parameter("min_linear_speed").get_parameter_value().double_value
        )
        self.slowdown_distance = (
            self.get_parameter("slowdown_distance_m").get_parameter_value().double_value
        )
        self.position_tolerance = (
            self.get_parameter("position_tolerance_m").get_parameter_value().double_value
        )
        self.heading_kp = (
            self.get_parameter("heading_kp").get_parameter_value().double_value
        )
        self.max_angular_speed = (
            self.get_parameter("max_angular_speed").get_parameter_value().double_value
        )
        self.heading_deadband = math.radians(
            self.get_parameter("heading_deadband_deg")
            .get_parameter_value()
            .double_value
        )
        self.heading_tolerance = math.radians(
            self.get_parameter("heading_tolerance_deg")
            .get_parameter_value()
            .double_value
        )
        self.lateral_tolerance = (
            self.get_parameter("lateral_tolerance_m").get_parameter_value().double_value
        )
        self.settle_time_sec = (
            self.get_parameter("settle_time_sec").get_parameter_value().double_value
        )
        self.repeat_delay_sec = (
            self.get_parameter("repeat_delay_sec").get_parameter_value().double_value
        )
        loop_hz = self.get_parameter("loop_hz").get_parameter_value().double_value

        self.cmd_pub = self.create_publisher(Twist, cmd_vel_topic, 10)
        self.create_subscription(Odometry, odom_topic, self._odom_callback, 10)
        self.timer = self.create_timer(1.0 / loop_hz, self._on_timer)

        self.current_x = None
        self.current_y = None
        self.current_yaw = None
        self.current_step = 0
        self.current_run = 1
        self.state = "wait_for_odom"
        self.state_deadline = None
        self.completed = False
        self.step_start_x = None
        self.step_start_y = None
        self.step_target_distance = None
        self.step_target_heading = None
        self.step_final_forward = None
        self.step_final_lateral = None
        self.step_final_heading_error = None

        if self.repeat_count < 1:
            raise ValueError("repeat_count must be at least 1.")
        if self.linear_speed <= 0.0:
            raise ValueError("linear_speed must be greater than 0.")
        if self.min_linear_speed <= 0.0:
            raise ValueError("min_linear_speed must be greater than 0.")
        if self.min_linear_speed > self.linear_speed:
            raise ValueError("min_linear_speed must be <= linear_speed.")
        if self.slowdown_distance <= 0.0:
            raise ValueError("slowdown_distance_m must be greater than 0.")
        if self.position_tolerance <= 0.0:
            raise ValueError("position_tolerance_m must be greater than 0.")
        if self.lateral_tolerance <= 0.0:
            raise ValueError("lateral_tolerance_m must be greater than 0.")
        if self.max_angular_speed <= 0.0:
            raise ValueError("max_angular_speed must be greater than 0.")
        if self.repeat_delay_sec < 0.0:
            raise ValueError("repeat_delay_sec must be >= 0.")

        self.get_logger().info(
            "Straight sheet test ready: distances=%s m, repeat_count=%d, speed=%.2f m/s, min_speed=%.2f m/s, slowdown=%.2f m, position_tolerance=%.3f m, lateral_tolerance=%.3f m, heading_tolerance=%.1f deg"
            % (
                self.distance_sequence_m,
                self.repeat_count,
                self.linear_speed,
                self.min_linear_speed,
                self.slowdown_distance,
                self.position_tolerance,
                self.lateral_tolerance,
                math.degrees(self.heading_tolerance),
            )
        )

    def _read_distance_sequence(self) -> List[float]:
        values = self.get_parameter("distances_m").get_parameter_value().double_array_value
        if not values:
            raise ValueError("distances_m must contain at least one distance.")
        distances = [float(value) for value in values]
        if any(distance <= 0.0 for distance in distances):
            raise ValueError("distances_m must contain only positive values.")
        return distances

    def _odom_callback(self, msg: Odometry) -> None:
        self.current_x = msg.pose.pose.position.x
        self.current_y = msg.pose.pose.position.y
        self.current_yaw = yaw_from_quaternion(msg.pose.pose.orientation)

    def _on_timer(self) -> None:
        if self.completed:
            return

        if self.current_x is None or self.current_y is None or self.current_yaw is None:
            return

        now = self.get_clock().now().nanoseconds / 1e9
        if self.state == "wait_for_odom":
            self._start_next_step()
            return

        if self.state == "moving":
            forward_progress, lateral_error = self._progress_errors()
            remaining = self.step_target_distance - forward_progress
            heading_error = shortest_angular_distance(
                self.current_yaw, self.step_target_heading
            )
            if (
                abs(remaining) <= self.position_tolerance
                and abs(lateral_error) <= self.lateral_tolerance
                and abs(heading_error) <= self.heading_tolerance
            ):
                self.step_final_forward = forward_progress
                self.step_final_lateral = lateral_error
                self.step_final_heading_error = heading_error
                self._publish_stop()
                self.state = "verifying"
                self.state_deadline = now + self.settle_time_sec
                return

            self._publish_motion_command(remaining, heading_error)
            return

        if self.state == "verifying":
            self._publish_stop()
            if now >= self.state_deadline:
                self._complete_step_if_stable()
            return

        if self.state == "settling":
            self._publish_stop()
            if now >= self.state_deadline:
                self._start_next_step()

    def _progress_errors(self) -> tuple[float, float]:
        return compute_progress_errors(
            self.step_start_x,
            self.step_start_y,
            self.current_x,
            self.current_y,
            self.step_target_heading,
        )

    def _publish_motion_command(self, remaining: float, heading_error: float) -> None:
        twist = Twist()
        twist.linear.x = compute_linear_speed(
            remaining,
            self.linear_speed,
            self.min_linear_speed,
            self.slowdown_distance,
        )

        if abs(heading_error) <= self.heading_deadband:
            angular_cmd = 0.0
        else:
            angular_cmd = self.heading_kp * heading_error
        angular_cmd = max(-self.max_angular_speed, min(self.max_angular_speed, angular_cmd))
        twist.angular.z = angular_cmd
        self.cmd_pub.publish(twist)

    def _complete_step_if_stable(self) -> None:
        forward_progress = self.step_final_forward
        lateral_error = self.step_final_lateral
        heading_error = self.step_final_heading_error
        remaining = self.step_target_distance - forward_progress

        self.get_logger().info(
            "Run %d/%d Step %d done: target=%.3f m, forward=%.3f m, residual=%.3f m, lateral_error=%.3f m, heading_error=%.2f deg"
            % (
                self.current_run,
                self.repeat_count,
                self.current_step,
                self.step_target_distance,
                forward_progress,
                remaining,
                lateral_error,
                math.degrees(heading_error),
            )
        )
        delay_sec = self.settle_time_sec
        if (
            self.current_step >= len(self.distance_sequence_m)
            and self.current_run < self.repeat_count
        ):
            delay_sec = self.repeat_delay_sec
        self.state = "settling"
        self.state_deadline = self.get_clock().now().nanoseconds / 1e9 + delay_sec

    def _start_next_step(self) -> None:
        if self.current_step >= len(self.distance_sequence_m):
            if self.current_run >= self.repeat_count:
                self._publish_stop()
                self.completed = True
                self.state = "done"
                self.get_logger().info(
                    "Straight sheet test complete: %d/%d runs finished."
                    % (self.current_run, self.repeat_count)
                )
                return

            self.current_run += 1
            self.current_step = 0
            self.get_logger().info(
                "Starting next run: %d/%d" % (self.current_run, self.repeat_count)
            )

        self.step_start_x = self.current_x
        self.step_start_y = self.current_y
        self.step_target_heading = self.current_yaw
        self.step_target_distance = self.distance_sequence_m[self.current_step]
        self.step_final_forward = None
        self.step_final_lateral = None
        self.step_final_heading_error = None
        self.current_step += 1
        self.state = "moving"
        self.get_logger().info(
            "Run %d/%d Step %d start: drive %.3f m from current pose"
            % (
                self.current_run,
                self.repeat_count,
                self.current_step,
                self.step_target_distance,
            )
        )

    def _publish_stop(self) -> None:
        if not rclpy.ok():
            return
        self.cmd_pub.publish(Twist())


def main() -> None:
    rclpy.init()
    node = StraightTestRunner()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node._publish_stop()
        node.destroy_node()
        rclpy.shutdown()
