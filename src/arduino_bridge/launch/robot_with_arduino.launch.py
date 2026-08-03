import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
    TimerAction,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    arduino_port = LaunchConfiguration("arduino_port")
    opencr_port = LaunchConfiguration("opencr_port")
    baud = LaunchConfiguration("baud")
    audio_sink = LaunchConfiguration("audio_sink")
    turtlebot3_bringup = get_package_share_directory("turtlebot3_bringup")

    return LaunchDescription([
        DeclareLaunchArgument(
            "arduino_port",
            default_value=(
                "/dev/serial/by-id/"
                "usb-Arduino__www.arduino.cc__0043_1344B435234351A048A6-if00"
            ),
            description="Arduino serial port; /dev/serial/by-id/... 사용 권장",
        ),
        DeclareLaunchArgument(
            "opencr_port",
            default_value=(
                "/dev/serial/by-id/"
                "usb-ROBOTIS_OpenCR_Virtual_ComPort_in_FS_Mode_FFFFFFFEFFFF-if00"
            ),
            description="TurtleBot3 OpenCR serial port",
        ),
        DeclareLaunchArgument("baud", default_value="9600"),
        DeclareLaunchArgument(
            "audio_sink",
            default_value=(
                "alsa_output.usb-Jieli_Technology_UACDemoV1.0_"
                "4150344E37343316-00.analog-stereo"
            ),
            description="gTTS 음성을 출력할 PulseAudio sink",
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(turtlebot3_bringup, "launch", "robot.launch.py")
            ),
            launch_arguments={"usb_port": opencr_port}.items(),
        ),
        Node(
            package="arduino_bridge",
            executable="serial_bridge",
            name="serial_bridge",
            parameters=[
                {
                    "port": arduino_port,
                    "baud": ParameterValue(baud, value_type=int),
                }
            ],
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
        Node(
            package="robot_audio_output",
            executable="audio_output_node",
            name="audio_output_node",
            additional_env={"PULSE_SINK": audio_sink},
            output="screen",
        ),
        ExecuteProcess(
            cmd=[
                "ros2",
                "topic",
                "pub",
                "--once",
                "/arduino/led",
                "std_msgs/msg/String",
                "{data: IDLE}",
            ],
            output="screen",
        ),
        TimerAction(
            period=2.0,
            actions=[
                ExecuteProcess(
                    cmd=[
                        "ros2",
                        "topic",
                        "pub",
                        "--once",
                        "/audio/command",
                        "robot_audio_interfaces/msg/AudioCommand",
                        (
                            "{type: 0, text: '주행을 시작합니다', sound_id: '', "
                            "volume: 1.0, repeat: 1}"
                        ),
                    ],
                    output="screen",
                ),
            ],
        ),
    ])
