from __future__ import annotations

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from std_msgs.msg import Int32, String

from .json_topic import dump_json_message


class SafetyManager(Node):
    def __init__(self) -> None:
        super().__init__("safety_manager")
        self.declare_parameter("front_stop_distance_cm", 35)
        self.declare_parameter("rear_stop_distance_cm", 25)
        self._front_cm = 400
        self._rear_cm = 400
        self._cmd_pub = self.create_publisher(Twist, "/cmd_vel", 10)
        self._state_pub = self.create_publisher(String, "/medirun/safety/state", 10)

        self.create_subscription(Int32, "/medirun/sensor/front_distance_cm", self._handle_front, 10)
        self.create_subscription(Int32, "/medirun/sensor/rear_distance_cm", self._handle_rear, 10)
        self.create_subscription(Twist, "/medirun/navigation/cmd_vel_raw", self._handle_cmd, 10)
        self.create_subscription(Twist, "/medirun/ui/cmd_vel_raw", self._handle_cmd, 10)

    def _handle_front(self, msg: Int32) -> None:
        self._front_cm = msg.data
        self._publish_state()

    def _handle_rear(self, msg: Int32) -> None:
        self._rear_cm = msg.data
        self._publish_state()

    def _handle_cmd(self, msg: Twist) -> None:
        blocked = False
        safe = Twist()
        safe.linear.x = msg.linear.x
        safe.angular.z = msg.angular.z
        if msg.linear.x > 0.0 and self._front_cm <= int(self.get_parameter("front_stop_distance_cm").value):
            safe.linear.x = 0.0
            blocked = True
        if msg.linear.x < 0.0 and self._rear_cm <= int(self.get_parameter("rear_stop_distance_cm").value):
            safe.linear.x = 0.0
            blocked = True
        self._cmd_pub.publish(safe)
        if blocked:
            self._publish_state("BLOCKED")

    def _publish_state(self, state: str = "CLEAR") -> None:
        message = String()
        message.data = dump_json_message(
            {
                "state": state,
                "front_distance_cm": self._front_cm,
                "rear_distance_cm": self._rear_cm,
            }
        )
        self._state_pub.publish(message)


def main() -> None:
    rclpy.init()
    node = SafetyManager()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
