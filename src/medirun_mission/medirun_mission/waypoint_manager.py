from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

import rclpy
import yaml
from rclpy.node import Node
from std_msgs.msg import String

from .json_topic import dump_json_message, load_json_message


class WaypointManager(Node):
    def __init__(self) -> None:
        super().__init__("waypoint_manager")
        self.declare_parameter("waypoints_file", "src/medirun_mission/config/waypoints.yaml")
        self._dispatch_pub = self.create_publisher(String, "/medirun/mission/dispatch_command", 10)
        self._waypoints = self._load_waypoints()
        self.create_subscription(
            String,
            "/medirun/mission/dispatch_request",
            self._handle_dispatch_request,
            10,
        )
        self.get_logger().info("waypoint_manager loaded %d routes" % len(self._waypoints))

    def _load_waypoints(self) -> Dict[str, Any]:
        path = Path(self.get_parameter("waypoints_file").value)
        with path.open("r", encoding="utf-8") as stream:
            payload = yaml.safe_load(stream) or {}
        return payload.get("routes", {})

    def _handle_dispatch_request(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        button_id = payload.get("button_id")
        route = self._waypoints.get(button_id)
        if route is None:
            self.get_logger().warning("unknown button_id=%s" % button_id)
            return

        command = dict(payload)
        command["route"] = route
        command["target_waypoint_id"] = route["target_waypoint_id"]
        out = String()
        out.data = dump_json_message(command)
        self._dispatch_pub.publish(out)


def main() -> None:
    rclpy.init()
    node = WaypointManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
