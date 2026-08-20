from __future__ import annotations

import math
import time
from typing import Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
from rclpy.node import Node
from robot_audio_interfaces.msg import AudioCommand
from std_msgs.msg import Bool, String

from .json_topic import dump_json_message, load_json_message

# 문서 02 §7.1/SR-F-010/TC-12: 반출 감지 15초 후 복귀, 미반출 시 60초마다
# 안내, 180초 경과 시 강제 복귀. Pi에서 로컬로 완결한다 — Wi-Fi가 끊긴
# 채로도(호출구역 도착 통지까지는 이미 받은 뒤라는 전제) 반출 감지, 타이머,
# 음성 안내, Nav2 복귀 goal 전송까지 전부 이 노드 혼자 처리한다(2026-08-19,
# PC의 MissionController가 예전에 하던 걸 이전).
RETURN_DELAY_AFTER_PAYLOAD_REMOVED_SEC = 15.0
REMINDER_INTERVAL_SEC = 60.0
FORCED_RETURN_TIMEOUT_SEC = 180.0
PAYLOAD_REMOVED_CONFIRM_SEC = 0.5


def _yaw_to_quaternion(yaw: float):
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


class ArrivalReturnManager(Node):
    def __init__(self) -> None:
        super().__init__("arrival_return_manager")

        self._armed = False
        self._mission_id = ""
        self._queue_id = ""
        self._home_pose: Optional[dict] = None

        self._payload_removed = False
        self._payload_loaded_last: Optional[bool] = None
        self._payload_removed_started_at: Optional[float] = None

        self._forced_deadline: Optional[float] = None
        self._removed_deadline: Optional[float] = None
        self._next_reminder_at: Optional[float] = None

        self._goal_handle = None

        self._audio_pub = self.create_publisher(AudioCommand, "/audio/command", 10)
        self._dock_pub = self.create_publisher(Bool, "/medirun/vision/dock_request", 10)
        self._status_pub = self.create_publisher(String, "/medirun/mission/pi_return_status", 10)
        self._nav_client = ActionClient(self, NavigateToPose, "navigate_to_pose")

        self.create_subscription(String, "/medirun/mission/arrival_notice", self._handle_arrival_notice, 10)
        self.create_subscription(Bool, "/arduino/payload_loaded", self._handle_payload, 10)

        self.create_timer(0.5, self._tick)

    # ---- PC -> Pi: 도착 통지 ----
    def _handle_arrival_notice(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        if not bool(payload.get("armed", False)):
            self._disarm("PC 요청(운영자 개입 또는 이미 처리됨)")
            return

        home_pose = payload.get("home_pose")
        if not home_pose or "x" not in home_pose or "y" not in home_pose:
            self.get_logger().error("arrival_notice에 home_pose(x/y)가 없어 무시합니다: %s" % msg.data)
            return

        self._mission_id = payload.get("mission_id", "")
        self._queue_id = payload.get("queue_id", "")
        self._home_pose = home_pose

        self._payload_removed = False
        self._payload_loaded_last = None
        self._payload_removed_started_at = None

        now = time.monotonic()
        self._forced_deadline = now + FORCED_RETURN_TIMEOUT_SEC
        self._removed_deadline = None
        self._next_reminder_at = now + REMINDER_INTERVAL_SEC

        self._armed = True
        self._publish_status("armed")
        self.get_logger().info(
            "arrival_notice armed mission_id=%s queue_id=%s" % (self._mission_id, self._queue_id)
        )

    def _disarm(self, reason: str) -> None:
        if not self._armed:
            return
        self._armed = False
        self._forced_deadline = None
        self._removed_deadline = None
        self._next_reminder_at = None
        self.get_logger().info("감시 해제: %s" % reason)

    # ---- 홀센서 ----
    def _handle_payload(self, msg: Bool) -> None:
        if not self._armed or self._payload_removed:
            self._payload_loaded_last = msg.data
            return

        now = time.monotonic()
        if self._payload_loaded_last and not msg.data:
            self._payload_removed_started_at = now
        elif not self._payload_loaded_last and msg.data:
            self._payload_removed_started_at = None

        if (
            not msg.data
            and self._payload_removed_started_at is not None
            and now - self._payload_removed_started_at >= PAYLOAD_REMOVED_CONFIRM_SEC
        ):
            self._payload_removed = True
            self._forced_deadline = None
            self._removed_deadline = now + RETURN_DELAY_AFTER_PAYLOAD_REMOVED_SEC
            self._next_reminder_at = None
            self._publish_status("payload_removed")
            self.get_logger().info(
                "AED 반출 감지 — %.0f초 후 자동 복귀" % RETURN_DELAY_AFTER_PAYLOAD_REMOVED_SEC
            )

        self._payload_loaded_last = msg.data

    # ---- 타이머 ----
    def _tick(self) -> None:
        if not self._armed:
            return
        now = time.monotonic()

        if self._removed_deadline is not None and now >= self._removed_deadline:
            self._begin_return("AED 반출 감지 후 자동 복귀")
            return

        if self._forced_deadline is not None and now >= self._forced_deadline:
            self._begin_return("AED 미반출 · 3분 경과로 강제 복귀")
            return

        if not self._payload_removed and self._next_reminder_at is not None and now >= self._next_reminder_at:
            # 설계서에 이 안내의 정확한 대사가 없어 임시 문구 — 나중에 조정 가능.
            self._play_audio("AED를 반출해 주세요")
            self._next_reminder_at = now + REMINDER_INTERVAL_SEC

    # ---- 복귀 시작 ----
    def _begin_return(self, reason: str) -> None:
        self.get_logger().info("복귀 시작: %s" % reason)
        self._armed = False
        self._forced_deadline = None
        self._removed_deadline = None
        self._next_reminder_at = None
        self._play_audio("복귀합니다")
        self._publish_status("returning")
        self._send_nav_goal()

    def _send_nav_goal(self) -> None:
        if self._home_pose is None:
            self.get_logger().error("home_pose가 없어 복귀 goal을 보낼 수 없습니다.")
            self._publish_status("return_failed")
            return

        if not self._nav_client.wait_for_server(timeout_sec=2.0):
            self.get_logger().error("navigate_to_pose 액션 서버를 찾을 수 없습니다.")
            self._publish_status("return_failed")
            return

        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = self._make_pose_stamped(
            float(self._home_pose["x"]),
            float(self._home_pose["y"]),
            float(self._home_pose.get("yaw", 0.0)),
            str(self._home_pose.get("frame_id", "map")),
        )
        send_future = self._nav_client.send_goal_async(goal_msg)
        send_future.add_done_callback(self._on_goal_response)

    def _make_pose_stamped(self, x: float, y: float, yaw: float, frame_id: str) -> PoseStamped:
        pose = PoseStamped()
        pose.header.frame_id = frame_id
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = x
        pose.pose.position.y = y
        _, _, qz, qw = _yaw_to_quaternion(yaw)
        pose.pose.orientation.z = qz
        pose.pose.orientation.w = qw
        return pose

    def _on_goal_response(self, future) -> None:
        goal_handle = future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error("복귀 goal이 거부되었습니다.")
            self._publish_status("return_failed")
            return
        self._goal_handle = goal_handle
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self._on_result)

    def _on_result(self, future) -> None:
        wrapped = future.result()
        self._goal_handle = None
        if wrapped.status == GoalStatus.STATUS_SUCCEEDED:
            # 도킹 요청도 여기서 로컬로 바로 보낸다 — docking_manager도 Pi에서
            # 도니 이 역시 Wi-Fi 무관. 도킹 성공/실패 결과(/medirun/vision/
            # dock_result)는 docking_manager가 직접 방송하고, PC의
            # MissionController가 그걸 구독해 미션을 마무리한다(기존과 동일
            # 경로 재사용 — 여기서 더 할 일 없음).
            self._dock_pub.publish(Bool(data=True))
        else:
            self._play_audio("오류 발생")
            self._publish_status("return_failed")

    # ---- 공용 ----
    def _play_audio(self, text: str) -> None:
        msg = AudioCommand()
        msg.type = AudioCommand.TYPE_TTS
        msg.text = text
        msg.volume = 1.0
        msg.repeat = 1
        self._audio_pub.publish(msg)

    def _publish_status(self, phase: str) -> None:
        msg = String()
        msg.data = dump_json_message({"phase": phase, "mission_id": self._mission_id, "queue_id": self._queue_id})
        self._status_pub.publish(msg)


def main() -> None:
    rclpy.init()
    node = ArrivalReturnManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
