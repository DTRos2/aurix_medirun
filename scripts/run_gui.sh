#!/bin/bash
# PC 대시보드(medirun_gui) + 콜존 파이프라인(mqtt_bridge, emergency_dispatcher)을
# 한 번에 띄우는 편의 스크립트.
#
#   ./scripts/run_gui.sh
#
# 전제: 로봇(Pi) 쪽에서 pi_bringup.launch.py가 이미 떠 있어야 한다. Nav2/안전정지/
# 파킹/아두이노 브리지는 전부 Pi가 전담하므로 이 스크립트는 절대 안 띄운다.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "${SCRIPT_DIR}")"

source /opt/ros/humble/setup.bash
source "${WORKSPACE_ROOT}/install/setup.bash"

exec ros2 launch medirun_bringup pc_bringup.launch.py "$@"
