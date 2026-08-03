from __future__ import annotations

import json
from typing import Optional

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Int32, String

from .serial_utils import SerialConfig, SerialLineReader, parse_key_value_line


class ArduinoBridge(Node):
    def __init__(self) -> None:
        super().__init__("arduino_bridge")
        self.declare_parameter("serial_port", "/dev/ttyUSB1")
        self.declare_parameter("baud_rate", 9600)
        self.declare_parameter("mock_mode", True)
        self.declare_parameter("poll_interval_sec", 0.05)
        self.declare_parameter("mock_front_distance_cm", 120)
        self.declare_parameter("mock_rear_distance_cm", 90)
        self.declare_parameter("mock_payload_loaded", True)

        self._mock_mode = bool(self.get_parameter("mock_mode").value)
        self._reader: Optional[SerialLineReader] = None
        self._last_led_command = "LED:IDLE"

        self._front_pub = self.create_publisher(Int32, "/medirun/sensor/front_distance_cm", 10)
        self._rear_pub = self.create_publisher(Int32, "/medirun/sensor/rear_distance_cm", 10)
        self._payload_pub = self.create_publisher(Bool, "/medirun/sensor/payload_loaded", 10)
        self._sos_pub = self.create_publisher(String, "/medirun/sensor/sos_button", 10)
        self._diag_pub = self.create_publisher(String, "/medirun/sensor/arduino_status", 10)
        self.create_subscription(
            String,
            "/medirun/sensor/led_command",
            self._handle_led_command,
            10,
        )

        if not self._mock_mode:
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
            "arduino_bridge ready: mock_mode=%s" % str(self._mock_mode).lower()
        )

    def _poll_serial(self) -> None:
        if self._mock_mode:
            self._publish_mock_state()
            return
        if self._reader is None:
            return
        line = self._reader.read_line()
        if not line:
            return
        if line == "READY":
            self._publish_diag("READY", {"source": "robot_arduino"})
            return
        key, value = parse_key_value_line(line)
        if key == "DIST":
            self._publish_int(self._front_pub, int(value))
        elif key == "DIST_REAR":
            self._publish_int(self._rear_pub, int(value))
        elif key == "HALL":
            self._publish_bool(self._payload_pub, value == "1")
        elif key.startswith("BTN_"):
            self._publish_button(key)
        elif key == "RAW":
            self._publish_diag("RAW_HALL", {"value": value})
        else:
            self._publish_diag("IGNORED", {"line": line})

    def _publish_mock_state(self) -> None:
        self._publish_int(
            self._front_pub,
            int(self.get_parameter("mock_front_distance_cm").value),
        )
        self._publish_int(
            self._rear_pub,
            int(self.get_parameter("mock_rear_distance_cm").value),
        )
        self._publish_bool(
            self._payload_pub,
            bool(self.get_parameter("mock_payload_loaded").value),
        )

    def _handle_led_command(self, msg: String) -> None:
        self._last_led_command = msg.data.strip()
        if self._reader is not None:
            self._reader.write_line(self._last_led_command)
        self._publish_diag("LED_COMMAND", {"command": self._last_led_command})

    def _publish_diag(self, state: str, payload: dict) -> None:
        msg = String()
        msg.data = json.dumps({"state": state, **payload}, ensure_ascii=True)
        self._diag_pub.publish(msg)

    def _publish_button(self, button_id: str) -> None:
        msg = String()
        msg.data = button_id
        self._sos_pub.publish(msg)
        self._publish_diag("BUTTON", {"button_id": button_id})

    @staticmethod
    def _publish_int(publisher, value: int) -> None:
        msg = Int32()
        msg.data = value
        publisher.publish(msg)

    @staticmethod
    def _publish_bool(publisher, value: bool) -> None:
        msg = Bool()
        msg.data = value
        publisher.publish(msg)

    def destroy_node(self) -> bool:
        if self._reader is not None:
            self._reader.close()
        return super().destroy_node()


def main() -> None:
    rclpy.init()
    node = ArduinoBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
