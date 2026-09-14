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
        self.declare_parameter("waypoints_file", "maps/waypoints/second.yaml")
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
        # GUI(WaypointRepository)와 같은 스키마: waypoints: {id: {x, y, yaw, kind, label}}
        # 이 노드의 다음 단계(mission_manager/navigation_manager)는 이전부터
        # {target_waypoint_id, frame_id, pose:{x,y,yaw}} 형태를 기대하므로 여기서 변환한다.
        path = Path(self.get_parameter("waypoints_file").value)
        with path.open("r", encoding="utf-8") as stream:
            payload = yaml.safe_load(stream) or {}
        frame_id = payload.get("frame_id", "map")
        waypoints = payload.get("waypoints", {})
        routes: Dict[str, Any] = {}
        for waypoint_id, waypoint in waypoints.items():
            routes[waypoint_id] = {
                "target_waypoint_id": waypoint_id,
                "frame_id": frame_id,
                "pose": {
                    "x": waypoint["x"],
                    "y": waypoint["y"],
                    "yaw": waypoint.get("yaw", 0.0),
                },
            }
        return routes

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
