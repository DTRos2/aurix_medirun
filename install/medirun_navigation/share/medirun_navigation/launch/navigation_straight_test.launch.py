from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterFile
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    return LaunchDescription(
        [
            Node(
                package="medirun_navigation",
                executable="straight_test_runner",
                name="straight_test_runner",
                output="screen",
                parameters=[
                    ParameterFile(
                        PathJoinSubstitution(
                            [
                                FindPackageShare("medirun_navigation"),
                                "config",
                                "navigation_straight_test.yaml",
                            ]
                        ),
                        allow_substs=True,
                    )
                ],
            )
        ]
    )
