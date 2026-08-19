# 로봇(Pi) 쪽에서 필요한 걸 한 번에 띄우는 launch 파일.
# arduino_bridge/robot_with_arduino.launch.py(드라이버·카메라·오디오) +
# Nav2 로컬라이제이션 + navigation_safety(초음파 안전 게이트 리매핑) +
# ultrasonic_safety_stop을 하나로 묶는다 — 원래 별도 터미널 4개로 띄우던 것
# (2026-08-19, Pi에서 Nav2 부하 테스트 중 수동으로 나눠 띄우던 걸 정리).
#
# 사용 예:
#   ros2 launch medirun_bringup pi_bringup.launch.py
#   ros2 launch medirun_bringup pi_bringup.launch.py map:=/path/other.yaml
#
# 전제:
#   - TURTLEBOT3_MODEL 환경변수 (기본 burger)
#   - arduino_bridge에 ultrasonic_safety_stop이 빌드되어 있어야 함
#     (Pi의 ~/arduino_bridge_ws는 이 저장소와 별개 워크스페이스라 수동 동기화 필요,
#     README "아두이노 브리지" 절 참고)
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

TURTLEBOT3_MODEL = os.environ.get("TURTLEBOT3_MODEL", "burger")
ROS_DISTRO = os.environ.get("ROS_DISTRO", "humble")

DEFAULT_MAP = os.path.join(os.path.expanduser("~"), "last_class_map_modi.yaml")


def generate_launch_description():
    map_yaml = LaunchConfiguration("map")
    use_sim_time = LaunchConfiguration("use_sim_time")

    param_file_name = TURTLEBOT3_MODEL + ".yaml"
    if ROS_DISTRO == "humble":
        param_dir = os.path.join(
            get_package_share_directory("turtlebot3_navigation2"), "param", ROS_DISTRO, param_file_name
        )
    else:
        param_dir = os.path.join(
            get_package_share_directory("turtlebot3_navigation2"), "param", param_file_name
        )

    arduino_bridge_dir = get_package_share_directory("arduino_bridge")
    nav2_launch_file_dir = os.path.join(get_package_share_directory("nav2_bringup"), "launch")
    medirun_launch_file_dir = os.path.join(get_package_share_directory("medirun_bringup"), "launch")

    return LaunchDescription(
        [
            DeclareLaunchArgument("map", default_value=DEFAULT_MAP, description="불러올 occupancy map YAML 경로"),
            DeclareLaunchArgument("use_sim_time", default_value="false", description="Gazebo 시뮬레이션이면 true"),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource([arduino_bridge_dir, "/launch/robot_with_arduino.launch.py"]),
                launch_arguments={"use_sim_time": use_sim_time}.items(),
            ),
            GroupAction(
                actions=[
                    IncludeLaunchDescription(
                        PythonLaunchDescriptionSource([nav2_launch_file_dir, "/localization_launch.py"]),
                        launch_arguments={
                            "map": map_yaml,
                            "use_sim_time": use_sim_time,
                            "params_file": param_dir,
                            "autostart": "true",
                        }.items(),
                    ),
                    IncludeLaunchDescription(
                        PythonLaunchDescriptionSource([medirun_launch_file_dir, "/navigation_safety.launch.py"]),
                        launch_arguments={
                            "use_sim_time": use_sim_time,
                            "params_file": param_dir,
                            "autostart": "true",
                        }.items(),
                    ),
                ]
            ),
            Node(
                package="arduino_bridge",
                executable="ultrasonic_safety_stop",
                name="ultrasonic_safety_stop",
                parameters=[{"front_stop_distance_cm": 15, "rear_stop_distance_cm": 15, "hold_duration_sec": 0.0}],
                output="screen",
            ),
        ]
    )
