cmake_minimum_required(VERSION 3.8)
project(arduino_bridge LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2_msgs REQUIRED)

add_executable(serial_bridge
  src/serial_bridge.cpp
)
target_compile_features(serial_bridge PUBLIC cxx_std_17)
ament_target_dependencies(serial_bridge rclcpp std_msgs)

add_executable(led_status_node
  src/led_status_node.cpp
)
target_compile_features(led_status_node PUBLIC cxx_std_17)
ament_target_dependencies(led_status_node rclcpp std_msgs)

add_executable(estop_node
  src/estop_node.cpp
)
target_compile_features(estop_node PUBLIC cxx_std_17)
ament_target_dependencies(estop_node rclcpp std_msgs geometry_msgs)

add_executable(ps4_teleop_node
  src/ps4_teleop_node.cpp
)
target_compile_features(ps4_teleop_node PUBLIC cxx_std_17)
ament_target_dependencies(ps4_teleop_node rclcpp geometry_msgs sensor_msgs)

add_executable(bringup_monitor_node
  src/bringup_monitor_node.cpp
)
target_compile_features(bringup_monitor_node PUBLIC cxx_std_17)
ament_target_dependencies(bringup_monitor_node rclcpp geometry_msgs nav_msgs sensor_msgs std_msgs tf2_msgs)

install(TARGETS
  serial_bridge
  led_status_node
  estop_node
  ps4_teleop_node
  bringup_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
