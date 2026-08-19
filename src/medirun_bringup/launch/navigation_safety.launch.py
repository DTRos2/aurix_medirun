# Nav2 Humble navigation_launch.py를 MediRun 초음파 안전 게이트에 맞게 조정한 파일.
#
# 핵심 차이:
#   1) velocity_smoother의 최종 출력 -> /medirun/navigation/cmd_vel_raw
#   2) behavior_server의 후진/회전 출력 -> /medirun/navigation/cmd_vel_raw
#
# 따라서 일반 경로 주행뿐 아니라 BackUp/Spin 복구 동작도 반드시
# ultrasonic_safety_stop을 거쳐 최종 /cmd_vel로 전달된다.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, SetEnvironmentVariable
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import LoadComposableNodes, Node
from launch_ros.descriptions import ComposableNode, ParameterFile
from nav2_common.launch import RewrittenYaml


RAW_CMD_VEL_TOPIC = "/medirun/navigation/cmd_vel_raw"


def generate_launch_description():
    bringup_dir = get_package_share_directory("nav2_bringup")
    # burger.yaml의 default_bt_xml_filename이 실제 존재하지 않는 파일명이라
    # Nav2 기본 BT(Spin 복구 각도 90도)로 조용히 대체되고 있었다. 트레일러엔
    # 너무 큰 복구 회전이라 spin_dist를 줄인 커스텀 BT로 덮어쓴다(2026-08-19).
    gentle_recovery_bt = os.path.join(
        get_package_share_directory("medirun_bringup"), "config", "navigate_w_gentle_recovery.xml"
    )

    namespace = LaunchConfiguration("namespace")
    use_sim_time = LaunchConfiguration("use_sim_time")
    autostart = LaunchConfiguration("autostart")
    params_file = LaunchConfiguration("params_file")
    use_composition = LaunchConfiguration("use_composition")
    container_name = LaunchConfiguration("container_name")
    container_name_full = (namespace, "/", container_name)
    use_respawn = LaunchConfiguration("use_respawn")
    log_level = LaunchConfiguration("log_level")

    lifecycle_nodes = [
        "controller_server",
        "smoother_server",
        "planner_server",
        "behavior_server",
        "bt_navigator",
        "waypoint_follower",
        "velocity_smoother",
    ]

    remappings = [("/tf", "tf"), ("/tf_static", "tf_static")]
    behavior_remappings = remappings + [("cmd_vel", RAW_CMD_VEL_TOPIC)]
    velocity_smoother_remappings = remappings + [
        ("cmd_vel", "cmd_vel_nav"),
        ("cmd_vel_smoothed", RAW_CMD_VEL_TOPIC),
    ]

    configured_params = ParameterFile(
        RewrittenYaml(
            source_file=params_file,
            root_key=namespace,
            param_rewrites={
                "use_sim_time": use_sim_time,
                "autostart": autostart,
                "default_bt_xml_filename": gentle_recovery_bt,
            },
            convert_types=True,
        ),
        allow_substs=True,
    )

    stdout_linebuf_envvar = SetEnvironmentVariable("RCUTILS_LOGGING_BUFFERED_STREAM", "1")

    load_nodes = GroupAction(
        condition=IfCondition(PythonExpression(["not ", use_composition])),
        actions=[
            Node(
                package="nav2_controller",
                executable="controller_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=remappings + [("cmd_vel", "cmd_vel_nav")],
            ),
            Node(
                package="nav2_smoother",
                executable="smoother_server",
                name="smoother_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=remappings,
            ),
            Node(
                package="nav2_planner",
                executable="planner_server",
                name="planner_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=remappings,
            ),
            Node(
                package="nav2_behaviors",
                executable="behavior_server",
                name="behavior_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=behavior_remappings,
            ),
            Node(
                package="nav2_bt_navigator",
                executable="bt_navigator",
                name="bt_navigator",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=remappings,
            ),
            Node(
                package="nav2_waypoint_follower",
                executable="waypoint_follower",
                name="waypoint_follower",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=remappings,
            ),
            Node(
                package="nav2_velocity_smoother",
                executable="velocity_smoother",
                name="velocity_smoother",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=velocity_smoother_remappings,
            ),
            Node(
                package="nav2_lifecycle_manager",
                executable="lifecycle_manager",
                name="lifecycle_manager_navigation",
                output="screen",
                arguments=["--ros-args", "--log-level", log_level],
                parameters=[
                    {"use_sim_time": use_sim_time},
                    {"autostart": autostart},
                    {"node_names": lifecycle_nodes},
                ],
            ),
        ],
    )

    load_composable_nodes = LoadComposableNodes(
        condition=IfCondition(use_composition),
        target_container=container_name_full,
        composable_node_descriptions=[
            ComposableNode(
                package="nav2_controller",
                plugin="nav2_controller::ControllerServer",
                name="controller_server",
                parameters=[configured_params],
                remappings=remappings + [("cmd_vel", "cmd_vel_nav")],
            ),
            ComposableNode(
                package="nav2_smoother",
                plugin="nav2_smoother::SmootherServer",
                name="smoother_server",
                parameters=[configured_params],
                remappings=remappings,
            ),
            ComposableNode(
                package="nav2_planner",
                plugin="nav2_planner::PlannerServer",
                name="planner_server",
                parameters=[configured_params],
                remappings=remappings,
            ),
            ComposableNode(
                package="nav2_behaviors",
                plugin="behavior_server::BehaviorServer",
                name="behavior_server",
                parameters=[configured_params],
                remappings=behavior_remappings,
            ),
            ComposableNode(
                package="nav2_bt_navigator",
                plugin="nav2_bt_navigator::BtNavigator",
                name="bt_navigator",
                parameters=[configured_params],
                remappings=remappings,
            ),
            ComposableNode(
                package="nav2_waypoint_follower",
                plugin="nav2_waypoint_follower::WaypointFollower",
                name="waypoint_follower",
                parameters=[configured_params],
                remappings=remappings,
            ),
            ComposableNode(
                package="nav2_velocity_smoother",
                plugin="nav2_velocity_smoother::VelocitySmoother",
                name="velocity_smoother",
                parameters=[configured_params],
                remappings=velocity_smoother_remappings,
            ),
            ComposableNode(
                package="nav2_lifecycle_manager",
                plugin="nav2_lifecycle_manager::LifecycleManager",
                name="lifecycle_manager_navigation",
                parameters=[
                    {
                        "use_sim_time": use_sim_time,
                        "autostart": autostart,
                        "node_names": lifecycle_nodes,
                    }
                ],
            ),
        ],
    )

    return LaunchDescription(
        [
            stdout_linebuf_envvar,
            DeclareLaunchArgument("namespace", default_value="", description="Top-level namespace"),
            DeclareLaunchArgument(
                "use_sim_time", default_value="false", description="Use simulation (Gazebo) clock if true"
            ),
            DeclareLaunchArgument(
                "params_file",
                default_value=os.path.join(bringup_dir, "params", "nav2_params.yaml"),
                description="Full path to the ROS2 parameters file",
            ),
            DeclareLaunchArgument("autostart", default_value="true", description="Automatically startup Nav2"),
            DeclareLaunchArgument(
                "use_composition", default_value="False", description="Use composed bringup if True"
            ),
            DeclareLaunchArgument(
                "container_name", default_value="nav2_container", description="Nav2 component container name"
            ),
            DeclareLaunchArgument(
                "use_respawn", default_value="False", description="Respawn nodes when composition is disabled"
            ),
            DeclareLaunchArgument("log_level", default_value="info", description="Log level"),
            load_nodes,
            load_composable_nodes,
        ]
    )
