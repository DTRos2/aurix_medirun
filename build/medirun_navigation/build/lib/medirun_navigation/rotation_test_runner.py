import math
from datetime import datetime
from pathlib import Path
from typing import List

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from sensor_msgs.msg import Imu

from .angle_math import normalize_angle, shortest_angular_distance, yaw_from_quaternion


class RotationTestRunner(Node):
    def __init__(self) -> None:
        super().__init__("rotation_test_runner")

        self.declare_parameter("odom_topic", "/odom")
        self.declare_parameter("imu_topic", "/imu")
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter("angles_deg", [90.0])
        self.declare_parameter("repeat_count", 1)
        self.declare_parameter("command_compensation_deg", 0.0)
        self.declare_parameter("angular_speed", 0.35)
        self.declare_parameter("min_angular_speed", 0.20)
        self.declare_parameter("slowdown_angle_deg", 5.0)
        self.declare_parameter("yaw_tolerance_deg", 3.0)
        self.declare_parameter("settle_time_sec", 0.0)
        self.declare_parameter("repeat_delay_sec", 3.0)
        self.declare_parameter("loop_hz", 20.0)
        self.declare_parameter("report_output_dir", "docs/tests")
        self.declare_parameter("test_id", "T-00")

        odom_topic = self.get_parameter("odom_topic").get_parameter_value().string_value
        imu_topic = self.get_parameter("imu_topic").get_parameter_value().string_value
        cmd_vel_topic = (
            self.get_parameter("cmd_vel_topic").get_parameter_value().string_value
        )
        self.angle_sequence_deg = self._read_angle_sequence()
        self.repeat_count = (
            self.get_parameter("repeat_count").get_parameter_value().integer_value
        )
        self.command_compensation_deg = (
            self.get_parameter("command_compensation_deg")
            .get_parameter_value()
            .double_value
        )
        self.angular_speed = (
            self.get_parameter("angular_speed").get_parameter_value().double_value
        )
        self.min_angular_speed = (
            self.get_parameter("min_angular_speed").get_parameter_value().double_value
        )
        self.slowdown_angle = math.radians(
            self.get_parameter("slowdown_angle_deg")
            .get_parameter_value()
            .double_value
        )
        self.yaw_tolerance = math.radians(
            self.get_parameter("yaw_tolerance_deg")
            .get_parameter_value()
            .double_value
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

        self.current_odom_yaw = None
        self.current_imu_yaw = None
        self.active_target_yaw = None
        self.current_step = 0
        self.current_run = 1
        self.state = "wait_for_odom"
        self.state_deadline = None
        self.step_start_odom_yaw = None
        self.step_start_imu_yaw = None
        self.step_command_deg = None
        self.step_target_command_deg = None
        self.completed = False
        self.last_progress_log_error_deg = None
        self.run_started_at = datetime.now()
        self.report_output_dir = Path(report_output_dir)
        self.step_results = []

        if self.repeat_count < 1:
            raise ValueError("repeat_count must be at least 1.")
        if self.angular_speed <= 0.0:
            raise ValueError("angular_speed must be greater than 0.")
        if self.min_angular_speed <= 0.0:
            raise ValueError("min_angular_speed must be greater than 0.")
        if self.min_angular_speed > self.angular_speed:
            raise ValueError("min_angular_speed must be <= angular_speed.")
        if self.slowdown_angle <= 0.0:
            raise ValueError("slowdown_angle_deg must be greater than 0.")
        if self.repeat_delay_sec < 0.0:
            raise ValueError("repeat_delay_sec must be >= 0.")

        self.get_logger().info(
            "[READY] angles=%s deg, repeat_count=%d, compensation=%.2f deg, speed=%.2f rad/s, min_speed=%.2f rad/s, repeat_delay=%.1f sec, odom_topic=%s, imu_topic=%s"
            % (
                self.angle_sequence_deg,
                self.repeat_count,
                self.command_compensation_deg,
                self.angular_speed,
                self.min_angular_speed,
                self.repeat_delay_sec,
                odom_topic,
                imu_topic,
            )
        )

    def _read_angle_sequence(self) -> List[float]:
        values = self.get_parameter("angles_deg").get_parameter_value().double_array_value
        if not values:
            raise ValueError("angles_deg must contain at least one angle.")
        return [float(value) for value in values]

    def _odom_callback(self, msg: Odometry) -> None:
        self.current_odom_yaw = yaw_from_quaternion(msg.pose.pose.orientation)

    def _imu_callback(self, msg: Imu) -> None:
        self.current_imu_yaw = yaw_from_quaternion(msg.orientation)

    def _on_timer(self) -> None:
        if self.completed:
            return

        if self.current_odom_yaw is None:
            return

        now = self.get_clock().now().nanoseconds / 1e9
        if self.state == "wait_for_odom":
            self._start_next_step()
            return

        if self.state == "rotating":
            error = shortest_angular_distance(self.current_odom_yaw, self.active_target_yaw)
            if abs(error) <= self.yaw_tolerance:
                self._publish_stop()
                # Fine stop verification disabled for coarse-error testing.
                # self.state = "verifying"
                # self.state_deadline = now + self.settle_time_sec
                self._complete_step_if_stable()
                return

            self._log_rotation_progress(error)
            self._publish_rotation_command(error)
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

    def _publish_rotation_command(self, error: float) -> None:
        twist = Twist()
        commanded_speed = self.angular_speed
        # Fine slowdown near target disabled to intentionally allow larger stop error.
        # if abs(error) < self.slowdown_angle:
        #     commanded_speed = self.angular_speed * (abs(error) / self.slowdown_angle)
        commanded_speed = max(self.min_angular_speed, min(self.angular_speed, commanded_speed))
        twist.angular.z = math.copysign(commanded_speed, error)
        self.cmd_pub.publish(twist)

    def _complete_step_if_stable(self) -> None:
        error = shortest_angular_distance(self.current_odom_yaw, self.active_target_yaw)
        # Fine re-check loop disabled for coarse-error testing.
        # if abs(error) > self.yaw_tolerance:
        #     self.state = "rotating"
        #     return

        odom_measured_deg = math.degrees(
            shortest_angular_distance(self.step_start_odom_yaw, self.current_odom_yaw)
        )
        odom_error_deg = odom_measured_deg - self.step_command_deg
        imu_measured_deg, imu_error_deg = self._compute_imu_metrics()
        self.step_results.append(
            {
                "run": self.current_run,
                "step": self.current_step,
                "request_deg": self.step_command_deg,
                "target_deg": self.step_target_command_deg,
                "start_odom_deg": math.degrees(self.step_start_odom_yaw),
                "end_odom_deg": math.degrees(self.current_odom_yaw),
                "odom_measured_deg": odom_measured_deg,
                "odom_error_deg": odom_error_deg,
                "odom_residual_deg": math.degrees(error),
                "start_imu_deg": (
                    math.degrees(self.step_start_imu_yaw)
                    if self.step_start_imu_yaw is not None
                    else None
                ),
                "end_imu_deg": (
                    math.degrees(self.current_imu_yaw)
                    if self.current_imu_yaw is not None
                    else None
                ),
                "imu_measured_deg": imu_measured_deg,
                "imu_error_deg": imu_error_deg,
            }
        )
        self.get_logger().info(
            "[RESULT] run=%d/%d step=%d request=%.1f deg, target=%.1f deg"
            % (
                self.current_run,
                self.repeat_count,
                self.current_step,
                self.step_command_deg,
                self.step_target_command_deg,
            )
        )
        self.get_logger().info(
            "[RESULT] odom: measured=%.2f deg, error=%.2f deg, residual=%.2f deg"
            % (
                odom_measured_deg,
                odom_error_deg,
                math.degrees(error),
            )
        )
        self.get_logger().info(
            "[RESULT] imu : measured=%s deg, error=%s deg"
            % (
                self._format_optional_deg(imu_measured_deg),
                self._format_optional_deg(imu_error_deg),
            )
        )
        self.get_logger().info("[RESULT] cmd_vel.angular.z=0.0")

        delay_sec = self.settle_time_sec
        if (
            self.current_step >= len(self.angle_sequence_deg)
            and self.current_run < self.repeat_count
        ):
            delay_sec = self.repeat_delay_sec

        self.state = "settling"
        self.state_deadline = self.get_clock().now().nanoseconds / 1e9 + delay_sec

    def _start_next_step(self) -> None:
        if self.current_step >= len(self.angle_sequence_deg):
            if self.current_run >= self.repeat_count:
                self._publish_stop()
                self.completed = True
                self.state = "done"
                report_path = self._write_report()
                self.get_logger().info(
                    "[DONE] rotation test complete: %d/%d runs finished."
                    % (self.current_run, self.repeat_count)
                )
                self.get_logger().info("[DONE] report saved: %s" % report_path)
                return

            self.current_run += 1
            self.current_step = 0
            self.get_logger().info(
                "[WAIT] next run starts after %.1f sec: %d/%d"
                % (self.repeat_delay_sec, self.current_run, self.repeat_count)
            )

        self.step_command_deg = self.angle_sequence_deg[self.current_step]
        self.step_target_command_deg = self._apply_command_compensation(
            self.step_command_deg
        )
        delta_yaw = math.radians(self.step_target_command_deg)
        self.step_start_odom_yaw = self.current_odom_yaw
        self.step_start_imu_yaw = self.current_imu_yaw
        self.active_target_yaw = normalize_angle(self.current_odom_yaw + delta_yaw)
        self.current_step += 1
        self.state = "rotating"
        self.last_progress_log_error_deg = None
        self.get_logger().info(
            "[START] run=%d/%d step=%d request=%.1f deg, target=%.1f deg"
            % (
                self.current_run,
                self.repeat_count,
                self.current_step,
                self.step_command_deg,
                self.step_target_command_deg,
            )
        )
        self.get_logger().info(
            "[START] odom_yaw=%.2f deg, imu_yaw=%s deg"
            % (
                math.degrees(self.step_start_odom_yaw),
                self._format_optional_deg(
                    math.degrees(self.step_start_imu_yaw)
                    if self.step_start_imu_yaw is not None
                    else None
                ),
            )
        )

    def _compute_imu_metrics(self) -> tuple[float | None, float | None]:
        if self.step_start_imu_yaw is None or self.current_imu_yaw is None:
            return None, None

        imu_measured_deg = math.degrees(
            shortest_angular_distance(self.step_start_imu_yaw, self.current_imu_yaw)
        )
        imu_error_deg = imu_measured_deg - self.step_command_deg
        return imu_measured_deg, imu_error_deg

    def _format_optional_deg(self, value: float | None) -> str:
        if value is None:
            return "n/a"
        return "%.2f" % value

    def _apply_command_compensation(self, requested_deg: float) -> float:
        if requested_deg == 0.0:
            return 0.0
        return requested_deg + math.copysign(
            self.command_compensation_deg, requested_deg
        )

    def _write_report(self) -> str:
        self.report_output_dir.mkdir(parents=True, exist_ok=True)
        safe_test_id = self._sanitize_test_id(self.test_id)
        report_path = self.report_output_dir / f"{safe_test_id}_rotation_accuracy.md"
        report_path.write_text(self._build_report_markdown(), encoding="utf-8")
        return str(report_path)

    def _build_report_markdown(self) -> str:
        lines = [
            "# Rotation Test Report",
            "",
            "## Summary",
            "",
            f"- Test ID: {self.test_id}",
            f"- Executed at: {self.run_started_at.strftime('%Y-%m-%d %H:%M:%S')}",
            f"- Repeat count: {self.repeat_count}",
            f"- Requested angles (deg): {self.angle_sequence_deg}",
            f"- Command compensation (deg): {self.command_compensation_deg:.2f}",
            f"- Angular speed (rad/s): {self.angular_speed:.2f}",
            f"- Min angular speed (rad/s): {self.min_angular_speed:.2f}",
            f"- Repeat delay (sec): {self.repeat_delay_sec:.1f}",
            f"- Odom topic: {self.get_parameter('odom_topic').value}",
            f"- IMU topic: {self.get_parameter('imu_topic').value}",
            "",
            "## Results",
            "",
            "| Run | Step | Request (deg) | Target (deg) | Odom Measured (deg) | Odom Error (deg) | Odom Residual (deg) | IMU Measured (deg) | IMU Error (deg) |",
            "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
        ]

        for result in self.step_results:
            lines.append(
                "| {run} | {step} | {request_deg:.2f} | {target_deg:.2f} | {odom_measured_deg:.2f} | {odom_error_deg:.2f} | {odom_residual_deg:.2f} | {imu_measured_deg} | {imu_error_deg} |".format(
                    run=result["run"],
                    step=result["step"],
                    request_deg=result["request_deg"],
                    target_deg=result["target_deg"],
                    odom_measured_deg=result["odom_measured_deg"],
                    odom_error_deg=result["odom_error_deg"],
                    odom_residual_deg=result["odom_residual_deg"],
                    imu_measured_deg=self._format_markdown_value(
                        result["imu_measured_deg"]
                    ),
                    imu_error_deg=self._format_markdown_value(result["imu_error_deg"]),
                )
            )

        lines.extend(
            [
                "",
                "## Statistics",
                "",
                f"- Average odom error (deg): {self._average_for_key('odom_error_deg')}",
                f"- Average absolute odom error (deg): {self._average_abs_for_key('odom_error_deg')}",
                f"- Average imu error (deg): {self._average_for_key('imu_error_deg')}",
                f"- Average absolute imu error (deg): {self._average_abs_for_key('imu_error_deg')}",
                "",
                "## Notes",
                "",
                "- If the robot under-rotates in real-world measurement, increase `command_compensation_deg`.",
                "- Compare this report with manual protractor or floor marker measurements.",
            ]
        )
        return "\n".join(lines) + "\n"

    def _average_for_key(self, key: str) -> str:
        values = [result[key] for result in self.step_results if result[key] is not None]
        if not values:
            return "n/a"
        return "%.2f" % (sum(values) / len(values))

    def _average_abs_for_key(self, key: str) -> str:
        values = [abs(result[key]) for result in self.step_results if result[key] is not None]
        if not values:
            return "n/a"
        return "%.2f" % (sum(values) / len(values))

    def _format_markdown_value(self, value: float | None) -> str:
        if value is None:
            return "n/a"
        return "%.2f" % value

    def _sanitize_test_id(self, value: str) -> str:
        sanitized = value.strip().replace(" ", "_")
        return "".join(
            char for char in sanitized if char.isalnum() or char in ("-", "_")
        ) or "T-00"

    def _log_rotation_progress(self, error: float) -> None:
        error_deg = abs(math.degrees(error))
        if self.last_progress_log_error_deg is None:
            self.last_progress_log_error_deg = error_deg
            self.get_logger().info(
                "[PROGRESS] run=%d/%d step=%d remaining=%.2f deg"
                % (
                    self.current_run,
                    self.repeat_count,
                    self.current_step,
                    error_deg,
                )
            )
            return

        if self.last_progress_log_error_deg - error_deg < 15.0:
            return

        self.last_progress_log_error_deg = error_deg
        self.get_logger().info(
            "[PROGRESS] run=%d/%d step=%d remaining=%.2f deg"
            % (
                self.current_run,
                self.repeat_count,
                self.current_step,
                error_deg,
            )
        )

    def _publish_stop(self) -> None:
        if not rclpy.ok():
            return
        self.cmd_pub.publish(Twist())


def main() -> None:
    rclpy.init()
    node = RotationTestRunner()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node._publish_stop()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
