from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_vision"))
    config = str(share_dir / "config" / "docking.yaml")
    return LaunchDescription(
        [
            Node(
                package="medirun_vision",
                executable="docking_manager",
                name="docking_manager",
                parameters=[config],
                output="screen",
            )
        ]
    )
