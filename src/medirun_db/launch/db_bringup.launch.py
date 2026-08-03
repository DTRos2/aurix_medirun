from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_db"))
    config = str(share_dir / "config" / "db_runtime.yaml")
    return LaunchDescription(
        [
            Node(
                package="medirun_db",
                executable="event_logger",
                name="event_logger",
                parameters=[config],
                output="screen",
            ),
            Node(
                package="medirun_db",
                executable="network_monitor",
                name="network_monitor",
                parameters=[config],
                output="screen",
            ),
            Node(
                package="medirun_db",
                executable="log_retention",
                name="log_retention",
                parameters=[config],
                output="screen",
            ),
        ]
    )
