from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterFile
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    repeat_count = LaunchConfiguration("repeat_count")
    command_compensation_deg = LaunchConfiguration("command_compensation_deg")
    report_output_dir = LaunchConfiguration("report_output_dir")
    test_id = LaunchConfiguration("test_id")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "repeat_count",
                default_value="1",
                description="Number of rotation test repetitions.",
            ),
            DeclareLaunchArgument(
                "command_compensation_deg",
                default_value="0.0",
                description="Extra rotation compensation in degrees applied to each command.",
            ),
            DeclareLaunchArgument(
                "report_output_dir",
                default_value="docs/tests",
                description="Directory where markdown reports are saved.",
            ),
            DeclareLaunchArgument(
                "test_id",
                default_value="T-00",
                description="Test ID used for the report filename.",
            ),
            Node(
                package="medirun_navigation",
                executable="rotation_test_runner",
                name="rotation_test_runner",
                output="screen",
                parameters=[
                    ParameterFile(
                        PathJoinSubstitution(
                            [
                                FindPackageShare("medirun_navigation"),
                                "config",
                                "navigation_rotation_test.yaml",
                            ]
                        ),
                        allow_substs=True,
                    ),
                    {
                        "repeat_count": repeat_count,
                        "command_compensation_deg": command_compensation_deg,
                        "report_output_dir": report_output_dir,
                        "test_id": test_id,
                    },
                ],
            ),
        ]
    )
