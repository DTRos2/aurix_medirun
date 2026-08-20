# PC(대시보드)에서 필요한 걸 한 번에 띄우는 launch 파일.
# medirun_gui 대시보드 + 콜존 파이프라인(mqtt_bridge, emergency_dispatcher)만
# 묶는다 — Nav2/안전정지/파킹은 이제 전부 Pi(pi_bringup.launch.py)가 전담하므로
# 여기서는 절대 안 띄운다(2026-08-19 Nav2 Pi 이전 이후 PC는 GUI만 책임짐).
#
# 사용 예:
#   ros2 launch medirun_bringup pc_bringup.launch.py
#
# 전제:
#   - 로봇(Pi) 쪽에서 pi_bringup.launch.py가 이미 떠 있어야 한다.
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

CYCLONEDDS_CONFIG = os.path.join(
    get_package_share_directory("turtlebot3_bringup"), "config", "cyclonedds_udp.xml"
)


def generate_launch_description():
    medirun_launch_file_dir = os.path.join(get_package_share_directory("medirun_bringup"), "launch")

    # name= 파라미터를 일부러 안 준다: launch_ros가 이걸 프로세스 전체에
    # `--ros-args -r __node:=medirun_gui` 전역 리매핑으로 주입하는데, 이 앱은
    # 내부적으로 rclcpp::Node를 여러 개(medirun_gui_nav_bridge,
    # medirun_gui_mission_bridge, medirun_gui_battery_monitor 등) 만들고 각각
    # 이미 고유 이름을 하드코딩해뒀다. 이 전역 리매핑이 그 이름들 중 일부를
    # "medirun_gui"로 덮어써서 서비스/액션 호출이 어느 노드로 갈지 불확정해지는
    # 문제가 있다(bringup.launch.py에서 2026-08-19 확인된 것과 동일 이슈).
    gui_node = Node(
        package="medirun_gui",
        executable="medirun_gui_app",
        output="screen",
        additional_env={
            "RMW_IMPLEMENTATION": "rmw_cyclonedds_cpp",
            "CYCLONEDDS_URI": "file://" + CYCLONEDDS_CONFIG,
        },
    )

    return LaunchDescription(
        [
            gui_node,
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource([medirun_launch_file_dir, "/callzone_bringup.launch.py"]),
            ),
        ]
    )
