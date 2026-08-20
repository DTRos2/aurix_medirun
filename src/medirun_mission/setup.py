from setuptools import find_packages, setup

package_name = "medirun_mission"

setup(
    name=package_name,
    version="0.0.1",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
        (f"share/{package_name}/launch", ["launch/mission_bringup.launch.py"]),
        (f"share/{package_name}/config", ["config/mission.yaml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="aurix",
    maintainer_email="team@example.com",
    description="Mission orchestration package for the MediRun project.",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "emergency_dispatcher = medirun_mission.emergency_dispatcher:main",
            "waypoint_manager = medirun_mission.waypoint_manager:main",
            "mission_manager = medirun_mission.mission_manager:main",
            "arrival_return_manager = medirun_mission.arrival_return_manager:main",
        ]
    },
)
