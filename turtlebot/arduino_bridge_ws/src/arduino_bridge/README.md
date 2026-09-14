# Arduino Bridge

Arduino 센서·버튼·LED와 TurtleBot3의 주행 장치, LiDAR, 음성 출력을 ROS 2에서 함께 사용하기 위한 패키지입니다.

## 통합 실행

```bash
ros2 launch arduino_bridge robot_with_arduino.launch.py
```

이 명령은 `robot_with_arduino.launch.py`를 실행하여 TurtleBot3와 Arduino 주변 장치를 한 번에 구동합니다. 별도의 터미널에서 각 노드를 하나씩 실행할 필요가 없습니다.

### 실행되는 구성

| 구성 | 역할 |
| --- | --- |
| TurtleBot3 `robot.launch.py` | OpenCR 모터 제어, 로봇 상태 퍼블리셔, LiDAR 드라이버를 실행합니다. |
| `serial_bridge` | Arduino와 시리얼로 통신하며 버튼·거리 센서·홀 센서 값을 ROS 2 토픽으로 변환하고, LED·부저 명령을 Arduino로 전달합니다. |
| `led_status_node` | 비상정지 버튼과 `/robot_status`를 확인하여 Arduino LED 상태를 `IDLE`, `RUN`, `STOP`으로 변경합니다. |
| `estop_node` | `BTN_01`이 입력될 때마다 비상정지를 활성화/해제합니다. 활성화 중에는 정지 명령을 `/cmd_vel`에 20 Hz로 발행합니다. |
| `audio_output_node` | `/audio/command`로 들어온 음성 또는 효과음 명령을 지정한 PulseAudio 출력 장치로 재생합니다. |

실행 직후에는 다음 초기 동작도 한 번 수행됩니다.

- Arduino LED에 `IDLE` 명령을 보내 대기 상태로 설정합니다.
- 스피커로 **“주행을 시작합니다”** 안내 음성을 재생합니다.

> `BTN_01`은 토글 방식입니다. 한 번 누르면 비상정지가 활성화되고, 다시 누르면 해제됩니다.

## 주요 토픽

### Arduino에서 ROS 2로 전달

| 토픽 | 메시지 형식 | 내용 |
| --- | --- | --- |
| `/arduino/button` | `std_msgs/msg/String` | 버튼 입력 (`BTN_01`, `BTN_02`, `BTN_03`) |
| `/arduino/front_distance_cm` | `std_msgs/msg/Int32` | 전방 거리 센서 값(cm) |
| `/arduino/rear_distance_cm` | `std_msgs/msg/Int32` | 후방 거리 센서 값(cm) |
| `/arduino/payload_loaded` | `std_msgs/msg/Bool` | 홀 센서 기반 적재 여부 |
| `/arduino/status` | `std_msgs/msg/String` | Arduino 준비, LED 응답, 비상정지 등의 상태 |

### ROS 2에서 Arduino 및 장치로 전달

| 토픽 | 메시지 형식 | 내용 |
| --- | --- | --- |
| `/arduino/led` | `std_msgs/msg/String` | LED 상태 명령 (`IDLE`, `RUN`, `ARRIVE`, `STOP`, `ERROR`, `DOCKED`) |
| `/arduino/buzzer` | `std_msgs/msg/Bool` | 부저 켜기/끄기 |
| `/audio/command` | `robot_audio_interfaces/msg/AudioCommand` | TTS 또는 효과음 재생 명령 |
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 비상정지 활성화 시 속도 0 명령 |

`led_status_node`는 `/robot_status`의 값이 `driving`이면 LED를 `RUN`으로, 그 외에는 `IDLE`로 설정합니다. 비상정지가 활성화되면 다른 상태보다 `STOP`이 우선 적용됩니다.

## 실행 전 준비

TurtleBot3 모델과 LiDAR 모델을 환경 변수로 지정해야 합니다. 아래 값은 사용하는 하드웨어에 맞게 변경합니다.

```bash
export TURTLEBOT3_MODEL=burger
export LDS_MODEL=LDS-02

source /opt/ros/$ROS_DISTRO/setup.bash
source ~/turtlebot3_ws/install/setup.bash
source ~/arduino_bridge_ws/install/setup.bash
```

기본 설정은 다음 장치를 사용합니다.

| 인자 | 기본값 | 설명 |
| --- | --- | --- |
| `arduino_port` | Arduino의 `/dev/serial/by-id/...` 경로 | 센서 및 LED 제어용 Arduino 포트 |
| `opencr_port` | OpenCR의 `/dev/serial/by-id/...` 경로 | TurtleBot3 모터 제어용 OpenCR 포트 |
| `baud` | `9600` | Arduino 시리얼 통신 속도 (`9600` 또는 `115200`) |
| `audio_sink` | USB 오디오 장치의 PulseAudio sink | 안내 음성을 출력할 스피커 |

현재 연결된 시리얼 장치는 다음 명령으로 확인할 수 있습니다.

```bash
ls -l /dev/serial/by-id/
```

오디오 출력 장치는 다음 명령으로 확인할 수 있습니다.

```bash
pactl list short sinks
```

## 포트 또는 오디오 장치 변경

launch 인자는 명령 뒤에 `인자명:=값` 형식으로 전달합니다.

```bash
ros2 launch arduino_bridge robot_with_arduino.launch.py \
  arduino_port:=/dev/ttyACM1 \
  opencr_port:=/dev/ttyACM0 \
  baud:=9600 \
  audio_sink:=<PulseAudio_sink_이름>
```

고정된 `/dev/ttyACM*` 이름은 USB 연결 순서에 따라 달라질 수 있으므로, 실제 운용 환경에서는 `/dev/serial/by-id/...` 경로 사용을 권장합니다.

## 동작 확인

실행 후 노드와 주요 토픽을 확인합니다.

```bash
ros2 node list
ros2 topic list
ros2 topic echo /arduino/status
ros2 topic echo /arduino/button
```

센서 값은 다음과 같이 확인할 수 있습니다.

```bash
ros2 topic echo /arduino/front_distance_cm
ros2 topic echo /arduino/rear_distance_cm
ros2 topic echo /arduino/payload_loaded
```

LED와 부저를 직접 시험하려면 다음 명령을 사용합니다.

```bash
ros2 topic pub --once /arduino/led std_msgs/msg/String "{data: RUN}"
ros2 topic pub --once /arduino/buzzer std_msgs/msg/Bool "{data: true}"
```

## 참고 사항

- Arduino와 OpenCR은 서로 다른 시리얼 포트를 사용해야 합니다.
- 시리얼 포트를 열 수 없으면 `serial_bridge`가 시작되지 않으므로 포트 경로와 사용자 권한을 확인합니다.
- `robot.launch.py` 실행에는 `TURTLEBOT3_MODEL`과 `LDS_MODEL` 환경 변수가 필요합니다.
- 비상정지 해제 후 실제 주행을 다시 시작하려면 주행 명령을 새로 보내야 할 수 있습니다.
