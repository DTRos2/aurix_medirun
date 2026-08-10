from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_sensor"))
    config = str(share_dir / "config" / "sensor_ports.yaml")
    return LaunchDescription(
        [
            DeclareLaunchArgument("start_sos_bridge", default_value="false"),
            Node(
                package="medirun_sensor",
                executable="sos_bridge",
                name="sos_bridge",
                parameters=[config],
                condition=IfCondition(LaunchConfiguration("start_sos_bridge")),
                output="screen",
            ),
            Node(
                package="medirun_sensor",
                executable="arduino_bridge",
                name="arduino_bridge",
                parameters=[config],
                output="screen",
            ),
            Node(
                package="medirun_sensor",
                executable="mqtt_bridge",
                name="mqtt_bridge",
                parameters=[config],
                output="screen",
            ),
            Node(
                package="medirun_sensor",
                executable="ready_notifier",
                name="ready_notifier",
                output="screen",
            ),
        ]
    )
