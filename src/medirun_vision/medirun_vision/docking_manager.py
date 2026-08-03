from __future__ import annotations

import time
from typing import Optional

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, String

import json


class DockingManager(Node):
    def __init__(self) -> None:
        super().__init__("docking_manager")
        self.declare_parameter("simulate_docking", True)
        self.declare_parameter("simulated_docking_sec", 4.0)
        self._active_since: Optional[float] = None
        self._result_pub = self.create_publisher(String, "/medirun/vision/dock_result", 10)
        self.create_subscription(Bool, "/medirun/vision/dock_request", self._handle_request, 10)
        self.create_timer(0.2, self._tick)

    def _handle_request(self, msg: Bool) -> None:
        if msg.data:
            self._active_since = time.monotonic()

    def _tick(self) -> None:
        if self._active_since is None:
            return
        if not bool(self.get_parameter("simulate_docking").value):
            return
        if time.monotonic() - self._active_since < float(
            self.get_parameter("simulated_docking_sec").value
        ):
            return
        out = String()
        out.data = json.dumps({"status": "SUCCEEDED"}, ensure_ascii=True, sort_keys=True)
        self._result_pub.publish(out)
        self._active_since = None


def main() -> None:
    rclpy.init()
    node = DockingManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
