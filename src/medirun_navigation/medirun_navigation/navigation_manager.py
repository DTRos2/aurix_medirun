from __future__ import annotations

import math
from typing import Optional

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
from rclpy.action.client import ClientGoalHandle
from rclpy.node import Node
from rclpy.task import Future
from std_msgs.msg import Bool, String

from .json_topic import dump_json_message, load_json_message


def _yaw_to_quaternion(yaw: float):
    return (0.0, 0.0, math.sin(yaw / 2.0), math.cos(yaw / 2.0))


class NavigationManager(Node):
    def __init__(self) -> None:
        super().__init__("navigation_manager")

        self._active_goal: Optional[dict] = None
        self._goal_handle: Optional[ClientGoalHandle] = None
        self._status_pub = self.create_publisher(String, "/medirun/navigation/status", 10)
        self._nav_client = ActionClient(self, NavigateToPose, "navigate_to_pose")
        self.create_subscription(String, "/medirun/navigation/goal", self._handle_goal, 10)
        self.create_subscription(Bool, "/medirun/navigation/cancel", self._handle_cancel, 10)

    def _handle_goal(self, msg: String) -> None:
        if self._active_goal is not None:
            self.get_logger().warning(
                "이미 주행 중인 goal이 있어 새 요청을 무시합니다: %s" % msg.data
            )
            return

        payload = load_json_message(msg.data)
        route = payload.get("route") or {}
        pose = route.get("pose") or {}
        if "x" not in pose or "y" not in pose:
            self.get_logger().error("goal payload에 route.pose.x/y가 없습니다: %s" % msg.data)
            return

        self._active_goal = payload
        self._publish_status("ACCEPTED")

        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = self._make_pose_stamped(
            float(pose["x"]), float(pose["y"]), float(pose.get("yaw", 0.0)), str(route.get("frame_id", "map"))
        )

        if not self._nav_client.wait_for_server(timeout_sec=1.0):
            self.get_logger().error("navigate_to_pose 액션 서버를 찾을 수 없습니다.")
            self._finish("FAILED")
            return

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

    def _on_goal_response(self, future: Future) -> None:
        goal_handle = future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error("navigate_to_pose goal이 거부되었습니다.")
            self._finish("FAILED")
            return
        self._goal_handle = goal_handle
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self._on_result)

    def _on_result(self, future: Future) -> None:
        wrapped = future.result()
        status = wrapped.status
        if status == GoalStatus.STATUS_SUCCEEDED:
            self._finish("SUCCEEDED")
        elif status == GoalStatus.STATUS_CANCELED:
            self._finish("CANCELED")
        else:
            self._finish("FAILED")

    def _handle_cancel(self, msg: Bool) -> None:
        if not msg.data or self._goal_handle is None:
            return
        self._goal_handle.cancel_goal_async()

    def _finish(self, status: str) -> None:
        self._publish_status(status)
        self._active_goal = None
        self._goal_handle = None

    def _publish_status(self, status: str) -> None:
        msg = String()
        payload = {"status": status}
        if self._active_goal is not None:
            payload["goal_type"] = self._active_goal.get("goal_type", "")
            payload["mission_id"] = self._active_goal.get("mission_id", "")
            payload["waypoint_id"] = self._active_goal.get("waypoint_id", "")
        msg.data = dump_json_message(payload)
        self._status_pub.publish(msg)


def main() -> None:
    rclpy.init()
    node = NavigationManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
