from __future__ import annotations

import time
from typing import Any, Dict, Optional

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, String

from .json_topic import dump_json_message, load_json_message


class MissionManager(Node):
    def __init__(self) -> None:
        super().__init__("mission_manager")
        self.declare_parameter("return_delay_after_payload_removed_sec", 15.0)
        self.declare_parameter("arrival_timeout_sec", 180.0)
        self.declare_parameter("home_waypoint_id", "HOME")
        self.declare_parameter("docking_waypoint_id", "DOCK")
        self.declare_parameter("simulate_navigation", True)

        self._mission: Dict[str, Any] = {}
        self._state = "IDLE"
        self._arrival_deadline_sec: Optional[float] = None
        self._payload_loaded = True
        self._nav_goal_pub = self.create_publisher(String, "/medirun/navigation/goal", 10)
        self._nav_cancel_pub = self.create_publisher(Bool, "/medirun/navigation/cancel", 10)
        self._state_pub = self.create_publisher(String, "/medirun/mission/state", 10)
        self._dock_pub = self.create_publisher(Bool, "/medirun/vision/dock_request", 10)

        self.create_subscription(String, "/medirun/mission/dispatch_command", self._start_mission, 10)
        self.create_subscription(String, "/medirun/navigation/status", self._handle_navigation_status, 10)
        self.create_subscription(String, "/medirun/mission/payload_event", self._handle_payload_event, 10)
        self.create_subscription(String, "/medirun/vision/dock_result", self._handle_dock_result, 10)
        self.create_subscription(String, "/medirun/ui/mission_command", self._handle_operator_command, 10)
        self.create_timer(0.5, self._check_arrival_timeout)
        self._publish_state()

    def _start_mission(self, msg: String) -> None:
        if self._state not in {"IDLE", "COMPLETED", "FAILED"}:
            self.get_logger().warning("new dispatch ignored while state=%s" % self._state)
            return
        self._mission = load_json_message(msg.data)
        self._transition("DISPATCHED")
        self._send_nav_goal(self._mission["route"]["target_waypoint_id"], "service_target")
        self._transition("NAVIGATING")

    def _handle_navigation_status(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        status = payload.get("status")
        goal_type = payload.get("goal_type")
        if status != "SUCCEEDED":
            if status == "FAILED":
                self._transition("FAILED")
            return
        if goal_type == "service_target" and self._state == "NAVIGATING":
            self._transition("ARRIVED")
            self._arrival_deadline_sec = time.monotonic() + float(
                self.get_parameter("arrival_timeout_sec").value
            )
            return
        if goal_type == "return_home" and self._state == "RETURNING_HOME":
            self._transition("DOCKING")
            dock_msg = Bool()
            dock_msg.data = True
            self._dock_pub.publish(dock_msg)

    def _handle_payload_event(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        if payload.get("event") == "PAYLOAD_REMOVED" and self._state == "ARRIVED":
            self._transition("PAYLOAD_REMOVED")
            self._arrival_deadline_sec = time.monotonic() + float(
                self.get_parameter("return_delay_after_payload_removed_sec").value
            )

    def _handle_dock_result(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        if self._state != "DOCKING":
            return
        if payload.get("status") == "SUCCEEDED":
            self._transition("DOCKED")
            self._transition("COMPLETED")
        elif payload.get("status") == "FAILED":
            self._transition("FAILED")

    def _handle_operator_command(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        command = payload.get("command")
        if command == "cancel":
            cancel = Bool()
            cancel.data = True
            self._nav_cancel_pub.publish(cancel)
            self._transition("FAILED")
        elif command == "return_home":
            self._begin_return_home()

    def _check_arrival_timeout(self) -> None:
        if self._state not in {"ARRIVED", "PAYLOAD_REMOVED"} or self._arrival_deadline_sec is None:
            return
        if time.monotonic() < self._arrival_deadline_sec:
            return
        self._begin_return_home()

    def _begin_return_home(self) -> None:
        if self._state in {"RETURNING_HOME", "DOCKING", "DOCKED", "COMPLETED"}:
            return
        self._transition("RETURNING_HOME")
        self._send_nav_goal(str(self.get_parameter("home_waypoint_id").value), "return_home")

    def _send_nav_goal(self, waypoint_id: str, goal_type: str) -> None:
        message = String()
        message.data = dump_json_message(
            {
                "mission_id": self._mission.get("mission_id"),
                "goal_type": goal_type,
                "waypoint_id": waypoint_id,
                "route": self._mission.get("route", {}),
            }
        )
        self._nav_goal_pub.publish(message)

    def _transition(self, state: str) -> None:
        self._state = state
        self._publish_state()
        self.get_logger().info("mission transition -> %s" % state)

    def _publish_state(self) -> None:
        message = String()
        message.data = dump_json_message(
            {
                "mission_id": self._mission.get("mission_id", ""),
                "state": self._state,
                "button_id": self._mission.get("button_id", ""),
                "target_waypoint_id": self._mission.get("target_waypoint_id", ""),
                "route": self._mission.get("route", {}),
            }
        )
        self._state_pub.publish(message)


def main() -> None:
    rclpy.init()
    node = MissionManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
