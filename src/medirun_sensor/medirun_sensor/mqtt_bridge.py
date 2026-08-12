from __future__ import annotations

import json

import paho.mqtt.client as mqtt
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class MqttBridge(Node):
    def __init__(self) -> None:
        super().__init__("mqtt_bridge")
        self.declare_parameter("mqtt_broker_host", "192.168.230.9")
        self.declare_parameter("mqtt_broker_port", 1883)
        self.declare_parameter("mqtt_topic", "medirun/button")

        self._sos_pub = self.create_publisher(String, "/medirun/sensor/sos_button", 10)
        self._diag_pub = self.create_publisher(String, "/medirun/sensor/sos_status", 10)

        self._topic = str(self.get_parameter("mqtt_topic").value)
        host = str(self.get_parameter("mqtt_broker_host").value)
        port = int(self.get_parameter("mqtt_broker_port").value)

        # paho-mqtt 2.x는 callback_api_version 인자가 있어야 경고 없이 동작하지만
        # apt의 python3-paho-mqtt(1.5.1)에는 이 인자/enum 자체가 없다. 두 버전
        # 모두 VERSION1과 동일한 콜백 시그니처(client, userdata, ...)로 동작한다.
        try:
            self._client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
        except (AttributeError, TypeError):
            self._client = mqtt.Client()
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message
        self._client.reconnect_delay_set(min_delay=1, max_delay=30)

        self.get_logger().info("MQTT 브로커 연결 시도: %s:%d" % (host, port))
        self._client.connect_async(host, port)
        self._client.loop_start()

    def _on_connect(self, client: mqtt.Client, userdata, flags, rc) -> None:
        if rc == 0:
            client.subscribe(self._topic)
            self.get_logger().info("MQTT 연결 완료, 구독 topic=%s" % self._topic)
            self._publish_diag("CONNECTED", {})
        else:
            self.get_logger().warning("MQTT 연결 실패: rc=%s" % rc)

    def _on_disconnect(self, client: mqtt.Client, userdata, rc) -> None:
        self.get_logger().warning("MQTT 연결 끊김: rc=%s (자동 재연결 시도)" % rc)
        self._publish_diag("DISCONNECTED", {"rc": rc})

    # paho-mqtt의 네트워크 스레드(loop_start)에서 호출된다 — rclpy publish는
    # 스레드 안전해서 문제 없지만, 이 콜백이 spin 스레드가 아니라는 점만 유의.
    def _on_message(self, client: mqtt.Client, userdata, msg: mqtt.MQTTMessage) -> None:
        button_id = msg.payload.decode("utf-8", errors="ignore").strip()
        if not button_id:
            return
        out = String()
        out.data = button_id
        self._sos_pub.publish(out)
        self.get_logger().info("MQTT 버튼 수신: %s" % button_id)
        self._publish_diag("BUTTON", {"button_id": button_id})

    def _publish_diag(self, state: str, payload: dict) -> None:
        msg = String()
        msg.data = json.dumps({"state": state, **payload}, ensure_ascii=True)
        self._diag_pub.publish(msg)

    def destroy_node(self) -> bool:
        self._client.loop_stop()
        self._client.disconnect()
        return super().destroy_node()


def main() -> None:
    rclpy.init()
    node = MqttBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
