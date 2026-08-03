from __future__ import annotations

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, String

from .json_topic import dump_json_message, load_json_message


class PayloadMonitor(Node):
    def __init__(self) -> None:
        super().__init__("payload_monitor")
        self.declare_parameter("payload_removed_confirm_sec", 0.5)
        self._mission_state = "IDLE"
        self._last_payload_loaded = True
        self._payload_removed_started_at = None
        self._event_pub = self.create_publisher(String, "/medirun/mission/payload_event", 10)
        self.create_subscription(Bool, "/medirun/sensor/payload_loaded", self._handle_payload, 10)
        self.create_subscription(String, "/medirun/mission/state", self._handle_state, 10)

    def _handle_state(self, msg: String) -> None:
        self._mission_state = load_json_message(msg.data).get("state", "IDLE")

    def _handle_payload(self, msg: Bool) -> None:
        now = self.get_clock().now().nanoseconds / 1_000_000_000
        if self._mission_state != "ARRIVED":
            self._payload_removed_started_at = None
            self._last_payload_loaded = msg.data
            return

        if self._last_payload_loaded and not msg.data:
            self._payload_removed_started_at = now
        elif not self._last_payload_loaded and msg.data:
            self._payload_removed_started_at = None

        if (
            not msg.data
            and self._payload_removed_started_at is not None
            and now - self._payload_removed_started_at
            >= float(self.get_parameter("payload_removed_confirm_sec").value)
        ):
            event = String()
            event.data = dump_json_message({"event": "PAYLOAD_REMOVED"})
            self._event_pub.publish(event)
            self._payload_removed_started_at = None
            self._mission_state = "PAYLOAD_REMOVED"
        self._last_payload_loaded = msg.data


def main() -> None:
    rclpy.init()
    node = PayloadMonitor()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
