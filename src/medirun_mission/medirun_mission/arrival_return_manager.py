from __future__ import annotations

import json
import math
import time
from pathlib import Path
from typing import Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateThroughPoses, NavigateToPose
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

# pi_return_status를 아무도(PC) 못 듣고 있을 때 유실되지 않도록 임시로
# 남겨두는 로컬 버퍼 — event_logger.py의 JSONL append 관례를 차용했다.
# PC가 다시 붙으면(구독자 수 0->양수 전이) 순서대로 재전송하고 비운다.
PENDING_EVENTS_PATH = Path.home() / ".medirun_pi" / "pending_mission_events.jsonl"


def _yaw_to_quaternion(yaw: float):
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


class ArrivalReturnManager(Node):
    def __init__(self) -> None:
        super().__init__("arrival_return_manager")

        self._armed = False
        self._mission_id = ""
        self._queue_id = ""
        self._home_pose: Optional[dict] = None
        # 도착 통지에 같이 실려온, 이 미션을 유발한 버튼의 trajectory(저장된
        # 순서 그대로의 {x,y,yaw} 리스트) — 비어있으면 기존처럼 HOME 단일
        # 지점으로 복귀한다.
        self._trajectory_poses: list = []

        self._payload_removed = False
        self._payload_loaded_last: Optional[bool] = None
        self._payload_removed_started_at: Optional[float] = None

        self._forced_deadline: Optional[float] = None
        self._removed_deadline: Optional[float] = None
        self._next_reminder_at: Optional[float] = None

        self._goal_handle = None
        # 직전 tick에서 본 pi_return_status 구독자 수 — 0에서 양수로 바뀌는
        # 순간(PC 재연결)을 잡아내는 용도.
        self._last_subscriber_count = 0
        # ultrasonic_safety_stop이 방송하는 정지 상태의 직전 값 — False->True로
        # 바뀌는 순간(정지 시작)에만 1회 안내한다(계속 정지해 있는 동안 반복
        # 재생하지 않음).
        self._obstacle_stop_active = False

        self._audio_pub = self.create_publisher(AudioCommand, "/audio/command", 10)
        self._dock_pub = self.create_publisher(Bool, "/medirun/vision/dock_request", 10)
        self._status_pub = self.create_publisher(String, "/medirun/mission/pi_return_status", 10)
        self._nav_client = ActionClient(self, NavigateToPose, "navigate_to_pose")
        self._through_poses_client = ActionClient(self, NavigateThroughPoses, "navigate_through_poses")

        self.create_subscription(String, "/medirun/mission/arrival_notice", self._handle_arrival_notice, 10)
        self.create_subscription(Bool, "/arduino/payload_loaded", self._handle_payload, 10)
        self.create_subscription(Bool, "/medirun/safety/obstacle_stop", self._handle_obstacle_stop, 10)

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
        # PC가 이 시점(Wi-Fi가 아직 살아있을 때)에 미리 resolve해서 통째로
        # 실어보낸 경로 — 나중에 복귀할 때 DB 재조회 없이 이걸 그대로 쓴다.
        self._trajectory_poses = [
            step for step in payload.get("trajectory", []) if "x" in step and "y" in step
        ]

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

    # ---- 초음파 안전정지(ultrasonic_safety_stop) ----
    def _handle_obstacle_stop(self, msg: Bool) -> None:
        active = bool(msg.data)
        # 복귀 goal이 실제로 나가있는 동안(self._goal_handle) 정지가 막
        # 시작된 순간에만 1회 안내한다. 출동 구간은 PC의 반복 경고가 이미
        # 담당하므로 여기서는 복귀 중에만 반응한다.
        if active and not self._obstacle_stop_active and self._goal_handle is not None:
            self._play_audio("비켜주세요")
        self._obstacle_stop_active = active

    # ---- 타이머 ----
    def _tick(self) -> None:
        # armed 여부와 무관하게 매 tick 확인 — 미션이 이미 끝난 뒤에도 PC가
        # 계속 오프라인이었다면 그 마지막 이벤트들이 버퍼에 남아있을 수 있다.
        self._check_reconnect_and_replay()

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
        self._play_audio("홈으로 복귀합니다")
        self._publish_status("returning")
        self._send_nav_goal()

    def _send_nav_goal(self) -> None:
        if self._home_pose is None:
            self.get_logger().error("home_pose가 없어 복귀 goal을 보낼 수 없습니다.")
            self._publish_status("return_failed")
            return

        frame_id = str(self._home_pose.get("frame_id", "map"))
        home_stamped = self._make_pose_stamped(
            float(self._home_pose["x"]),
            float(self._home_pose["y"]),
            float(self._home_pose.get("yaw", 0.0)),
            frame_id,
        )

        if self._trajectory_poses:
            # trajectory를 저장된 순서 그대로 탄 뒤 HOME을 이어붙여, 중간 정지
            # 없이 한 번의 navigate_through_poses로 복귀한다.
            if not self._through_poses_client.wait_for_server(timeout_sec=2.0):
                self.get_logger().error("navigate_through_poses 액션 서버를 찾을 수 없습니다.")
                self._publish_status("return_failed")
                return
            goal_msg = NavigateThroughPoses.Goal()
            goal_msg.poses = [
                self._make_pose_stamped(float(step["x"]), float(step["y"]), float(step.get("yaw", 0.0)), frame_id)
                for step in self._trajectory_poses
            ]
            goal_msg.poses.append(home_stamped)
            poses_total = len(goal_msg.poses)

            def _on_feedback(feedback_msg, _total=poses_total) -> None:
                remaining = int(feedback_msg.feedback.number_of_poses_remaining)
                passed = max(0, _total - remaining)
                self._publish_status("returning", waypoint_index=passed, poses_total=_total)

            send_future = self._through_poses_client.send_goal_async(goal_msg, feedback_callback=_on_feedback)
        else:
            if not self._nav_client.wait_for_server(timeout_sec=2.0):
                self.get_logger().error("navigate_to_pose 액션 서버를 찾을 수 없습니다.")
                self._publish_status("return_failed")
                return
            goal_msg = NavigateToPose.Goal()
            goal_msg.pose = home_stamped
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
            self._play_audio("복귀했습니다")
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

    def _publish_status(self, phase: str, waypoint_index: int = -1, poses_total: int = -1) -> None:
        payload = {"phase": phase, "mission_id": self._mission_id, "queue_id": self._queue_id}
        if waypoint_index >= 0:
            payload["waypoint_index"] = waypoint_index
            payload["poses_total"] = poses_total
        # PC가 지금 듣고 있는지와 무관하게 먼저 로컬에 남긴다 — 구독자가 없어
        # DDS가 그냥 버리는 경우(Wi-Fi 단절)에도 유실되지 않게 하기 위함.
        # 재연결되면 _check_reconnect_and_replay()가 순서대로 다시 보낸다.
        self._append_pending_event(payload)
        msg = String()
        msg.data = dump_json_message(payload)
        self._status_pub.publish(msg)

    def _append_pending_event(self, payload: dict) -> None:
        try:
            PENDING_EVENTS_PATH.parent.mkdir(parents=True, exist_ok=True)
            with PENDING_EVENTS_PATH.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(payload, ensure_ascii=True) + "\n")
        except OSError as exc:
            self.get_logger().error("진행상황 로컬 버퍼 기록 실패: %s" % exc)

    def _check_reconnect_and_replay(self) -> None:
        count = self._status_pub.get_subscription_count()
        reconnected = count > 0 and self._last_subscriber_count == 0
        self._last_subscriber_count = count
        if not reconnected:
            return
        self._replay_pending_events()

    def _replay_pending_events(self) -> None:
        if not PENDING_EVENTS_PATH.exists():
            return
        try:
            lines = PENDING_EVENTS_PATH.read_text(encoding="utf-8").splitlines()
        except OSError as exc:
            self.get_logger().error("진행상황 로컬 버퍼 읽기 실패: %s" % exc)
            return
        if not lines:
            return

        self.get_logger().info("PC 재연결 감지 — 버퍼된 진행상황 %d건 재전송합니다." % len(lines))
        for line in lines:
            if not line.strip():
                continue
            try:
                payload = json.loads(line)
            except ValueError:
                continue
            payload["replayed"] = True
            msg = String()
            msg.data = dump_json_message(payload)
            self._status_pub.publish(msg)

        # 최선노력(best-effort) 재전송이다 — PC가 실제로 받았다는 ack는 없지만,
        # 재전송 자체가 구독자가 막 나타난 시점에 이뤄지므로 통상적으로는
        # 안전하다. 다시 쌓이지 않도록 비운다(다음 실패는 새로 버퍼링됨).
        try:
            PENDING_EVENTS_PATH.unlink()
        except OSError:
            pass


def main() -> None:
    rclpy.init()
    node = ArrivalReturnManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
