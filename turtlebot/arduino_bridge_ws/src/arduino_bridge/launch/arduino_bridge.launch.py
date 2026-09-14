from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    port = LaunchConfiguration("port")
    baud = LaunchConfiguration("baud")

    return LaunchDescription([
        DeclareLaunchArgument("port", default_value="/dev/ttyACM0"),
        DeclareLaunchArgument("baud", default_value="9600"),
        Node(
            package="arduino_bridge",
            executable="serial_bridge",
            name="serial_bridge",
            parameters=[{"port": port, "baud": ParameterValue(baud, value_type=int)}],
            output="screen",
        ),
        Node(
            package="arduino_bridge",
            executable="led_status_node",
            name="led_status_node",
            output="screen",
        ),
        Node(
            package="arduino_bridge",
            executable="estop_node",
            name="estop_node",
            output="screen",
        ),
    ])
