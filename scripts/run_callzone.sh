#!/bin/bash
# ESP32 콜버튼(MQTT) -> dispatch_request 까지 중계하는 편의 스크립트.
# 실제 웨이포인트 조회 + Nav2 주행은 대시보드(medirun_gui)의 MissionController가
# dispatch_request를 받아서 직접 수행한다 — 이 스크립트는 그 트리거만 만든다.
#
#   ./scripts/run.sh            # 먼저 다른 터미널에서 Nav2 + 대시보드부터 띄운다
#   ./scripts/run_callzone.sh   # 그 다음 이 스크립트로 MQTT 중계를 띄운다
#
# 전제: 대시보드(medirun_gui)가 이미 떠서 로그인까지 되어 있어야 한다.
# 대시보드가 없으면 버튼을 눌러도 dispatch_request만 나가고 로봇은 안 움직인다.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(dirname "${SCRIPT_DIR}")"

source /opt/ros/humble/setup.bash
source "${WORKSPACE_ROOT}/install/setup.bash"

exec ros2 launch medirun_bringup callzone_bringup.launch.py "$@"
