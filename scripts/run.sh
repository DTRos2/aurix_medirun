#!/bin/bash
# Nav2 + medirun_gui 대시보드를 한 번에 띄우는 편의 스크립트.
#
#   ./scripts/run.sh                       # 기본값으로 실행 (map: maps/last_class_map_modi.yaml)
#   ./scripts/run.sh map:=/path/other.yaml # 다른 맵 사용
#   ./scripts/run.sh use_rviz:=true        # RViz도 같이 띄우기
#
# 전제: 로봇 본체 드라이버(turtlebot3_bringup robot.launch.py 등)는 로봇(Pi)
# 쪽에서 이미 별도로 떠 있어야 한다. 이 스크립트는 Nav2 스택 + 대시보드만 띄운다.
#
# 이 스크립트가 하는 일:
#   1. ROS2 humble + 이 워크스페이스 install 환경 source
#      (turtlebot3, turtlebot3_msgs는 src/에 벤더링되어 있어 이 워크스페이스
#       install만 source하면 됨 — 팀원 개인 ~/turtlebot3_ws 불필요)
#   2. TURTLEBOT3_MODEL이 안 정해져 있으면 burger로 기본 설정
#   3. medirun_bringup의 bringup.launch.py 실행 (인자는 그대로 전달)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "${SCRIPT_DIR}")"

source /opt/ros/humble/setup.bash
source "${WORKSPACE_ROOT}/install/setup.bash"

: "${TURTLEBOT3_MODEL:=burger}"
export TURTLEBOT3_MODEL

exec ros2 launch medirun_bringup bringup.launch.py "$@"
