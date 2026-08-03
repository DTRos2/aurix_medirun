from __future__ import annotations

import socket

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class NetworkMonitor(Node):
    def __init__(self) -> None:
        super().__init__("network_monitor")
        self.declare_parameter("heartbeat_host", "8.8.8.8")
        self.declare_parameter("heartbeat_port", 53)
        self.declare_parameter("heartbeat_interval_sec", 5.0)
        self._pub = self.create_publisher(String, "/medirun/network/state", 10)
        self.create_timer(float(self.get_parameter("heartbeat_interval_sec").value), self._check)

    def _check(self) -> None:
        host = str(self.get_parameter("heartbeat_host").value)
        port = int(self.get_parameter("heartbeat_port").value)
        connected = False
        try:
            with socket.create_connection((host, port), timeout=1.0):
                connected = True
        except OSError:
            connected = False
        msg = String()
        msg.data = "CONNECTED" if connected else "DISCONNECTED"
        self._pub.publish(msg)


def main() -> None:
    rclpy.init()
    node = NetworkMonitor()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
