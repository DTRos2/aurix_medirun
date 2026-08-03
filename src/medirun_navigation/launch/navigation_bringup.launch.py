from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_navigation"))
    runtime_config = str(share_dir / "config" / "navigation_runtime.yaml")
    return LaunchDescription(
        [
            Node(
                package="medirun_navigation",
                executable="navigation_manager",
                name="navigation_manager",
                parameters=[runtime_config],
                output="screen",
            )
        ]
    )
