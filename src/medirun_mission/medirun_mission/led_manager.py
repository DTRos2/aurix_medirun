from __future__ import annotations

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

from .json_topic import load_json_message


LED_BY_STATE = {
    "IDLE": "LED:IDLE",
    "DISPATCHED": "LED:RUN",
    "NAVIGATING": "LED:RUN",
    "ARRIVED": "LED:ARRIVE",
    "PAYLOAD_REMOVED": "LED:ARRIVE",
    "RETURNING_HOME": "LED:RUN",
    "DOCKING": "LED:RUN",
    "DOCKED": "LED:DOCKED",
    "COMPLETED": "LED:IDLE",
    "FAILED": "LED:ERROR",
}


class LedManager(Node):
    def __init__(self) -> None:
        super().__init__("led_manager")
        self._pub = self.create_publisher(String, "/medirun/sensor/led_command", 10)
        self.create_subscription(String, "/medirun/mission/state", self._handle_state, 10)
        self.create_subscription(String, "/medirun/mission/dispatch_rejection", self._handle_reject, 10)

    def _handle_state(self, msg: String) -> None:
        state = load_json_message(msg.data).get("state", "IDLE")
        command = LED_BY_STATE.get(state, "LED:ERROR")
        out = String()
        out.data = command
        self._pub.publish(out)

    def _handle_reject(self, _: String) -> None:
        out = String()
        out.data = "LED:STOP"
        self._pub.publish(out)


def main() -> None:
    rclpy.init()
    node = LedManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
