from __future__ import annotations

from datetime import datetime, timedelta
from pathlib import Path

import rclpy
from rclpy.node import Node


class LogRetention(Node):
    def __init__(self) -> None:
        super().__init__("log_retention")
        self.declare_parameter("log_dir", "logs/medirun")
        self.declare_parameter("retention_days", 7)
        self.create_timer(3600.0, self._cleanup)

    def _cleanup(self) -> None:
        log_dir = Path(self.get_parameter("log_dir").value)
        if not log_dir.exists():
            return
        deadline = datetime.now() - timedelta(days=int(self.get_parameter("retention_days").value))
        for path in log_dir.glob("*.jsonl"):
            if datetime.fromtimestamp(path.stat().st_mtime) < deadline:
                path.unlink(missing_ok=True)


def main() -> None:
    rclpy.init()
    node = LogRetention()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
