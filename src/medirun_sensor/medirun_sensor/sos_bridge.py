from __future__ import annotations

import json
from typing import List

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

from .serial_utils import SerialConfig, SerialLineReader, cycle_mock_lines


class SosBridge(Node):
    def __init__(self) -> None:
        super().__init__("sos_bridge")
        self.declare_parameter("serial_port", "/dev/ttyUSB0")
        self.declare_parameter("baud_rate", 9600)
        self.declare_parameter("mock_mode", True)
        self.declare_parameter("mock_events", ["BTN_01", "BTN_02", "BTN_03"])
        self.declare_parameter("poll_interval_sec", 0.05)

        self._mock_mode = self.get_parameter("mock_mode").value
        self._mock_index = 0
        self._mock_events: List[str] = list(self.get_parameter("mock_events").value)
        self._sos_pub = self.create_publisher(String, "/medirun/sensor/sos_button", 10)
        self._diag_pub = self.create_publisher(String, "/medirun/sensor/sos_status", 10)

        if self._mock_mode:
            self._reader = None
        else:
            config = SerialConfig(
                port=self.get_parameter("serial_port").value,
                baud_rate=int(self.get_parameter("baud_rate").value),
            )
            self._reader = SerialLineReader(config)
            opened = self._reader.open()
            if not opened:
                self.get_logger().warning("pyserial unavailable, forcing mock mode")
                self._mock_mode = True
                self._reader = None

        interval = float(self.get_parameter("poll_interval_sec").value)
        self.create_timer(interval, self._poll_serial)
        self.get_logger().info(
            "sos_bridge ready: mock_mode=%s" % str(self._mock_mode).lower()
        )

    def _poll_serial(self) -> None:
        line = None
        if self._mock_mode:
            return
        if self._reader is not None:
            line = self._reader.read_line()
        if not line:
            return
        if line == "READY":
            self._publish_diag("READY", {"source": "sos_button_module"})
            return
        if not line.startswith("BTN_"):
            self._publish_diag("IGNORED", {"line": line})
            return

        msg = String()
        msg.data = line
        self._sos_pub.publish(msg)
        self._publish_diag("BUTTON", {"button_id": line})

    def _publish_diag(self, state: str, payload: dict) -> None:
        msg = String()
        msg.data = json.dumps({"state": state, **payload}, ensure_ascii=True)
        self._diag_pub.publish(msg)

    def destroy_node(self) -> bool:
        if self._reader is not None:
            self._reader.close()
        return super().destroy_node()


def main() -> None:
    rclpy.init()
    node = SosBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
