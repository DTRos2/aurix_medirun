# Pi에 붙은 카메라를 ROS2 토픽으로 발행하는 launch 파일.
# bringup.launch.py(Nav2 + 대시보드, VM에서 실행)와는 별개로, 로봇 본체
# 드라이버(turtlebot3_bringup robot.launch.py)와 마찬가지로 Pi 쪽에서 직접
# 띄운다.
#
# 사용 예:
#   ros2 launch medirun_bringup camera.launch.py
#   ros2 launch medirun_bringup camera.launch.py video_device:=/dev/video2
#
# 발행 토픽: /camera/image_raw, /camera/camera_info (v4l2_camera 패키지가
# 정한 토픽 이름을 노드 네임스페이스로만 감싼 것 — CONVENTIONS.md 4.3절:
# 외부 패키지 토픽은 그대로 쓴다)
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    video_device = LaunchConfiguration("video_device")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "video_device",
                default_value="/dev/video0",
                description="카메라 장치 경로 (로봇마다 다를 수 있음)",
            ),
            Node(
                package="v4l2_camera",
                executable="v4l2_camera_node",
                name="camera_bridge",
                namespace="camera",
                output="screen",
                parameters=[
                    {
                        "video_device": video_device,
                        "image_size": [640, 480],
                    }
                ],
            ),
        ]
    )
