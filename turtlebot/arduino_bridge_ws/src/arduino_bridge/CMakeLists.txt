cmake_minimum_required(VERSION 3.8)
project(arduino_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
# find_package(nav2_msgs REQUIRED)

add_executable(serial_bridge src/serial_bridge.cpp)
ament_target_dependencies(serial_bridge rclcpp std_msgs)

add_executable(estop_node src/estop_node.cpp)
ament_target_dependencies(estop_node rclcpp std_msgs geometry_msgs)

# add_executable(nav2_estop_node src/nav2_estop_node.cpp)
# ament_target_dependencies(nav2_estop_node
 #  rclcpp rclcpp_action std_msgs nav2_msgs geometry_msgs)

add_executable(led_status_node src/led_status_node.cpp)
ament_target_dependencies(led_status_node rclcpp std_msgs)

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
ament_target_dependencies(ultrasonic_safety_stop rclcpp std_msgs geometry_msgs)

install(TARGETS
  serial_bridge
  estop_node
 #  nav2_estop_node
  led_status_node
  ultrasonic_safety_stop
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
