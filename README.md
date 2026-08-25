# aurix_main_project_01
This repository is used for ROS 2 classes and serves as a workspace for the aurix team.

## Setup

Clone 직후 한 번만 실행해 Git 훅(커밋 규칙 검증)을 활성화한다.

```bash
./scripts/setup-git-hooks.sh
```

- 커밋/브랜치 규칙: [`COMMIT_RULES.md`](COMMIT_RULES.md)
- 네이밍·구조 컨벤션: [`CONVENTIONS.md`](CONVENTIONS.md)

## 환경

| 항목 | 버전/구성 |
|---|---|
| OS | Ubuntu 22.04 LTS |
| ROS 2 | Humble |
| 빌드 도구 | colcon |
| Python | 3.10 (`ament_python` 패키지) |
| GUI | Qt6 (C++), `ament_cmake` |
| DB | PostgreSQL 16 (Docker Compose) |

DB 접속 정보 등 실값 설정은 커밋하지 않는다. `config/*.yaml.example`을 복사해 `config/*.yaml`로 만들고 값을 채운다.

```bash
cp config/db.yaml.example config/db.yaml
cp config/device.yaml.example config/device.yaml
```

## 폴더 구조



### Raspberry Pi 4

```
~/arduino_bridge_ws/                          ← ② include 대상
├── src/arduino_bridge/
│   ├── launch/
│   │   └── robot_with_arduino.launch.py      
│   ├── src/
│   │   ├── serial_bridge.cpp                 → serial_bridge
│   │   ├── led_status_node.cpp               → led_status_node
│   │   ├── estop_node.cpp                    → estop_node
│   │   ├── ultrasonic_safety_policy.cpp      → ultrasonic_node
│   │   └── ultrasonic_safety_stop.cpp        → ultrasonic_node
│   ├── CMakeLists.txt
│   └── package.xml
├── firmware/estop_firmware/estop_firmware.ino   (Pi는 한 겹 더)
└── install/arduino_bridge/share/arduino_bridge/launch/
    └── robot_with_arduino.launch.py          ← 실제 실행되는 사본(심볼릭이어야 함)

~/turtlebot3_ws/                              ← ② include 대상
└── install/turtlebot3_bringup/share/turtlebot3_bringup/launch/
    ├── robot.launch.py                       ← usb_port:=opencr_port
    └── camera.launch.py                      ← format:=camera_format ★ 이번에 추가
└── install/coin_d4_driver/share/coin_d4_driver/params/
    └── single_lidar_node.yaml                (robot.launch.py가 참조)
└── src/turtlebot3/turtlebot3_bringup/config/
    └── tb3_burger_picam2_640x480.yaml        (camera_info_url 기본값)

~/medirun_ws/                                ← ① 실행 진입점
└── src/
    ├── medirun_bringup/                     ←  aurix_main_project_01/src/medirun_bringup
    │   ├── launch/
    │   └──   └── pi_bringup.launch.py       ← 실행
    └── medirun_mission/                     ←  aurix_main_project_01/src/medirun_mission

~/turtlebot3_ws/install/turtlebot3_navigation2/share/turtlebot3_navigation2/param/humble/burger.yaml
← aurix_main_project_01/src/turtlebot3/turtlebot3_navigation2/param/humble/burger.yaml
```




### Ubuntu
```
.
├── src/                    # ROS 2 패키지 (빌드 대상만 둔다)
│   ├── medirun_bringup/        # 전체 시스템 기동 launch (Nav2 + 대시보드)
│   ├── medirun_navigation/     # Nav2·AMCL, 이동 정밀도 시험
│   ├── medirun_mission/        # 미션 상태 관리, 도착·반출·복귀(arrival_return_manager)
│   ├── medirun_sensor/         # 버튼·초음파·홀센서·LED 브리지, MQTT 버튼(mqtt_bridge)
│   ├── medirun_vision/         # 카메라·ArUco 도킹
│   ├── medirun_gui/            # Qt(C++) 대시보드
│   ├── medirun_db/             # PostgreSQL 연동, 로그 보관(log_retention)
│   ├── medirun_msgs/           # 커스텀 메시지·서비스·액션
│   ├── arduino_bridge/         # 로봇(Pi) 실행 코드 팀 공유용 — 아래 "아두이노 브리지" 참고
│   ├── turtlebot3/             # ROBOTIS 공식 패키지 벤더링(humble) — bringup·cartographer·navigation2 등
│   └── turtlebot3_msgs/        # ROBOTIS 공식 메시지 벤더링(humble)
├── firmware/                # Arduino/ESP32 펌웨어
├── maps/                    # 지도(.pgm/.yaml), 웨이포인트 정의
├── config/                  # DB·장비 설정 (`.example`만 커밋)
├── db/                      # PostgreSQL 스키마·마이그레이션
├── docs/                    # 제출 문서, 시험표
│   ├── tests/                   # 시험 기록 문서(.md)·측정 원본(data/*.csv)
│   ├── dashboard_wireframe/     # Qt 대시보드 와이어프레임
│   └── screenshots/gui/         # Qt 대시보드 화면 캡처
├── scripts/                 # 기동·자동 실행 스크립트
│   └── analysis/                # 측정값 사후 분석 도구 (ROS 비의존)
├── COMMIT_RULES.md          # Git 브랜치·커밋 규칙
├── CONVENTIONS.md           # 네이밍·구조 컨벤션
└── README.md
```

## 빌드

`src/turtlebot3`, `src/turtlebot3_msgs`가 이 저장소에 벤더링되어 있어 별도로
`~/turtlebot3_ws` 같은 개인 워크스페이스를 미리 만들어둘 필요가 없다. `rosdep`으로
시스템 의존성(cartographer_ros, nav2 등)만 한 번 설치하면 된다.

```bash
source /opt/ros/humble/setup.bash
rosdep install --from-paths src --ignore-src -r -y   # 처음 한 번만
# 현재 인터페이스가 빠져있기 때문에 (--packages-skip medirun_msgs) 추가해주어야 함
colcon build --symlink-install --packages-skip medirun_msgs
source install/setup.bash
```

## 실행

### 메인 (전체 시스템 기동)

로봇 본체 드라이버(`turtlebot3_bringup robot.launch.py` 등, 라이다·모터)는 로봇(Pi) 쪽에서 별도로 이미 떠 있어야 한다. 그 위에서 Nav2 스택과 `medirun_gui` 대시보드를 한 번에 띄운다.

```bash
./scripts/run.sh
# 또는 직접
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch medirun_bringup bringup.launch.py
```

기본 맵은 `maps/last_class_map_modi.yaml`이며, `map:=/path/other.yaml`로 바꿀 수 있다. RViz를 같이 띄우려면 `use_rviz:=true`, 대시보드 없이 Nav2만 띄우려면 `use_gui:=false`를 붙인다.

### 카메라 (Pi에서 별도 실행)

로봇 본체 드라이버와 마찬가지로 Pi 쪽에서 직접 띄운다. 로봇 카메라는 CSI 카메라(IMX219)라 `v4l2_camera`가 아니라 `libcamera` 기반 `camera_ros`(`turtlebot3_bringup` 소유 launch)로 구동하며, `/camera/image_raw`(+ `compressed`)를 발행하면 원격 제어 페이지의 카메라 화면이 이를 구독한다.

```bash
source /opt/ros/humble/setup.bash
ros2 launch turtlebot3_bringup camera.launch.py format:=BGR888
```

카메라 launch는 `turtlebot3_bringup/config/cyclonedds_udp.xml`을 자동 적용해
CycloneDDS IPv4 UDP 전송을 사용한다. 이미지 데이터는 JPEG 압축 토픽을
`BEST_EFFORT`, depth 1로 구독하므로 네트워크가 잠시 밀려도 과거 프레임을
쌓지 않고 최신 프레임을 우선한다. Pi 쪽 소스를 갱신한 뒤에는
`turtlebot3_bringup`을 다시 빌드하고 `install/setup.bash`를 source해야 한다.

> `src/medirun_bringup/launch/camera.launch.py`(`v4l2_camera` 기반)는 UVC 웹캠 등 다른 하드웨어 대비용으로 남겨둔 미사용 파일이며, 실제 로봇에서는 쓰지 않는다.


### DB (PostgreSQL, Docker)

Docker / Docker Compose가 설치되어 있어야 한다. 최초 1회만 아래처럼 실행하면 `db/schema/postgresql_schema.sql`이 자동 적용된다.

```bash
cp .env.example .env   # 필요시 값 수정
docker compose up -d
```

```bash
docker exec aurix_postgres psql -U aurix -d aurix_medirun_db -c "\dt"   # 테이블 목록 확인
```

> `db/schema/postgresql_schema.sql`은 컨테이너를 **처음 만들 때(빈 볼륨일 때)만** 자동 실행된다. 스키마를 수정한 뒤 다시 적용하려면 기존 데이터를 지우고 재기동해야 한다: `docker compose down -v && docker compose up -d` (`-v`가 볼륨까지 삭제하므로 기존 데이터가 날아간다).

### 아두이노 브리지 (Pi 실행 코드 공유용)

`src/arduino_bridge/`는 로봇(Pi)에서 도는 코드(시리얼 브리지, LED 상태, e-stop, PS4 텔레옵 등)를 팀원끼리 공유·관리하기 위한 폴더다. **실제 Pi는 이 저장소가 아니라 별도 워크스페이스(`~/arduino_bridge_ws`)에서 실행하므로, 두 쪽이 수시로 어긋날 수 있다.** Pi 쪽을 고치면 이 폴더에도 반영하고, 반대로 이 폴더를 고쳤으면 Pi에도 동기화하는 걸 잊지 않는다.

**Pi와 GUI를 실행하는 PC 양쪽 모두 `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`로 통일**해서 쓴다 (각 머신의 `~/.bashrc`에 추가 권장). 서로 다른 DDS 벤더를 섞어 쓰면 노드 디스커버리가 꼬이거나, Fast-DDS 기본 설정에서는 Cartographer의 내부 상태 토픽이 커질 때 메시지가 드롭되는 문제가 있다.


### GUI (Qt 대시보드 단독 실행)

`medirun_gui`는 `ament_cmake` 패키지로, 다른 `medirun_*` 패키지와 동일하게 colcon으로 빌드·실행한다. Qt6 6.5 이상이 필요하며, 시스템 Qt 버전이 더 낮다면 `--cmake-args -DCMAKE_PREFIX_PATH=<Qt6.5+ 설치 경로>`로 지정한다.

```bash
source /opt/ros/humble/setup.bash
colcon build --packages-select medirun_gui --symlink-install
source install/setup.bash
ros2 run medirun_gui medirun_gui_app
```

소스만 변경했다면 `colcon build --packages-select medirun_gui`만 다시 실행한 뒤 위와 동일하게 실행한다.

### 테스트 (이동 정밀도 시험)

`medirun_navigation`의 회전/직진 정밀도 시험 러너를 실행한다. 결과는 `docs/tests/T-XX_*.md`에 markdown 보고서로 저장된다.

```bash
# 회전 정확도 (T-00)
ros2 launch medirun_navigation navigation_rotation_test.launch.py \
  repeat_count:=10 test_id:=T-00

# 직진 정확도 (T-01)
ros2 launch medirun_navigation navigation_straight_test.launch.py \
  repeat_count:=10 distance_m:=0.45 test_id:=T-01
```

주요 파라미터는 `src/medirun_navigation/config/navigation_*_test.yaml`에서 조정한다.

## 시험 결과

| 시험 ID | 항목 | 결과(절대평균오차) | 기준 | 판정 |
|---|---|---|---|---|
| T-00 | 회전 정확도 (90°, 10회) | 0.13° | ≤ 2° | 충족 |
| T-01 | 직진 정확도 (0.45m, 10회) | 0.2cm | ≤ 1cm | 충족 |

상세 결과: [`docs/tests/T-00_rotation_accuracy_summary.docx`](docs/tests/T-00_rotation_accuracy_summary.docx), [`docs/tests/T-01_straight_accuracy_summary.docx`](docs/tests/T-01_straight_accuracy_summary.docx)
