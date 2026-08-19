from __future__ import annotations

import json
import time
from typing import Optional

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from std_msgs.msg import Bool, String

from medirun_msgs.msg import ArucoMarker

# 마커가 짧게(STALE_GRACE_SEC 이내) 안 보여도 필터된 마지막 오차값으로 저속
# 주행을 유지한다. 그보다 오래(marker_timeout_sec) 안 보이면 즉시 정지하고
# SEARCHING_MARKER로 되돌아간다 — 트레일러가 붙어 있어 오추정 상태로 계속
# 움직이는 것보다 멈추는 쪽이 안전하다.
STALE_GRACE_SEC = 0.3
MAX_SEARCH_ATTEMPTS = 3


def _clamp(value: float, limit: float) -> float:
    return max(-limit, min(limit, value))


def _apply_min_rotation(angular: float, min_speed: float, max_speed: float) -> float:
    """제자리 회전은 정지마찰 때문에 너무 느리게 명령하면 헛돈다(트레일러
    포함 차체 정지마찰 실측 기준 min_speed 이하로는 거의 안 움직임). 회전
    방향이 있으면 최소 min_speed는 보장하고 max_speed를 넘지 않게 한다."""
    if angular == 0.0:
        return 0.0
    sign = 1.0 if angular > 0.0 else -1.0
    return sign * min(max(abs(angular), min_speed), max_speed)


class DockingManager(Node):
    def __init__(self) -> None:
        super().__init__("docking_manager")

        self.declare_parameter("simulate_docking", True)
        self.declare_parameter("simulated_docking_sec", 4.0)

        self.declare_parameter("lateral_tolerance", 0.03)
        self.declare_parameter("yaw_tolerance", 0.05)
        self.declare_parameter("yaw_align_threshold", 0.15)
        self.declare_parameter("max_angular_velocity", 0.8)
        self.declare_parameter("max_angular_velocity_moving", 0.15)
        # 제자리 회전(SEARCHING_MARKER 탐색 스윕, ALIGNING 정렬)이 정지마찰을
        # 못 이기고 헛도는 문제 완화용 최소 회전 속도. 실측 결과 0.5도
        # 힘이 부족해 0.7로 올림(2026-08-19).
        self.declare_parameter("min_rotation_speed", 0.7)
        self.declare_parameter("search_angular_velocity", 0.5)
        self.declare_parameter("search_timeout_sec", 20.0)
        self.declare_parameter("search_direction_switch_pause_sec", 0.25)
        self.declare_parameter("linear_speed_min", 0.03)
        self.declare_parameter("linear_speed_max", 0.07)
        self.declare_parameter("final_adjustment_distance", 0.3)
        self.declare_parameter("target_distance", 0.15)
        self.declare_parameter("marker_timeout_sec", 1.5)
        self.declare_parameter("pre_dock_settle_sec", 0.5)
        self.declare_parameter("kp_yaw", 0.6)
        self.declare_parameter("error_filter_alpha", 0.3)

        self._active_since: Optional[float] = None  # simulate_docking 스텁용

        self._state = "IDLE"
        self._state_entered_time = 0.0
        self._search_attempts = 0
        self._search_direction = 1.0

        self._latest_marker: Optional[ArucoMarker] = None
        self._last_detected_time: Optional[float] = None
        self._filtered_lateral = 0.0
        self._filtered_yaw = 0.0

        self._result_pub = self.create_publisher(String, "/medirun/vision/dock_result", 10)
        self._state_pub = self.create_publisher(String, "/medirun/parking/state", 10)
        self._cmd_pub = self.create_publisher(Twist, "/medirun/parking/cmd_vel_raw", 10)

        self.create_subscription(Bool, "/medirun/vision/dock_request", self._handle_request, 10)
        self.create_subscription(ArucoMarker, "/aruco/marker_pose", self._handle_marker, 10)

        self.create_timer(0.1, self._tick)

    def _handle_request(self, msg: Bool) -> None:
        if not msg.data:
            self._cancel()
            return
        if bool(self.get_parameter("simulate_docking").value):
            self._active_since = time.monotonic()
            return
        self._search_attempts = 0
        self._enter_state("SEARCHING_MARKER", reset_settle=True)

    def _cancel(self) -> None:
        if bool(self.get_parameter("simulate_docking").value):
            self._active_since = None
            return
        if self._state in ("IDLE", "PARKED", "FAILED"):
            return
        self._publish_cmd(0.0, 0.0)
        self._state = "FAILED"
        self._publish_state()
        self._publish_result("FAILED", "cancelled_by_operator")

    def _handle_marker(self, msg: ArucoMarker) -> None:
        self._latest_marker = msg
        if not msg.detected:
            return
        now = time.monotonic()
        alpha = float(self.get_parameter("error_filter_alpha").value)
        if self._last_detected_time is None:
            self._filtered_lateral = msg.lateral_error
            self._filtered_yaw = msg.yaw_error
        else:
            self._filtered_lateral = alpha * msg.lateral_error + (1.0 - alpha) * self._filtered_lateral
            self._filtered_yaw = alpha * msg.yaw_error + (1.0 - alpha) * self._filtered_yaw
        self._last_detected_time = now

    def _tick(self) -> None:
        if bool(self.get_parameter("simulate_docking").value):
            self._tick_simulated()
            return
        if self._state in ("IDLE", "PARKED", "FAILED"):
            return

        handler = {
            "SEARCHING_MARKER": self._tick_searching,
            "ALIGNING": self._tick_aligning,
            "FORWARD_PARKING": self._tick_driving,
            "FINAL_ADJUSTMENT": self._tick_driving,
        }.get(self._state)
        if handler is not None:
            handler()

    def _tick_simulated(self) -> None:
        if self._active_since is None:
            return
        if time.monotonic() - self._active_since < float(self.get_parameter("simulated_docking_sec").value):
            return
        self._publish_result("SUCCEEDED")
        self._active_since = None

    def _marker_age(self) -> Optional[float]:
        if self._last_detected_time is None:
            return None
        return time.monotonic() - self._last_detected_time

    def _handle_marker_loss_if_stale(self) -> bool:
        """마커 유실이 marker_timeout_sec를 넘기면 정지 후 재탐색으로 되돌린다."""
        age = self._marker_age()
        timeout = float(self.get_parameter("marker_timeout_sec").value)
        if age is not None and age <= timeout:
            return False
        self._publish_cmd(0.0, 0.0)
        self._search_attempts += 1
        if self._search_attempts > MAX_SEARCH_ATTEMPTS:
            self._fail("marker_lost")
        else:
            self._enter_state("SEARCHING_MARKER", reset_settle=False)
        return True

    def _tick_searching(self) -> None:
        elapsed = time.monotonic() - self._state_entered_time
        settle = float(self.get_parameter("pre_dock_settle_sec").value)
        if elapsed < settle:
            self._publish_cmd(0.0, 0.0)
            return

        age = self._marker_age()
        timeout = float(self.get_parameter("marker_timeout_sec").value)
        if age is not None and age <= timeout:
            self._enter_state("ALIGNING", reset_settle=False)
            return

        search_elapsed = elapsed - settle
        if search_elapsed > float(self.get_parameter("search_timeout_sec").value):
            self._fail("marker_not_found")
            return

        self._sweep_search(search_elapsed)

    def _sweep_search(self, search_elapsed: float) -> None:
        # 좌우로 sweep_period(3초)씩 돌고, 방향을 바꾸기 전 pause_sec만큼
        # 완전히 멈췄다가 반대로 돈다 — 멈춤 없이 바로 반대 방향 최소속도로
        # 튀면 트레일러에 급격한 저크가 생겨서 "갑자기 다른 방향으로 확
        # 도는" 것처럼 보인다(2026-08-19).
        pause_sec = float(self.get_parameter("search_direction_switch_pause_sec").value)
        sweep_period = 3.0
        phase = search_elapsed % (sweep_period + pause_sec)
        if phase >= sweep_period:
            self._publish_cmd(0.0, 0.0)
            return
        if phase < 1e-3:
            self._search_direction *= -1.0
        min_angular = float(self.get_parameter("min_rotation_speed").value)
        max_angular = float(self.get_parameter("max_angular_velocity").value)
        angular = self._search_direction * float(self.get_parameter("search_angular_velocity").value)
        angular = _apply_min_rotation(angular, min_angular, max_angular)
        self._publish_cmd(0.0, angular)

    def _in_place_align_cmd(self, yaw: float) -> float:
        """전후진을 섞지 않는 순수 제자리 회전 각속도. 최소속도를 보장해
        정지마찰로 헛도는 걸 막는다(2026-08-19: 전후진 섞는 호 그리기 방식은
        이동 반경이 커져 마커를 자주 놓치는 문제가 있어 원복)."""
        min_angular = float(self.get_parameter("min_rotation_speed").value)
        max_angular = float(self.get_parameter("max_angular_velocity").value)
        kp_yaw = float(self.get_parameter("kp_yaw").value)
        angular = _clamp(kp_yaw * yaw, max_angular)
        return _apply_min_rotation(angular, min_angular, max_angular)

    def _tick_aligning(self) -> None:
        if self._handle_marker_loss_if_stale():
            return
        yaw = self._filtered_yaw
        if abs(yaw) <= float(self.get_parameter("yaw_align_threshold").value):
            self._enter_state("FORWARD_PARKING", reset_settle=False)
            return
        self._publish_cmd(0.0, self._in_place_align_cmd(yaw))

    def _tick_driving(self) -> None:
        if self._handle_marker_loss_if_stale():
            return

        marker = self._latest_marker
        distance = marker.distance if marker is not None else 0.0
        lateral = self._filtered_lateral
        yaw = self._filtered_yaw

        final_distance = float(self.get_parameter("final_adjustment_distance").value)
        if self._state == "FORWARD_PARKING" and distance <= final_distance:
            self._enter_state("FINAL_ADJUSTMENT", reset_settle=False)

        target_distance = float(self.get_parameter("target_distance").value)
        lateral_tol = float(self.get_parameter("lateral_tolerance").value)
        yaw_tol = float(self.get_parameter("yaw_tolerance").value)
        if (
            self._state == "FINAL_ADJUSTMENT"
            and distance <= target_distance
            and abs(lateral) <= lateral_tol
            and abs(yaw) <= yaw_tol
        ):
            self._publish_cmd(0.0, 0.0)
            self._state = "PARKED"
            self._publish_state()
            self._publish_result("SUCCEEDED")
            return

        max_angular_moving = float(self.get_parameter("max_angular_velocity_moving").value)
        yaw_align_threshold = float(self.get_parameter("yaw_align_threshold").value)
        kp_yaw = float(self.get_parameter("kp_yaw").value)
        if abs(yaw) > yaw_align_threshold:
            # 트레일러 킹핀 구조상 전진 중 큰 각도 보정은 위험하므로 먼저
            # 멈추고 정렬한다 — 이 순간엔 선속도가 0이라 ALIGNING과 같은
            # 순수 제자리 회전으로 맞춘다.
            self._publish_cmd(0.0, self._in_place_align_cmd(yaw))
            return

        speed_min = float(self.get_parameter("linear_speed_min").value)
        speed_max = float(self.get_parameter("linear_speed_max").value)
        linear = speed_min if self._state == "FINAL_ADJUSTMENT" else speed_max
        linear = max(speed_min, min(speed_max, linear))

        age = self._marker_age() or 0.0
        if age > STALE_GRACE_SEC:
            linear *= 0.5

        self._publish_cmd(linear, _clamp(kp_yaw * yaw, max_angular_moving))

    def _enter_state(self, state: str, reset_settle: bool) -> None:
        self._state = state
        self._state_entered_time = time.monotonic()
        if reset_settle is False and state == "SEARCHING_MARKER":
            self._state_entered_time -= float(self.get_parameter("pre_dock_settle_sec").value)
        self._publish_state()

    def _fail(self, reason: str) -> None:
        self._publish_cmd(0.0, 0.0)
        self._state = "FAILED"
        self._publish_state()
        self._publish_result("FAILED", reason)

    def _publish_cmd(self, linear_x: float, angular_z: float) -> None:
        twist = Twist()
        twist.linear.x = linear_x
        twist.angular.z = angular_z
        self._cmd_pub.publish(twist)

    def _publish_state(self) -> None:
        msg = String()
        msg.data = self._state
        self._state_pub.publish(msg)

    def _publish_result(self, status: str, reason: str = "") -> None:
        out = String()
        payload = {"status": status}
        if reason:
            payload["reason"] = reason
        out.data = json.dumps(payload, ensure_ascii=True, sort_keys=True)
        self._result_pub.publish(out)


def main() -> None:
    rclpy.init()
    node = DockingManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
