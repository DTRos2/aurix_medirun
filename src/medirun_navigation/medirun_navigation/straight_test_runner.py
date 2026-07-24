import math
from datetime import datetime
from pathlib import Path

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from sensor_msgs.msg import Imu

from .angle_math import shortest_angular_distance, yaw_from_quaternion


class StraightTestRunner(Node):
    def __init__(self) -> None:
        super().__init__("straight_test_runner")

        self.declare_parameter("odom_topic", "/odom")
        self.declare_parameter("imu_topic", "/imu")
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter("distance_m", 0.45)
        self.declare_parameter("repeat_count", 1)
        self.declare_parameter("command_compensation_m", 0.0)
        self.declare_parameter("linear_speed", 0.10)
        self.declare_parameter("min_linear_speed", 0.05)
        self.declare_parameter("slowdown_distance_m", 0.15)
        self.declare_parameter("distance_tolerance_m", 0.01)
        self.declare_parameter("heading_kp", 1.5)
        self.declare_parameter("max_angular_speed", 0.20)
        self.declare_parameter("settle_time_sec", 0.5)
        self.declare_parameter("repeat_delay_sec", 3.0)
        self.declare_parameter("loop_hz", 20.0)
        self.declare_parameter("report_output_dir", "docs/tests")
        self.declare_parameter("test_id", "T-01")

        odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        imu_topic = self.get_parameter("imu_topic").get_parameter_value().string_value
        cmd_vel_topic = (
            self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        )

        self.distance_m = (
            self.get_parameter("distance_m").get_parameter_value().double_value
        )
        self.repeat_count = (
            self.get_parameter("repeat_count").get_parameter_value().integer_value
        )
        self.command_compensation_m = (
            self.get_parameter("command_compensation_m")
            .get_parameter_value()
            .double_value
        )
        self.linear_speed = (
            self.get_parameter("linear_speed").get_parameter_value().double_value
        )
        self.min_linear_speed = (
            self.get_parameter("min_linear_speed").get_parameter_value().double_value
        )
        self.slowdown_distance_m = (
            self.get_parameter("slowdown_distance_m")
            .get_parameter_value()
            .double_value
        )
        self.distance_tolerance_m = (
            self.get_parameter("distance_tolerance_m")
            .get_parameter_value()
            .double_value
        )
        self.heading_kp = (
            self.get_parameter("heading_kp").get_parameter_value().double_value
        )
        self.max_angular_speed = (
            self.get_parameter("max_angular_speed").get_parameter_value().double_value
        )
        self.settle_time_sec = (
            self.get_parameter("settle_time_sec").get_parameter_value().double_value
        )
        self.repeat_delay_sec = (
            self.get_parameter("repeat_delay_sec").get_parameter_value().double_value
        )
        loop_hz = self.get_parameter("loop_hz").get_parameter_value().double_value
        report_output_dir = (
            self.get_parameter("report_output_dir").get_parameter_value().string_value
        )
        self.test_id = self.get_parameter("test_id").get_parameter_value().string_value

        self.cmd_pub = self.create_publisher(Twist, cmd_vel_topic, 10)
        self.create_subscription(Odometry, odom_topic, self._odom_callback, 10)
        self.create_subscription(Imu, imu_topic, self._imu_callback, 10)
        self.timer = self.create_timer(1.0 / loop_hz, self._on_timer)

        self.current_position = None
        self.current_odom_yaw = None
        self.current_imu_yaw = None
        self.current_run = 1
        self.state = "wait_for_odom"
        self.state_deadline = None
        self.start_position = None
        self.start_odom_yaw = None
        self.start_imu_yaw = None
        self.target_distance_m = None
        self.completed = False
        self.last_progress_log_remaining_m = None
        self.run_started_at = datetime.now()
        self.report_output_dir = Path(report_output_dir)
        self.step_results = []

        if self.repeat_count < 1:
            raise ValueError("repeat_count must be at least 1.")
        if self.distance_m <= 0.0:
            raise ValueError("distance_m must be greater than 0.")
        if self.linear_speed <= 0.0:
            raise ValueError("linear_speed must be greater than 0.")
        if self.min_linear_speed <= 0.0:
            raise ValueError("min_linear_speed must be greater than 0.")
        if self.min_linear_speed > self.linear_speed:
            raise ValueError("min_linear_speed must be <= linear_speed.")
        if self.slowdown_distance_m <= 0.0:
            raise ValueError("slowdown_distance_m must be greater than 0.")
        if self.distance_tolerance_m <= 0.0:
            raise ValueError("distance_tolerance_m must be greater than 0.")
        if self.max_angular_speed < 0.0:
            raise ValueError("max_angular_speed must be >= 0.")
        if self.repeat_delay_sec < 0.0:
            raise ValueError("repeat_delay_sec must be >= 0.")

        self.get_logger().info(
            "[READY] distance=%.3f m, repeat_count=%d, compensation=%.3f m, speed=%.2f m/s, min_speed=%.2f m/s, repeat_delay=%.1f sec, odom_topic=%s, imu_topic=%s"
            % (
                self.distance_m,
                self.repeat_count,
                self.command_compensation_m,
                self.linear_speed,
                self.min_linear_speed,
                self.repeat_delay_sec,
                odom_topic,
                imu_topic,
            )
        )

    def _odom_callback(self, msg: Odometry) -> None:
        position = msg.pose.pose.position
        self.current_position = (position.x, position.y)
        self.current_odom_yaw = yaw_from_quaternion(msg.pose.pose.orientation)

    def _imu_callback(self, msg: Imu) -> None:
        self.current_imu_yaw = yaw_from_quaternion(msg.orientation)

    def _on_timer(self) -> None:
        if self.completed:
            return

        if self.current_position is None or self.current_odom_yaw is None:
            return

        now = self.get_clock().now().nanoseconds / 1e9
        if self.state == "wait_for_odom":
            self._start_next_run()
            return

        if self.state == "moving":
            traveled = self._compute_traveled_distance()
            remaining = self.target_distance_m - traveled
            if remaining <= self.distance_tolerance_m:
                self._publish_stop()
                self.state = "verifying"
                self.state_deadline = now + self.settle_time_sec
                return

            self._log_progress(remaining)
            self._publish_drive_command(remaining)
            return

        if self.state == "verifying":
            self._publish_stop()
            if now >= self.state_deadline:
                self._complete_run_if_stable()
            return

        if self.state == "settling":
            self._publish_stop()
            if now >= self.state_deadline:
                self._start_next_run()

    def _publish_drive_command(self, remaining: float) -> None:
        twist = Twist()
        commanded_speed = self.linear_speed
        if remaining < self.slowdown_distance_m:
            commanded_speed = self.linear_speed * (remaining / self.slowdown_distance_m)
        commanded_speed = max(
            self.min_linear_speed, min(self.linear_speed, commanded_speed)
        )

        heading_error = shortest_angular_distance(
            self.current_odom_yaw, self.start_odom_yaw
        )
        angular_cmd = self.heading_kp * heading_error
        angular_cmd = max(-self.max_angular_speed, min(self.max_angular_speed, angular_cmd))

        twist.linear.x = commanded_speed
        twist.angular.z = angular_cmd
        self.cmd_pub.publish(twist)

    def _complete_run_if_stable(self) -> None:
        traveled = self._compute_traveled_distance()
        remaining = self.target_distance_m - traveled
        if remaining > self.distance_tolerance_m:
            self.state = "moving"
            return

        measured_distance = traveled
        error_distance = measured_distance - self.distance_m
        lateral_error = self._compute_lateral_error()
        odom_yaw_change_deg = math.degrees(
            shortest_angular_distance(self.start_odom_yaw, self.current_odom_yaw)
        )
        imu_yaw_change_deg = self._compute_imu_yaw_change_deg()

        self.step_results.append(
            {
                "run": self.current_run,
                "request_distance_m": self.distance_m,
                "target_distance_m": self.target_distance_m,
                "measured_distance_m": measured_distance,
                "distance_error_m": error_distance,
                "distance_residual_m": remaining,
                "lateral_error_m": lateral_error,
                "start_x_m": self.start_position[0],
                "start_y_m": self.start_position[1],
                "end_x_m": self.current_position[0],
                "end_y_m": self.current_position[1],
                "odom_yaw_change_deg": odom_yaw_change_deg,
                "imu_yaw_change_deg": imu_yaw_change_deg,
            }
        )

        self.get_logger().info(
            "[RESULT] run=%d/%d request=%.3f m, target=%.3f m"
            % (
                self.current_run,
                self.repeat_count,
                self.distance_m,
                self.target_distance_m,
            )
        )
        self.get_logger().info(
            "[RESULT] odom: measured=%.3f m, error=%.3f m, residual=%.3f m, lateral=%.3f m"
            % (
                measured_distance,
                error_distance,
                remaining,
                lateral_error,
            )
        )
        self.get_logger().info(
            "[RESULT] yaw : odom=%s deg, imu=%s deg"
            % (
                self._format_optional_value(odom_yaw_change_deg, "deg"),
                self._format_optional_value(imu_yaw_change_deg, "deg"),
            )
        )
        self.get_logger().info("[RESULT] cmd_vel.linear.x=0.0")

        delay_sec = self.repeat_delay_sec
        if self.current_run >= self.repeat_count:
            delay_sec = self.settle_time_sec

        self.state = "settling"
        self.state_deadline = self.get_clock().now().nanoseconds / 1e9 + delay_sec

    def _start_next_run(self) -> None:
        if self.current_run > self.repeat_count:
            self._publish_stop()
            self.completed = True
            self.state = "done"
            report_path = self._write_report()
            self.get_logger().info(
                "[DONE] straight test complete: %d/%d runs finished."
                % (self.repeat_count, self.repeat_count)
            )
            self.get_logger().info("[DONE] report saved: %s" % report_path)
            return

        self.start_position = self.current_position
        self.start_odom_yaw = self.current_odom_yaw
        self.start_imu_yaw = self.current_imu_yaw
        self.target_distance_m = self._apply_command_compensation(self.distance_m)
        self.last_progress_log_remaining_m = None
        self.state = "moving"
        self.get_logger().info(
            "[START] run=%d/%d request=%.3f m, target=%.3f m"
            % (
                self.current_run,
                self.repeat_count,
                self.distance_m,
                self.target_distance_m,
            )
        )
        self.get_logger().info(
            "[START] odom=(%.3f, %.3f) m, imu_yaw=%s deg"
            % (
                self.start_position[0],
                self.start_position[1],
                self._format_optional_value(
                    math.degrees(self.start_imu_yaw)
                    if self.start_imu_yaw is not None
                    else None,
                    "deg",
                ),
            )
        )
        self.current_run += 1

    def _compute_traveled_distance(self) -> float:
        delta_x = self.current_position[0] - self.start_position[0]
        delta_y = self.current_position[1] - self.start_position[1]
        return math.hypot(delta_x, delta_y)

    def _compute_lateral_error(self) -> float:
        delta_x = self.current_position[0] - self.start_position[0]
        delta_y = self.current_position[1] - self.start_position[1]
        heading_x = math.cos(self.start_odom_yaw)
        heading_y = math.sin(self.start_odom_yaw)
        forward_projection = delta_x * heading_x + delta_y * heading_y
        lateral_x = delta_x - forward_projection * heading_x
        lateral_y = delta_y - forward_projection * heading_y
        return math.copysign(math.hypot(lateral_x, lateral_y), lateral_y)

    def _compute_imu_yaw_change_deg(self) -> float | None:
        if self.start_imu_yaw is None or self.current_imu_yaw is None:
            return None
        return math.degrees(
            shortest_angular_distance(self.start_imu_yaw, self.current_imu_yaw)
        )

    def _apply_command_compensation(self, requested_m: float) -> float:
        if requested_m == 0.0:
            return 0.0
        return requested_m + math.copysign(self.command_compensation_m, requested_m)

    def _write_report(self) -> str:
        self.report_output_dir.mkdir(parents=True, exist_ok=True)
        safe_test_id = self._sanitize_test_id(self.test_id)
        report_path = self.report_output_dir / f"{safe_test_id}_straight_accuracy.md"
        report_path.write_text(self._build_report_markdown(), encoding="utf-8")
        return str(report_path)

    def _build_report_markdown(self) -> str:
        lines = [
            "# Straight Test Report",
            "",
            "## Summary",
            "",
            f"- Test ID: {self.test_id}",
            f"- Executed at: {self.run_started_at.strftime('%Y-%m-%d %H:%M:%S')}",
            f"- Repeat count: {self.repeat_count}",
            f"- Requested distance (m): {self.distance_m:.3f}",
            f"- Command compensation (m): {self.command_compensation_m:.3f}",
            f"- Linear speed (m/s): {self.linear_speed:.2f}",
            f"- Min linear speed (m/s): {self.min_linear_speed:.2f}",
            f"- Slowdown distance (m): {self.slowdown_distance_m:.3f}",
            f"- Distance tolerance (m): {self.distance_tolerance_m:.3f}",
            f"- Repeat delay (sec): {self.repeat_delay_sec:.1f}",
            f"- Odom topic: {self.get_parameter('odom_topic').value}",
            f"- IMU topic: {self.get_parameter('imu_topic').value}",
            "",
            "## Results",
            "",
            "| Run | Request (m) | Target (m) | Measured (m) | Error (m) | Residual (m) | Lateral Error (m) | Odom Yaw Change (deg) | IMU Yaw Change (deg) |",
            "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
        ]

        for result in self.step_results:
            lines.append(
                "| {run} | {request_distance_m:.3f} | {target_distance_m:.3f} | {measured_distance_m:.3f} | {distance_error_m:.3f} | {distance_residual_m:.3f} | {lateral_error_m:.3f} | {odom_yaw_change_deg:.2f} | {imu_yaw_change_deg} |".format(
                    run=result["run"],
                    request_distance_m=result["request_distance_m"],
                    target_distance_m=result["target_distance_m"],
                    measured_distance_m=result["measured_distance_m"],
                    distance_error_m=result["distance_error_m"],
                    distance_residual_m=result["distance_residual_m"],
                    lateral_error_m=result["lateral_error_m"],
                    odom_yaw_change_deg=result["odom_yaw_change_deg"],
                    imu_yaw_change_deg=self._format_markdown_value(
                        result["imu_yaw_change_deg"]
                    ),
                )
            )

        lines.extend(
            [
                "",
                "## Statistics",
                "",
                f"- Average distance error (m): {self._average_for_key('distance_error_m')}",
                f"- Average absolute distance error (m): {self._average_abs_for_key('distance_error_m')}",
                f"- Average lateral error (m): {self._average_for_key('lateral_error_m')}",
                f"- Average absolute lateral error (m): {self._average_abs_for_key('lateral_error_m')}",
                f"- Average odom yaw change (deg): {self._average_for_key('odom_yaw_change_deg')}",
                f"- Average imu yaw change (deg): {self._average_for_key('imu_yaw_change_deg')}",
                "",
                "## Notes",
                "",
                "- One floor block is treated as `0.45 m` for this test.",
                "- If the robot consistently stops short in real-world measurement, increase `command_compensation_m`.",
                "- Compare odom distance with floor markings or a tape measure.",
            ]
        )
        return "\n".join(lines) + "\n"

    def _average_for_key(self, key: str) -> str:
        values = [result[key] for result in self.step_results if result[key] is not None]
        if not values:
            return "n/a"
        return "%.3f" % (sum(values) / len(values))

    def _average_abs_for_key(self, key: str) -> str:
        values = [abs(result[key]) for result in self.step_results if result[key] is not None]
        if not values:
            return "n/a"
        return "%.3f" % (sum(values) / len(values))

    def _format_markdown_value(self, value: float | None) -> str:
        if value is None:
            return "n/a"
        return "%.2f" % value

    def _format_optional_value(self, value: float | None, unit: str) -> str:
        if value is None:
            return "n/a"
        return f"{value:.2f} {unit}"

    def _sanitize_test_id(self, value: str) -> str:
        sanitized = value.strip().replace(" ", "_")
        return "".join(
            char for char in sanitized if char.isalnum() or char in ("-", "_")
        ) or "T-01"

    def _log_progress(self, remaining: float) -> None:
        if self.last_progress_log_remaining_m is None:
            self.last_progress_log_remaining_m = remaining
            self.get_logger().info(
                "[PROGRESS] run=%d/%d remaining=%.3f m"
                % (
                    self.current_run - 1,
                    self.repeat_count,
                    remaining,
                )
            )
            return

        if self.last_progress_log_remaining_m - remaining < 0.10:
            return

        self.last_progress_log_remaining_m = remaining
        self.get_logger().info(
            "[PROGRESS] run=%d/%d remaining=%.3f m"
            % (
                self.current_run - 1,
                self.repeat_count,
                remaining,
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
        if rclpy.ok():
            rclpy.shutdown()
