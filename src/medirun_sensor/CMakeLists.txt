cmake_minimum_required(VERSION 3.8)
project(medirun_sensor LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_library(serial_utils STATIC
  src/serial_utils.cpp
)
target_compile_features(serial_utils PUBLIC cxx_std_17)
target_include_directories(serial_utils PUBLIC include)

add_executable(arduino_bridge src/arduino_bridge.cpp)
target_compile_features(arduino_bridge PUBLIC cxx_std_17)
target_include_directories(arduino_bridge PUBLIC include)
ament_target_dependencies(arduino_bridge rclcpp std_msgs)
target_link_libraries(arduino_bridge serial_utils)

add_executable(sos_bridge src/sos_bridge.cpp)
target_compile_features(sos_bridge PUBLIC cxx_std_17)
target_include_directories(sos_bridge PUBLIC include)
ament_target_dependencies(sos_bridge rclcpp std_msgs)
target_link_libraries(sos_bridge serial_utils)

add_executable(ready_notifier src/ready_notifier.cpp)
target_compile_features(ready_notifier PUBLIC cxx_std_17)
target_include_directories(ready_notifier PUBLIC include)
ament_target_dependencies(ready_notifier rclcpp std_msgs)

install(TARGETS
  arduino_bridge
  ready_notifier
  sos_bridge
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
