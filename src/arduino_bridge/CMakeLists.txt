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

add_library(ultrasonic_safety_policy
  src/ultrasonic_safety_policy.cpp
)
target_include_directories(ultrasonic_safety_policy PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(ultrasonic_safety_policy PUBLIC cxx_std_17)

add_executable(ultrasonic_safety_stop src/ultrasonic_safety_stop.cpp)
target_link_libraries(ultrasonic_safety_stop ultrasonic_safety_policy)
target_compile_features(ultrasonic_safety_stop PUBLIC cxx_std_17)
ament_target_dependencies(ultrasonic_safety_stop rclcpp geometry_msgs std_msgs)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_ultrasonic_safety_policy
    test/test_ultrasonic_safety_policy.cpp
  )
  target_link_libraries(test_ultrasonic_safety_policy ultrasonic_safety_policy)
endif()

install(TARGETS
  serial_bridge
  led_status_node
  estop_node
  ps4_teleop_node
  bringup_monitor_node
  ultrasonic_safety_stop
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS ultrasonic_safety_policy
  EXPORT export_ultrasonic_safety_policy
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(PROGRAMS
  scripts/mqtt_call_button_bridge.py
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_export_targets(export_ultrasonic_safety_policy HAS_LIBRARY_TARGET)
ament_export_include_directories(include)

ament_package()
