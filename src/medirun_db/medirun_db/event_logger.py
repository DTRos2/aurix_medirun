from __future__ import annotations

import json
from datetime import datetime
from pathlib import Path

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class EventLogger(Node):
    def __init__(self) -> None:
        super().__init__("event_logger")
        self.declare_parameter("log_dir", "logs/medirun")
        log_dir = Path(self.get_parameter("log_dir").value)
        log_dir.mkdir(parents=True, exist_ok=True)
        self._file_path = log_dir / f"events_{datetime.now().strftime('%Y%m%d')}.jsonl"
        for topic in [
            "/medirun/mission/state",
            "/medirun/mission/dispatch_rejection",
            "/medirun/safety/state",
            "/medirun/ui/telemetry",
            "/medirun/vision/dock_result",
        ]:
            self.create_subscription(String, topic, self._log_message_factory(topic), 10)

    def _log_message_factory(self, topic: str):
        def _callback(msg: String) -> None:
            timestamp = self.get_clock().now().to_msg()
            with self._file_path.open("a", encoding="utf-8") as stream:
                stream.write(
                    json.dumps(
                        {
                            "topic": topic,
                            "stamp_sec": timestamp.sec,
                            "stamp_nanosec": timestamp.nanosec,
                            "data": msg.data,
                        },
                        ensure_ascii=True,
                    )
                    + "\n"
                )

        return _callback


def main() -> None:
    rclpy.init()
    node = EventLogger()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
