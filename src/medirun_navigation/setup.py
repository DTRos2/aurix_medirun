from setuptools import find_packages, setup

package_name = "medirun_navigation"

setup(
    name=package_name,
    version="0.0.1",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
        (
            f"share/{package_name}/launch",
            [
                "launch/navigation_bringup.launch.py",
                "launch/navigation_rotation_test.launch.py",
                "launch/navigation_straight_test.launch.py",
            ],
        ),
        (
            f"share/{package_name}/config",
            [
                "config/navigation_runtime.yaml",
                "config/navigation_rotation_test.yaml",
                "config/navigation_straight_test.yaml",
            ],
        ),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="aurix",
    maintainer_email="team@example.com",
    description="Navigation package for the MediRun project.",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "rotation_test_runner = medirun_navigation.rotation_test_runner:main",
            "straight_test_runner = medirun_navigation.straight_test_runner:main",
            "navigation_manager = medirun_navigation.navigation_manager:main",
        ]
    },
)
