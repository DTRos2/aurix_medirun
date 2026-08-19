# ArUco 기반 전면주차 관련 노드를 한 번에 띄우는 launch 파일.
# medirun_bringup/launch/bringup.launch.py에서 include 된다.
#
# camera_x/y/z/yaw/pitch/roll: base_link 기준 카메라 장착 위치/자세.
# 실제 로봇에 맞춰 launch 인자로 덮어써야 한다(기본값은 트레일러 앞쪽,
# 로봇 정면 하단에 카메라가 정면을 보도록 장착한 예시 값이다).
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_vision"))
    config = str(share_dir / "config" / "parking.yaml")

    camera_x = LaunchConfiguration("camera_x")
    camera_y = LaunchConfiguration("camera_y")
    camera_z = LaunchConfiguration("camera_z")
    camera_yaw = LaunchConfiguration("camera_yaw")
    camera_pitch = LaunchConfiguration("camera_pitch")
    camera_roll = LaunchConfiguration("camera_roll")

    return LaunchDescription(
        [
            DeclareLaunchArgument("camera_x", default_value="0.10", description="base_link 기준 카메라 X(전방) 오프셋(m)"),
            DeclareLaunchArgument("camera_y", default_value="0.0", description="base_link 기준 카메라 Y(좌우) 오프셋(m)"),
            DeclareLaunchArgument("camera_z", default_value="0.05", description="base_link 기준 카메라 Z(높이) 오프셋(m)"),
            DeclareLaunchArgument("camera_yaw", default_value="0.0", description="카메라 장착 yaw(rad)"),
            DeclareLaunchArgument("camera_pitch", default_value="0.0", description="카메라 장착 pitch(rad)"),
            DeclareLaunchArgument("camera_roll", default_value="0.0", description="카메라 장착 roll(rad)"),
            Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                name="camera_link_broadcaster",
                arguments=[
                    "--x", camera_x,
                    "--y", camera_y,
                    "--z", camera_z,
                    "--yaw", camera_yaw,
                    "--pitch", camera_pitch,
                    "--roll", camera_roll,
                    "--frame-id", "base_link",
                    "--child-frame-id", "camera_link",
                ],
            ),
            Node(
                package="medirun_vision",
                executable="aruco_detector_node",
                name="aruco_detector_node",
                parameters=[config],
                output="screen",
            ),
            Node(
                package="medirun_vision",
                executable="docking_manager",
                name="docking_manager",
                parameters=[config],
                output="screen",
            ),
        ]
    )
