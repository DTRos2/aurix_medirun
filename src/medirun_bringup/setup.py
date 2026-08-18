from setuptools import find_packages, setup

package_name = "medirun_bringup"

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
                "launch/.gitkeep",
                "launch/bringup.launch.py",
                "launch/navigation_safety.launch.py",
                "launch/camera.launch.py",
                "launch/callzone_bringup.launch.py",
            ],
        ),
        (f"share/{package_name}/config", ["config/.gitkeep"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="aurix",
    maintainer_email="team@example.com",
    description="Bringup package for the MediRun project.",
    license="Apache-2.0",
    tests_require=["pytest"],
)
