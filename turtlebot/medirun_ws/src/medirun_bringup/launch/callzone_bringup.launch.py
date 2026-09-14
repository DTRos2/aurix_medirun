# ESP32 콜버튼(MQTT) -> dispatch_request 까지만 띄우는 launch.
#
# waypoint_manager/mission_manager/navigation_manager는 여기서 안 띄운다 —
# 대시보드(medirun_gui)가 이미 dispatch_request를 받아 자체적으로 웨이포인트
# 조회 + navigate_to_pose 호출까지 수행하는 MissionController를 갖고 있어서,
# 이 launch가 그것까지 같이 하면 같은 로봇에 목표를 두 번 보내 Nav2 goal이
# 서로 preempt되어 실패한다(aborted). 대시보드가 꺼져 있으면 이 launch만으로는
# 버튼을 눌러도 로봇이 움직이지 않는다 — 반드시 medirun_gui가 떠 있어야 한다.
#
# 사용 예:
#   ros2 launch medirun_bringup callzone_bringup.launch.py
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    sensor_config = str(Path(get_package_share_directory("medirun_sensor")) / "config" / "sensor_ports.yaml")
    mission_config = str(Path(get_package_share_directory("medirun_mission")) / "config" / "mission.yaml")

    return LaunchDescription(
        [
            Node(
                package="medirun_sensor",
                executable="mqtt_bridge",
                name="mqtt_bridge",
                parameters=[sensor_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="emergency_dispatcher",
                name="emergency_dispatcher",
                parameters=[mission_config],
                output="screen",
            ),
        ]
    )
