from __future__ import annotations

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

from .json_topic import load_json_message


AUDIO_BY_STATE = {
    "IDLE": "system_ready",
    "NAVIGATING": "aed_dispatch_started",
    "ARRIVED": "aed_has_arrived",
    "RETURNING_HOME": "returning_home",
    "FAILED": "operation_stopped",
    "COMPLETED": "docking_complete",
}


class AudioManager(Node):
    def __init__(self) -> None:
        super().__init__("audio_manager")
        self._last_prompt = ""
        self._pub = self.create_publisher(String, "/medirun/audio/announcement", 10)
        self.create_subscription(String, "/medirun/mission/state", self._handle_state, 10)

    def _handle_state(self, msg: String) -> None:
        state = load_json_message(msg.data).get("state", "IDLE")
        prompt = AUDIO_BY_STATE.get(state)
        if not prompt or prompt == self._last_prompt:
            return
        self._last_prompt = prompt
        out = String()
        out.data = prompt
        self._pub.publish(out)


def main() -> None:
    rclpy.init()
    node = AudioManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
