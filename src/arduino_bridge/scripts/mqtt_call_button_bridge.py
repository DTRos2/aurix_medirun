#!/usr/bin/env python3
# MQTT 무선 호출 버튼(ESP32)이 보낸 콜존 ID를 받아서
# GUI가 구독하는 /medirun/mission/dispatch_request로 그대로 중계한다.
# ESP32는 MQTT_TOPIC(예: medirun/button)으로 콜존 ID 문자열(예: "BTN_01")만 보낸다.
# 이 ID는 maps/waypoints/*.yaml에 저장된 call_zone 웨이포인트 ID와 정확히 일치해야 한다.

import json
import time

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class MqttCallButtonBridge(Node):
    def __init__(self) -> None:
        super().__init__("mqtt_call_button_bridge")

        self.declare_parameter("mqtt_broker", "192.168.230.9")
        self.declare_parameter("mqtt_port", 1883)
        self.declare_parameter("mqtt_topic", "medirun/button")
        self.declare_parameter("request_source", "mqtt_call_button")

        self._request_source = self.get_parameter("request_source").value
        self._mqtt_topic = self.get_parameter("mqtt_topic").value

        self._dispatch_pub = self.create_publisher(String, "/medirun/mission/dispatch_request", 10)

        broker = self.get_parameter("mqtt_broker").value
        port = int(self.get_parameter("mqtt_port").value)

        self._mqtt_client = mqtt.Client()
        self._mqtt_client.on_connect = self._on_connect
        self._mqtt_client.on_message = self._on_message
        self._mqtt_client.on_disconnect = self._on_disconnect

        self.get_logger().info(f"MQTT 브로커 연결 시도: {broker}:{port}")
        self._mqtt_client.connect_async(broker, port)
        self._mqtt_client.loop_start()

    def _on_connect(self, client, userdata, flags, rc) -> None:
        if rc == 0:
            client.subscribe(self._mqtt_topic)
            self.get_logger().info(f"MQTT 연결됨, 구독 시작: {self._mqtt_topic}")
        else:
            self.get_logger().error(f"MQTT 연결 실패: rc={rc}")

    def _on_disconnect(self, client, userdata, rc) -> None:
        self.get_logger().warning(f"MQTT 연결 끊김(rc={rc}), 재연결 대기 중")

    def _on_message(self, client, userdata, msg) -> None:
        button_id = msg.payload.decode("utf-8", errors="ignore").strip()
        if not button_id:
            return

        out = String()
        out.data = json.dumps(
            {
                "button_id": button_id,
                "request_source": self._request_source,
                "mission_id": "",
                "requested_at_sec": time.time(),
            },
            ensure_ascii=True,
        )
        self._dispatch_pub.publish(out)
        self.get_logger().info(f"MQTT 버튼 수신({button_id}) -> dispatch_request 발행")

    def destroy_node(self) -> bool:
        self._mqtt_client.loop_stop()
        self._mqtt_client.disconnect()
        return super().destroy_node()


def main() -> None:
    rclpy.init()
    node = MqttCallButtonBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
