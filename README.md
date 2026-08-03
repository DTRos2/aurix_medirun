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
| DB | PostgreSQL (로컬 인스턴스) |

DB 접속 정보 등 실값 설정은 커밋하지 않는다. `config/*.yaml.example`을 복사해 `config/*.yaml`로 만들고 값을 채운다.

```bash
cp config/db.yaml.example config/db.yaml
cp config/device.yaml.example config/device.yaml
```

## 폴더 구조

```
.
├── src/                    # ROS 2 패키지 (빌드 대상만 둔다)
│   ├── medirun_bringup/        # 전체 시스템 기동 launch (Nav2 + 대시보드)
│   ├── medirun_navigation/     # Nav2·AMCL, 이동 정밀도 시험
│   ├── medirun_mission/        # 미션 상태 관리 (구현 예정)
│   ├── medirun_sensor/         # 버튼·초음파·홀센서·LED 브리지 (구현 예정)
│   ├── medirun_vision/         # 카메라·ArUco 도킹 (구현 예정)
│   ├── medirun_gui/            # Qt(C++) 대시보드 (구현 중)
│   ├── medirun_db/             # PostgreSQL 연동 (구현 예정)
│   └── medirun_msgs/           # 커스텀 메시지·서비스·액션
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

```bash
source /opt/ros/humble/setup.bash
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

> `src/medirun_bringup/launch/camera.launch.py`(`v4l2_camera` 기반)는 UVC 웹캠 등 다른 하드웨어 대비용으로 남겨둔 미사용 파일이며, 실제 로봇에서는 쓰지 않는다.

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
