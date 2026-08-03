cmake_minimum_required(VERSION 3.8)
project(medirun_mission LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_library(medirun_mission_utils STATIC
  src/json_utils.cpp
  src/waypoint_store.cpp
)
target_compile_features(medirun_mission_utils PUBLIC cxx_std_17)
target_include_directories(medirun_mission_utils PUBLIC include)

function(add_mission_node target_name source_file)
  add_executable(${target_name} ${source_file})
  target_compile_features(${target_name} PUBLIC cxx_std_17)
  target_include_directories(${target_name} PUBLIC include)
  ament_target_dependencies(${target_name} geometry_msgs rclcpp std_msgs)
  target_link_libraries(${target_name} medirun_mission_utils)
  install(TARGETS ${target_name} DESTINATION lib/${PROJECT_NAME})
endfunction()

add_mission_node(audio_manager src/audio_manager.cpp)
add_mission_node(emergency_dispatcher src/emergency_dispatcher.cpp)
add_mission_node(led_manager src/led_manager.cpp)
add_mission_node(mission_manager src/mission_manager.cpp)
add_mission_node(payload_monitor src/payload_monitor.cpp)
add_mission_node(remote_control_bridge src/remote_control_bridge.cpp)
add_mission_node(safety_manager src/safety_manager.cpp)
add_mission_node(telemetry_bridge src/telemetry_bridge.cpp)
add_mission_node(waypoint_manager src/waypoint_manager.cpp)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
