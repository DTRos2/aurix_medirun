from setuptools import find_packages, setup

package_name = "medirun_vision"

setup(
    name=package_name,
    version="0.0.1",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
        (
            f"share/{package_name}/launch",
            ["launch/vision_bringup.launch.py", "launch/parking.launch.py"],
        ),
        (
            f"share/{package_name}/config",
            ["config/docking.yaml", "config/parking.yaml"],
        ),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="aurix",
    maintainer_email="team@example.com",
    description="Vision package for the MediRun project.",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "docking_manager = medirun_vision.docking_manager:main",
            "aruco_detector_node = medirun_vision.aruco_detector_node:main",
        ]
    },
)
