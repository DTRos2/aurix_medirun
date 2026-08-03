from __future__ import annotations

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from std_msgs.msg import String

from .json_topic import dump_json_message, load_json_message


class RemoteControlBridge(Node):
    def __init__(self) -> None:
        super().__init__("remote_control_bridge")
        self._mission_pub = self.create_publisher(String, "/medirun/ui/mission_command", 10)
        self._twist_pub = self.create_publisher(Twist, "/medirun/ui/cmd_vel_raw", 10)
        self.create_subscription(String, "/medirun/ui/command", self._handle_command, 10)

    def _handle_command(self, msg: String) -> None:
        payload = load_json_message(msg.data)
        command_type = payload.get("type")
        if command_type == "mission":
            out = String()
            out.data = dump_json_message({"command": payload.get("command", "")})
            self._mission_pub.publish(out)
            return
        if command_type == "drive":
            twist = Twist()
            twist.linear.x = float(payload.get("linear_x", 0.0))
            twist.angular.z = float(payload.get("angular_z", 0.0))
            self._twist_pub.publish(twist)


def main() -> None:
    rclpy.init()
    node = RemoteControlBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
