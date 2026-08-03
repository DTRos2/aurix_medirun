import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    enable_joy = LaunchConfiguration("enable_joy")
    joy_dev = LaunchConfiguration("joy_dev")
    joy_deadzone = LaunchConfiguration("joy_deadzone")
    joy_autorepeat_rate = LaunchConfiguration("joy_autorepeat_rate")
    arduino_bridge_share = get_package_share_directory("arduino_bridge")

    return LaunchDescription([
        DeclareLaunchArgument(
            "enable_joy",
            default_value="true",
            description="PS4 조이스틱 입력 노드를 함께 실행할지 여부",
        ),
        DeclareLaunchArgument(
            "joy_dev",
            default_value="/dev/input/js0",
            description="joy_node가 읽을 조이스틱 디바이스 경로",
        ),
        DeclareLaunchArgument(
            "joy_deadzone",
            default_value="0.12",
            description="joy_node deadzone",
        ),
        DeclareLaunchArgument(
            "joy_autorepeat_rate",
            default_value="20.0",
            description="joy_node autorepeat rate (Hz)",
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(arduino_bridge_share, "launch", "robot_with_arduino.launch.py")
            ),
        ),
        Node(
            package="joy",
            executable="joy_node",
            name="joy_node",
            parameters=[
                {
                    "dev": joy_dev,
                    "deadzone": ParameterValue(joy_deadzone, value_type=float),
                    "autorepeat_rate": ParameterValue(
                        joy_autorepeat_rate, value_type=float
                    ),
                }
            ],
            output="screen",
            condition=IfCondition(enable_joy),
        ),
        Node(
            package="arduino_bridge",
            executable="ps4_teleop_node",
            name="ps4_teleop_node",
            output="screen",
            condition=IfCondition(enable_joy),
        ),
    ])
