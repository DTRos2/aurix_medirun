from __future__ import annotations

import time
from typing import Optional

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from std_msgs.msg import Bool, String

from .json_topic import dump_json_message, load_json_message


class NavigationManager(Node):
    def __init__(self) -> None:
        super().__init__("navigation_manager")
        self.declare_parameter("simulate_navigation", True)
        self.declare_parameter("simulated_travel_sec", 5.0)
        self.declare_parameter("cruise_linear_x", 0.12)

        self._active_goal = None
        self._goal_started_at: Optional[float] = None
        self._status_pub = self.create_publisher(String, "/medirun/navigation/status", 10)
        self._cmd_pub = self.create_publisher(Twist, "/medirun/navigation/cmd_vel_raw", 10)
        self.create_subscription(String, "/medirun/navigation/goal", self._handle_goal, 10)
        self.create_subscription(Bool, "/medirun/navigation/cancel", self._handle_cancel, 10)
        self.create_timer(0.2, self._tick)

    def _handle_goal(self, msg: String) -> None:
        self._active_goal = load_json_message(msg.data)
        self._goal_started_at = time.monotonic()
        self._publish_status("ACCEPTED")

    def _handle_cancel(self, msg: Bool) -> None:
        if not msg.data or self._active_goal is None:
            return
        self._publish_stop()
        self._publish_status("FAILED")
        self._active_goal = None
        self._goal_started_at = None

    def _tick(self) -> None:
        if self._active_goal is None or self._goal_started_at is None:
            return
        if bool(self.get_parameter("simulate_navigation").value):
            self._publish_drive()
            if time.monotonic() - self._goal_started_at >= float(
                self.get_parameter("simulated_travel_sec").value
            ):
                self._publish_stop()
                self._publish_status("SUCCEEDED")
                self._active_goal = None
                self._goal_started_at = None

    def _publish_drive(self) -> None:
        twist = Twist()
        twist.linear.x = float(self.get_parameter("cruise_linear_x").value)
        self._cmd_pub.publish(twist)

    def _publish_stop(self) -> None:
        self._cmd_pub.publish(Twist())

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
