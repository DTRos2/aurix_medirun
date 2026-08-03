from __future__ import annotations

from typing import Dict

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Int32, String

from .json_topic import dump_json_message, load_json_message


class TelemetryBridge(Node):
    def __init__(self) -> None:
        super().__init__("telemetry_bridge")
        self._snapshot: Dict[str, object] = {
            "mission_state": "IDLE",
            "payload_loaded": True,
            "front_distance_cm": 0,
            "rear_distance_cm": 0,
            "safety_state": "CLEAR",
        }
        self._pub = self.create_publisher(String, "/medirun/ui/telemetry", 10)
        self.create_subscription(String, "/medirun/mission/state", self._handle_mission, 10)
        self.create_subscription(Bool, "/medirun/sensor/payload_loaded", self._handle_payload, 10)
        self.create_subscription(Int32, "/medirun/sensor/front_distance_cm", self._handle_front, 10)
        self.create_subscription(Int32, "/medirun/sensor/rear_distance_cm", self._handle_rear, 10)
        self.create_subscription(String, "/medirun/safety/state", self._handle_safety, 10)

    def _handle_mission(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        self._snapshot["mission_state"] = payload.get("state", "IDLE")
        self._snapshot["mission_id"] = payload.get("mission_id", "")
        self._snapshot["button_id"] = payload.get("button_id", "")
        self._publish()

    def _handle_payload(self, msg: Bool) -> None:
        self._snapshot["payload_loaded"] = msg.data
        self._publish()

    def _handle_front(self, msg: Int32) -> None:
        self._snapshot["front_distance_cm"] = msg.data
        self._publish()

    def _handle_rear(self, msg: Int32) -> None:
        self._snapshot["rear_distance_cm"] = msg.data
        self._publish()

    def _handle_safety(self, msg: String) -> None:
        self._snapshot["safety_state"] = load_json_message(msg.data).get("state", "CLEAR")
        self._publish()

    def _publish(self) -> None:
        message = String()
        message.data = dump_json_message(self._snapshot)
        self._pub.publish(message)


def main() -> None:
    rclpy.init()
    node = TelemetryBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
