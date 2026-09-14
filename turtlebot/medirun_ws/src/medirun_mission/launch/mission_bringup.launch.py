import os
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
WORKSPACE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))


def generate_launch_description() -> LaunchDescription:
    share_dir = Path(get_package_share_directory("medirun_mission"))
    mission_config = str(share_dir / "config" / "mission.yaml")
    # GUI(WaypointRepository::defaultWaypointFilePath)가 기본으로 보는 파일과 동일하게 맞춘다.
    waypoint_config = os.path.join(WORKSPACE_ROOT, "maps", "waypoints", "second.yaml")
    return LaunchDescription(
        [
            Node(
                package="medirun_mission",
                executable="waypoint_manager",
                name="waypoint_manager",
                parameters=[{"waypoints_file": waypoint_config}, mission_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="emergency_dispatcher",
                name="emergency_dispatcher",
                parameters=[mission_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="mission_manager",
                name="mission_manager",
                parameters=[mission_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="payload_monitor",
                name="payload_monitor",
                parameters=[mission_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="led_manager",
                name="led_manager",
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="safety_manager",
                name="safety_manager",
                parameters=[mission_config],
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="telemetry_bridge",
                name="telemetry_bridge",
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="audio_manager",
                name="audio_manager",
                output="screen",
            ),
            Node(
                package="medirun_mission",
                executable="remote_control_bridge",
                name="remote_control_bridge",
                output="screen",
            ),
        ]
    )
