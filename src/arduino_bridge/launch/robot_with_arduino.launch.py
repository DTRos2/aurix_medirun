import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    IncludeLaunchDescription,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    # turtlebot3_bringup reads this directly with os.environ[].  Avoid a launch
    # generation failure when the shell did not export LDS_MODEL.
    os.environ.setdefault("LDS_MODEL", "LDS-01")

    arduino_port = LaunchConfiguration("arduino_port")
    opencr_port = LaunchConfiguration("opencr_port")
    baud = LaunchConfiguration("baud")
    camera_format = LaunchConfiguration("camera_format")
    camera_width = LaunchConfiguration("camera_width")
    camera_height = LaunchConfiguration("camera_height")
    audio_sink = LaunchConfiguration("audio_sink")
    start_cartographer = LaunchConfiguration("start_cartographer")
    use_sim_time = LaunchConfiguration("use_sim_time")
    start_mqtt_call_button_bridge = LaunchConfiguration("start_mqtt_call_button_bridge")
    mqtt_broker = LaunchConfiguration("mqtt_broker")
    mqtt_port = LaunchConfiguration("mqtt_port")
    mqtt_topic = LaunchConfiguration("mqtt_topic")

    turtlebot3_bringup = get_package_share_directory("turtlebot3_bringup")
    turtlebot3_cartographer = get_package_share_directory("turtlebot3_cartographer")
    cartographer_config_dir = os.path.join(turtlebot3_cartographer, "config")

    # An overlay that has not been rebuilt may contain the package index but not
    # its data files yet.  Keep this launch usable in that state as well.
    if not os.path.isdir(cartographer_config_dir):
        workspace_dir = turtlebot3_cartographer
        for _ in range(4):
            workspace_dir = os.path.dirname(workspace_dir)
        cartographer_config_dir = os.path.join(
            workspace_dir,
            "src",
            "turtlebot3",
            "turtlebot3_cartographer",
            "config",
        )

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
        DeclareLaunchArgument("use_sim_time", default_value="false"),
        DeclareLaunchArgument(
            "start_cartographer",
            default_value="false",
            description="통합 launch에서 Cartographer도 함께 실행",
        ),
        DeclareLaunchArgument(
            "camera_format",
            default_value="BGR888",
            description="turtlebot3_bringup camera.launch.py에 전달할 픽셀 포맷",
        ),
        # 2026-08-10에 640x480 -> 1280x720으로 화질을 올렸던 결정(구 v4l2_camera_node
        # 경로, medirun_bringup/launch/camera.launch.py)이 2026-08-19 Pi 런치 통합
        # (pi_bringup.launch.py -> 이 파일 -> turtlebot3_bringup/camera.launch.py,
        # camera_ros 경로로 교체됨) 이후 반영되지 않고 640x480 기본값으로 되돌아가
        # 있었다. 여기서 다시 명시적으로 넘겨 그 결정을 유지한다.
        DeclareLaunchArgument(
            "camera_width",
            default_value="1280",
            description="turtlebot3_bringup camera.launch.py에 전달할 카메라 발행 폭",
        ),
        DeclareLaunchArgument(
            "camera_height",
            default_value="720",
            description="turtlebot3_bringup camera.launch.py에 전달할 카메라 발행 높이",
        ),
        DeclareLaunchArgument(
            "audio_sink",
            default_value=(
                "alsa_output.usb-Jieli_Technology_UACDemoV1.0_"
                "4150344E37343316-00.analog-stereo"
            ),
            description="gTTS 음성을 출력할 PulseAudio sink",
        ),
        DeclareLaunchArgument(
            "start_mqtt_call_button_bridge",
            default_value="false",
            description="ESP32 무선 호출 버튼(MQTT)을 dispatch_request로 중계하는 노드를 함께 실행",
        ),
        DeclareLaunchArgument(
            "mqtt_broker",
            default_value="192.168.230.9",
            description="무선 호출 버튼 ESP32가 연결하는 MQTT 브로커(Pi) IP",
        ),
        DeclareLaunchArgument("mqtt_port", default_value="1883"),
        DeclareLaunchArgument("mqtt_topic", default_value="medirun/button"),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(turtlebot3_bringup, "launch", "robot.launch.py")
            ),
            launch_arguments={
                "usb_port": opencr_port,
                "use_sim_time": use_sim_time,
            }.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(turtlebot3_bringup, "launch", "camera.launch.py")
            ),
            launch_arguments={
                "format": camera_format,
                "width": camera_width,
                "height": camera_height,
            }.items(),
        ),

        Node(
            package="cartographer_ros",
            executable="cartographer_node",
            name="cartographer_node",
            arguments=[
                "-configuration_directory",
                cartographer_config_dir,
                "-configuration_basename",
                "turtlebot3_lds_2d.lua",
            ],
            parameters=[{"use_sim_time": use_sim_time}],
            remappings=[
                ("scan", "/scan"),
                ("tf", "/tf"),
                ("tf_static", "/tf_static"),
            ],
            condition=IfCondition(start_cartographer),
            output="screen",
        ),
        Node(
            package="cartographer_ros",
            executable="cartographer_occupancy_grid_node",
            name="cartographer_occupancy_grid_node",
            arguments=["-resolution", "0.05", "-publish_period_sec", "1.0"],
            parameters=[{"use_sim_time": use_sim_time}],
            condition=IfCondition(start_cartographer),
            output="screen",
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
            package="arduino_bridge",
            executable="mqtt_call_button_bridge.py",
            name="mqtt_call_button_bridge",
            parameters=[
                {
                    "mqtt_broker": mqtt_broker,
                    "mqtt_port": ParameterValue(mqtt_port, value_type=int),
                    "mqtt_topic": mqtt_topic,
                }
            ],
            condition=IfCondition(start_mqtt_call_button_bridge),
            output="screen",
        ),
        Node(
            package="robot_audio_output",
            executable="audio_output_node",
            name="audio_output_node",
            additional_env={"PULSE_SINK": audio_sink},
            output="screen",
        ),

        # bringup과 동시에 시작한다. 각 `--once` 발행은 해당 구독자가
        # 연결될 때까지 기다리므로 별도의 고정 지연 시간이 필요하지 않다.
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
    ])
