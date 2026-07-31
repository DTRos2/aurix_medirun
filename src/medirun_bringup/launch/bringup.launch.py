# Nav2(turtlebot3_navigation2 기반)와 medirun_gui 대시보드를 한 번에 띄우는
# 전체 시스템 기동 launch 파일.
#
# 사용 예:
#   ros2 launch medirun_bringup bringup.launch.py
#   ros2 launch medirun_bringup bringup.launch.py map:=/path/other.yaml
#   ros2 launch medirun_bringup bringup.launch.py use_rviz:=true
#
# 전제:
#   - 로봇 본체 드라이버(turtlebot3_bringup robot.launch.py 등, 라이다·모터)는
#     이 launch가 아니라 로봇(Pi) 쪽에서 별도로 이미 떠 있어야 한다.
#   - TURTLEBOT3_MODEL 환경변수가 실행 전 export 되어 있어야 한다 (예: burger).
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    EmitEvent,
    IncludeLaunchDescription,
    RegisterEventHandler,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.events import Shutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

TURTLEBOT3_MODEL = os.environ.get("TURTLEBOT3_MODEL", "burger")
ROS_DISTRO = os.environ.get("ROS_DISTRO", "humble")

# 이 파일은 --symlink-install로 설치되므로 install/ 아래의 심볼릭 링크를 통해
# 실행된다. abspath는 심볼릭 링크를 따라가지 않아 install 경로로 잘못
# 계산되므로, realpath로 실제 소스 트리 경로(src/medirun_bringup/launch/...)를
# 구해야 한다. 계정 경로를 하드코딩하지 않기 위해 여기서부터 워크스페이스
# 루트를 상대 경로로 찾는다 (CONVENTIONS.md 3.2절: 장비별 절대 경로 하드코딩 금지).
THIS_DIR = os.path.dirname(os.path.realpath(__file__))
WORKSPACE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))
DEFAULT_MAP = os.path.join(WORKSPACE_ROOT, "maps", "last_class_map_modi.yaml")


def generate_launch_description():
    map_yaml = LaunchConfiguration("map")
    use_sim_time = LaunchConfiguration("use_sim_time")
    use_rviz = LaunchConfiguration("use_rviz")
    use_gui = LaunchConfiguration("use_gui")

    param_file_name = TURTLEBOT3_MODEL + ".yaml"
    if ROS_DISTRO == "humble":
        param_dir = os.path.join(
            get_package_share_directory("turtlebot3_navigation2"), "param", ROS_DISTRO, param_file_name
        )
    else:
        param_dir = os.path.join(
            get_package_share_directory("turtlebot3_navigation2"), "param", param_file_name
        )

    nav2_launch_file_dir = os.path.join(get_package_share_directory("nav2_bringup"), "launch")
    rviz_config_dir = os.path.join(
        get_package_share_directory("turtlebot3_navigation2"), "rviz", "tb3_navigation2.rviz"
    )

    gui_node = Node(
        package="medirun_gui",
        executable="medirun_gui_app",
        name="medirun_gui",
        output="screen",
        condition=IfCondition(use_gui),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("map", default_value=DEFAULT_MAP, description="불러올 occupancy map YAML 경로"),
            DeclareLaunchArgument("use_sim_time", default_value="false", description="Gazebo 시뮬레이션이면 true"),
            DeclareLaunchArgument(
                "use_rviz",
                default_value="false",
                description="RViz도 같이 띄울지 여부 (대시보드에 초기 위치 설정 기능이 있어 기본 false)",
            ),
            DeclareLaunchArgument(
                "use_gui", default_value="true", description="medirun_gui 대시보드를 자동으로 같이 띄울지 여부"
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource([nav2_launch_file_dir, "/bringup_launch.py"]),
                launch_arguments={
                    "map": map_yaml,
                    "use_sim_time": use_sim_time,
                    "params_file": param_dir,
                }.items(),
            ),
            Node(
                package="rviz2",
                executable="rviz2",
                name="rviz2",
                arguments=["-d", rviz_config_dir],
                parameters=[{"use_sim_time": use_sim_time}],
                output="screen",
                condition=IfCondition(use_rviz),
            ),
            # Nav2가 완전히 뜨기 전에 대시보드가 먼저 열려도 동작에는 문제 없다
            # (액션 서버가 아직 없으면 버튼 클릭 시 이벤트 로그에 에러가 남고,
            #  Nav2가 뜬 뒤 다시 누르면 정상 동작한다). 화면이 바로 나오도록
            # 약간의 지연만 둔다.
            TimerAction(
                period=2.0,
                actions=[gui_node],
            ),
            # 대시보드 창을 닫으면(창 X 또는 "대시보드 종료" 버튼) medirun_gui
            # 프로세스만 끝나고 Nav2 스택(component_container_isolated 등)은
            # ros2 launch가 기본적으로 계속 살려둔다. "대시보드를 닫으면 전체
            # 시스템도 같이 내려간다"는 기대에 맞춰, GUI 프로세스 종료를 감지해
            # launch 전체에 정상 종료(SIGINT와 동일한 경로)를 걸어준다.
            # use_gui:=false로 GUI 자체를 안 띄운 경우엔 이 이벤트가 그냥
            # 발생하지 않는다(무해).
            RegisterEventHandler(
                OnProcessExit(
                    target_action=gui_node,
                    on_exit=[EmitEvent(event=Shutdown(reason="medirun_gui 대시보드가 종료되었습니다."))],
                )
            ),
        ]
    )
