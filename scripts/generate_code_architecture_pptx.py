#!/usr/bin/env python3
from pathlib import Path

from generate_pptx import Picture, PptxBuilder, SlideSpec, TextBox, inches


NAVY = "071827"
INK = "0F172A"
MUTED = "5B6B7F"
CYAN = "22C7DF"
TEAL = "14B8A6"
GREEN = "22C55E"
AMBER = "F59E0B"
RED = "F43F5E"
BLUE = "3B82F6"
PURPLE = "8B5CF6"
WHITE = "FFFFFF"
PANEL = "FFFFFF"
LINE = "D5DEE9"


def box(x, y, w, h, text, size=1500, color=INK, fill=PANEL, line=LINE,
        radius=True, align="ctr", bold=False):
    return TextBox(inches(x), inches(y), inches(w), inches(h), text, size,
                   color, fill, line, radius, align, bold)


def label(x, y, w, h, text, size=1300, color=MUTED, align="l", bold=False):
    return TextBox(inches(x), inches(y), inches(w), inches(h), text, size,
                   color, None, None, False, align, bold)


def pill(x, y, w, text, fill, color=WHITE):
    return box(x, y, w, 0.36, text, 1050, color, fill, None, True, "ctr", True)


def build_slides(root: Path) -> list[SlideSpec]:
    assets = root / "assets"
    shots = root / "docs" / "screenshots" / "gui"
    hw = assets / "arduino_module"

    slides = []

    slides.append(SlideSpec(
        title="AURIX MediRun",
        subtitle="특정 상황으로 이해하는 AED 운반 보조 로봇 · 코드 기반 아키텍처",
        bg_color=NAVY,
        footer_text="TEAM AURIX · 10–15 MIN",
        variant="cover",
        text_boxes=[
            pill(0.95, 2.15, 2.0, "CODE-BASED", CYAN, NAVY),
            label(0.95, 2.85, 4.85, 0.72, "CALL → DECIDE → DRIVE", 2350, WHITE, "l", True),
            label(0.95, 3.62, 4.85, 0.52, "PROTECT → RECORD", 2350, "70E7F2", "l", True),
            label(0.95, 4.52, 4.85, 0.62,
                  "MQTT 호출 · ROS 2 Pub/Sub · Nav2 Action\nArduino 안전 · Qt GUI · PostgreSQL", 1300, "D9EAF2"),
            pill(7.30, 5.15, 3.55, "대표 상황 · 중앙 대합실 BTN_01", TEAL),
        ],
        pictures=[
            Picture(assets / "aurix_logo.png", inches(7.2), inches(1.22), inches(3.7), inches(1.9), None),
            Picture(assets / "robot.jpg", inches(7.25), inches(3.25), inches(3.6), inches(1.7), "2A6072"),
        ],
    ))

    slides.append(SlideSpec(
        title="특정 상황 · 중앙 대합실 BTN_01",
        subtitle="한 사람의 호출을 따라 전체 코드를 이해합니다",
        bg_color="F4FAFC", header_accent=CYAN,
        text_boxes=[
            box(0.95, 2.05, 2.7, 2.55, "① 상황 발생\n\n지하철 중앙 대합실에서\n승객이 갑자기 쓰러짐", 1550, INK, WHITE, "A9EAF1", True, "ctr", True),
            box(4.18, 2.05, 2.7, 2.55, "② 사람의 행동\n\nBTN_01 호출\n119 신고 · 응급조치", 1550, INK, WHITE, "A9EAF1", True, "ctr", True),
            box(7.42, 2.05, 3.65, 2.55, "③ 로봇의 역할\n\nHOME에서 AED를 싣고\n중앙 대합실로 운반", 1550, INK, WHITE, "A9EAF1", True, "ctr", True),
            label(3.68, 2.82, 0.45, 0.4, "→", 2400, CYAN, "ctr", True),
            label(6.92, 2.82, 0.45, 0.4, "→", 2400, CYAN, "ctr", True),
            box(0.95, 5.05, 10.12, 0.82,
                "역할 경계  |  로봇은 의료 판단을 하지 않습니다. AED 운반을 보조하고, 신고와 응급처치는 사람이 수행합니다.",
                1280, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="현재 구현 범위",
        subtitle="문서의 목표와 실제 런타임을 구분해야 구조가 정확해집니다",
        bg_color="F8FAFC", header_accent=TEAL,
        text_boxes=[
            box(0.95, 2.0, 3.1, 3.42,
                "현재 실행\n\nMQTT 호출\nGUI MissionController\nNav2 이동\n15cm 안전 게이트\nPostgreSQL 기록", 1450, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(4.25, 2.0, 3.1, 3.42,
                "선택 실행\n\nPS4 조종\nArduino LED / E-stop\nSLAM·웨이포인트\n회전·직진 시험", 1450, "78350F", "FFFBEB", "FCD34D", True, "ctr", True),
            box(7.55, 2.0, 3.52, 3.42,
                "소스·설계 단계\n\nmedirun_msgs typed interface\n분산 전체 미션 pipeline\n실제 자동 도킹\nROS DB 보조 노드", 1400, "334155", "F1F5F9", "CBD5E1", True, "ctr", True),
            box(0.95, 5.70, 10.12, 0.46,
                "현재 호출/미션 데이터는 주로 std_msgs/msg/String 안의 JSON을 사용합니다.",
                1180, "0C4A6E", "E0F2FE", "7DD3FC", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="전체 배치 아키텍처",
        subtitle="ESP32 · Raspberry Pi · PC/VM · PostgreSQL이 ROS 2 네트워크로 연결됩니다",
        bg_color="F3F7FA", header_accent=BLUE,
        text_boxes=[
            box(0.65, 2.18, 1.45, 1.45, "ESP32\nBTN_01", 1500, WHITE, BLUE, None, True, "ctr", True),
            label(2.12, 2.67, 0.55, 0.36, "→", 2200, BLUE, "ctr", True),
            box(2.67, 2.18, 1.55, 1.45, "MQTT\nBroker", 1500, "78350F", "FEF3C7", "F59E0B", True, "ctr", True),
            label(4.23, 2.67, 0.55, 0.36, "→", 2200, BLUE, "ctr", True),
            box(4.78, 1.95, 3.35, 1.95,
                "PC / VM\n\nmqtt_bridge → dispatcher\nQt GUI → Nav2", 1380, WHITE, "0E7490", None, True, "ctr", True),
            label(8.13, 2.67, 0.55, 0.36, "→", 2200, BLUE, "ctr", True),
            box(8.68, 1.95, 2.35, 1.95,
                "Raspberry Pi\n\nTurtleBot3 · Camera\nserial_bridge", 1350, WHITE, "115E59", None, True, "ctr", True),
            box(0.95, 4.45, 2.65, 1.15, "Arduino\n초음파 · 홀 · LED", 1320, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(3.62, 4.78, 0.55, 0.36, "↔", 2100, AMBER, "ctr", True),
            box(4.2, 4.45, 3.55, 1.15, "ROS 2 / CycloneDDS UDP\n/scan · /odom · /tf · /cmd_vel", 1270, "164E63", "ECFEFF", "67E8F9", True, "ctr", True),
            label(7.78, 4.78, 0.55, 0.36, "↔", 2100, PURPLE, "ctr", True),
            box(8.35, 4.45, 2.68, 1.15, "PostgreSQL\n호출 · 미션 · 기록", 1320, "4C1D95", "F5F3FF", "C4B5FD", True, "ctr", True),
        ],
    ))

    stages = [
        ("1", "MQTT", "BTN_01"), ("2", "SOS topic", "String"),
        ("3", "검증", "2s · 30%"), ("4", "Dispatch", "JSON"),
        ("5", "DB 좌표", "x · y · yaw"), ("6", "Queue", "중복 제거"),
        ("7", "Nav2", "Action goal"),
    ]
    stage_boxes = []
    x = 0.52
    for i, (number, name, detail) in enumerate(stages):
        stage_boxes.append(box(x, 2.25, 1.45, 2.0,
                               f"{number}\n{name}\n\n{detail}", 1250,
                               WHITE if i in (0, 3, 6) else INK,
                               [BLUE, "E0F2FE", "FEF3C7", TEAL, "EDE9FE", "DCFCE7", "0E7490"][i],
                               None if i in (0, 3, 6) else LINE, True, "ctr", True))
        if i < 6:
            stage_boxes.append(label(x + 1.45, 2.98, 0.25, 0.32, "→", 1550, MUTED, "ctr", True))
        x += 1.69
    stage_boxes += [
        box(0.95, 4.68, 10.15, 1.05,
            "dispatch_request  |  { mission_id: UUID, button_id: BTN_01, request_source: sos_button }\nGUI MissionController가 현재 단일 미션 조정자이며 중복 Nav2 goal을 막습니다.",
            1220, "0C4A6E", WHITE, "7DD3FC", True, "ctr", False)
    ]
    slides.append(SlideSpec(
        title="BTN_01 호출 처리 순서",
        subtitle="MQTT 문자열이 실제 로봇 이동 목표가 되기까지",
        bg_color="F7FBFD", header_accent=CYAN, text_boxes=stage_boxes,
    ))

    slides.append(SlideSpec(
        title="예외 상황과 대기열",
        subtitle="정상 흐름만큼 중요한 것은 반복 호출과 실패를 다루는 방식입니다",
        bg_color="FFFDFC", header_accent=AMBER,
        text_boxes=[
            box(0.95, 2.0, 2.35, 2.12, "정상 호출\n\nIdle이면 즉시 출발\nDB와 이력 저장", 1370, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(3.52, 2.0, 2.35, 2.12, "중복 BTN_01\n\n진행/대기 waypoint 검색\n중복 무시 기록", 1370, "164E63", "ECFEFF", "67E8F9", True, "ctr", True),
            box(6.09, 2.0, 2.35, 2.12, "추가 BTN_02\n\n새 호출을 queue에 등록\n현재 미션 뒤 처리", 1370, "4C1D95", "F5F3FF", "C4B5FD", True, "ctr", True),
            box(8.66, 2.0, 2.41, 2.12, "출동 거절\n\nCOOLDOWN · LOW_BATTERY\nPAYLOAD_MISSING · NOT_READY", 1260, "7F1D1D", "FEF2F2", "FCA5A5", True, "ctr", True),
            box(0.95, 4.48, 10.12, 1.1,
                "현재 배선 주의\ncallzone launch만 실행하면 payload / battery / readiness 발행자가 없어 dispatcher의 초기값을 사용합니다.\n실운영 전 /arduino/payload_loaded와 /battery_state 통합이 필요합니다.",
                1190, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="ROS 2 Pub / Sub / Type",
        subtitle="Publisher=방송국 · Topic=채널 · Subscriber=라디오 · Type=방송 서식",
        bg_color="F5FAFF", header_accent=BLUE,
        text_boxes=[
            box(0.95, 2.0, 2.15, 1.15, "Publisher\n방송국", 1500, WHITE, BLUE, None, True, "ctr", True),
            label(3.12, 2.38, 0.55, 0.36, "→", 2200, BLUE, "ctr", True),
            box(3.68, 2.0, 4.1, 1.15, "Topic + Type\n채널 + 데이터 서식", 1500, "0C4A6E", "E0F2FE", "7DD3FC", True, "ctr", True),
            label(7.80, 2.38, 0.55, 0.36, "→", 2200, BLUE, "ctr", True),
            box(8.36, 2.0, 2.71, 1.15, "Subscriber\n라디오", 1500, WHITE, "0E7490", None, True, "ctr", True),
            box(0.95, 3.58, 10.12, 0.62,
                "mqtt_bridge  →  /medirun/sensor/sos_button  [std_msgs/msg/String]  →  emergency_dispatcher",
                1210, "0C4A6E", WHITE, "BAE6FD", True, "ctr", True),
            box(0.95, 4.40, 10.12, 0.62,
                "Nav2 smoother / behavior_server  →  /medirun/navigation/cmd_vel_raw  [Twist]  →  safety_stop",
                1180, "14532D", WHITE, "BBF7D0", True, "ctr", True),
            box(0.95, 5.22, 10.12, 0.62,
                "OpenCR  →  /battery_state [sensor_msgs/msg/BatteryState]  ·  LiDAR → /scan [LaserScan]",
                1180, "4C1D95", WHITE, "DDD6FE", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="Nav2 이동과 위치 추정",
        subtitle="이동은 진행률·취소·결과가 필요한 장기 작업이므로 Action을 사용합니다",
        bg_color="F8FAFC", header_accent=PURPLE,
        text_boxes=[
            box(0.95, 2.0, 2.7, 1.6, "NavigateToPose\nAction\n\ngoal · feedback · cancel · result", 1350, WHITE, PURPLE, None, True, "ctr", True),
            box(4.18, 2.0, 2.7, 1.6, "AMCL + TF\n\n지도 속 로봇 위치와\n좌표계 관계 추정", 1350, "312E81", "EEF2FF", "A5B4FC", True, "ctr", True),
            box(7.42, 2.0, 3.65, 1.6, "Map + Costmap + LaserScan\n\n경로와 장애물 공간 판단", 1350, "164E63", "ECFEFF", "67E8F9", True, "ctr", True),
            label(3.68, 2.55, 0.45, 0.36, "→", 2100, PURPLE, "ctr", True),
            label(6.92, 2.55, 0.45, 0.36, "→", 2100, PURPLE, "ctr", True),
            box(0.95, 4.05, 5.0, 1.35,
                "접근 pose 검사\n목표 앞 0.5m → 0.4m → 0.3m\ncost < 90인 위치를 먼저 선택", 1320, "0C4A6E", WHITE, "7DD3FC", True, "ctr", True),
            box(6.08, 4.05, 4.99, 1.35,
                "GUI가 사용하는 입력\n/amcl_pose · /odom · /scan\n/global_costmap/costmap · TF", 1320, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            pill(4.57, 5.68, 2.9, "중복 goal은 atomic guard로 차단", TEAL),
        ],
    ))

    slides.append(SlideSpec(
        title="장애물 안전 정지와 Arduino",
        subtitle="Nav2 속도는 초음파 검사를 통과한 뒤에만 모터로 전달됩니다",
        bg_color="FFFBF5", header_accent=AMBER,
        text_boxes=[
            box(0.95, 2.05, 2.1, 1.25, "Nav2\nsmoother +\nBackUp / Spin", 1280, WHITE, BLUE, None, True, "ctr", True),
            label(3.07, 2.48, 0.55, 0.36, "→", 2200, AMBER, "ctr", True),
            box(3.64, 1.95, 3.65, 1.48, "ultrasonic_safety_stop\n\nfront / rear ≤ 15cm 정지\n> 15cm 즉시 해제", 1300, "7C2D12", "FEF3C7", "F59E0B", True, "ctr", True),
            label(7.31, 2.48, 0.55, 0.36, "→", 2200, AMBER, "ctr", True),
            box(7.88, 2.05, 3.15, 1.25, "/cmd_vel\nTurtleBot3 base", 1380, WHITE, "0F766E", None, True, "ctr", True),
            box(0.95, 3.88, 3.15, 1.55, "Arduino 센서\n\n초음파 100ms · 중앙값 3개\n홀 센서 · 버튼 · RGB LED", 1260, "7C2D12", WHITE, "FDBA74", True, "ctr", True),
            box(4.35, 3.88, 3.15, 1.55, "근접 timeout 보정\n\n이전 값 ≤ 25cm이면\n위험 거리 5cm로 간주", 1260, "7C2D12", WHITE, "FDBA74", True, "ctr", True),
            box(7.75, 3.88, 3.28, 1.55, "현재 상태\n\nNav2 recovery도 gate 통과\nGUI teleop 정책은 별도 필요", 1260, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
        ],
        pictures=[Picture(hw / "Ultrasonic_module.jpg", inches(10.35), inches(5.68), inches(0.68), inches(0.45), "FDBA74")],
    ))

    slides.append(SlideSpec(
        title="운영 GUI · 한 화면에서 판단",
        subtitle="일반 사용자 관제 + 관리자 SLAM·웨이포인트·시스템 관리",
        bg_color="F5F9FC", header_accent=CYAN,
        text_boxes=[
            box(7.95, 2.0, 3.1, 0.66, "전체 사용자", 1250, WHITE, "0E7490", None, True, "ctr", True),
            label(8.15, 2.82, 2.7, 1.28,
                  "운영 현황\n실시간 관제\n미션 관제\n원격 제어", 1380, INK, "l", True),
            box(7.95, 4.24, 3.1, 0.66, "관리자 추가", 1250, WHITE, PURPLE, None, True, "ctr", True),
            label(8.15, 5.03, 2.7, 0.85,
                  "지도 생성 · 웨이포인트\n사용자 · 로봇 · 설정", 1280, INK, "l", True),
            pill(0.95, 5.88, 6.55, "주행 기록·호출 이력은 구현됐지만 현재 메뉴에서 숨김", AMBER, "451A03"),
        ],
        pictures=[
            Picture(shots / "v3" / "00-5_remote_control_amcl.png", inches(0.95), inches(2.0), inches(6.55), inches(3.72), "86DDE8"),
        ],
    ))

    slides.append(SlideSpec(
        title="PostgreSQL · 전 과정을 추적",
        subtitle="호출은 화면에서 사라져도 판단·주행·영상 기록으로 남습니다",
        bg_color="F8FAFC", header_accent=PURPLE,
        text_boxes=[
            box(4.30, 2.32, 3.42, 1.35, "PostgreSQL\n19 tables", 1850, WHITE, PURPLE, None, True, "ctr", True),
            box(0.95, 2.0, 2.55, 1.55, "계정 · 장비\n\naurix_user\nrobot · robot_session", 1270, INK, WHITE, "C4B5FD", True, "ctr", True),
            box(8.52, 2.0, 2.55, 1.55, "호출 · 미션\n\ncall_button\nmission", 1270, INK, WHITE, "C4B5FD", True, "ctr", True),
            box(0.95, 4.15, 2.55, 1.55, "지도 · 경로\n\nmap · slam_job\nwaypoint_set · nav_goal_log", 1200, INK, WHITE, "C4B5FD", True, "ctr", True),
            box(4.30, 4.15, 3.42, 1.55, "이벤트 · 감사\n\nsystem_event · user_action\nconnection · control logs", 1200, INK, WHITE, "C4B5FD", True, "ctr", True),
            box(8.52, 4.15, 2.55, 1.55, "주행 · 영상\n\ndriving_record\ncamera_recording · files", 1200, INK, WHITE, "C4B5FD", True, "ctr", True),
            label(3.56, 2.63, 0.68, 0.35, "→", 1800, PURPLE, "ctr", True),
            label(7.77, 2.63, 0.68, 0.35, "→", 1800, PURPLE, "ctr", True),
            pill(4.30, 5.95, 3.42, "call_button = BTN_01 좌표의 기준 정보", TEAL),
        ],
    ))

    slides.append(SlideSpec(
        title="핵심 파라미터 · 동작을 바꾸는 손잡이",
        subtitle="숫자는 독립적이지 않습니다. 속도·센서 주기·공간 환경을 함께 시험해야 합니다",
        bg_color="FFFDF7", header_accent=AMBER,
        text_boxes=[
            box(0.95, 2.05, 1.85, 2.0, "2s\n\n호출\n쿨다운", 2200, "78350F", "FEF3C7", "FCD34D", True, "ctr", True),
            box(3.02, 2.05, 1.85, 2.0, "30%\n\n배터리\n거절 기준", 2200, "7F1D1D", "FEE2E2", "FCA5A5", True, "ctr", True),
            box(5.09, 2.05, 1.85, 2.0, "15cm\n\n전·후방\n정지 거리", 2200, "0C4A6E", "E0F2FE", "7DD3FC", True, "ctr", True),
            box(7.16, 2.05, 1.85, 2.0, "4s\n\n정지\n유지 시간", 2200, "14532D", "DCFCE7", "86EFAC", True, "ctr", True),
            box(9.22, 2.05, 1.85, 2.0, "0.05m\n\n지도\n해상도", 2200, "4C1D95", "EDE9FE", "C4B5FD", True, "ctr", True),
            box(0.95, 4.50, 4.88, 1.18,
                "MQTT\n192.168.230.9 : 1883\nmedirun/button", 1320, "78350F", WHITE, "FCD34D", True, "ctr", True),
            box(6.18, 4.50, 4.89, 1.18,
                "Arduino serial\n9600 baud · 약 100ms sensor sample\nPi는 by-id device 권장", 1280, "0C4A6E", WHITE, "7DD3FC", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="시험 결과와 현재 한계",
        subtitle="ROS 내부 log 성능과 실제 물리 성능을 함께 측정해야 합니다",
        bg_color="F7FCF8", header_accent=GREEN,
        text_boxes=[
            box(0.95, 2.0, 3.0, 2.05, "T-00 · 90° 회전\n\n0.13° MAE\n10 / 10 PASS\n(log 기준)", 1720, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(4.18, 2.0, 3.0, 2.05, "T-01 · 0.45m 직진\n\n0.002m MAE\n10 / 10 PASS\n(odom 기준)", 1720, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(7.42, 2.0, 3.65, 2.05, "물리 측정 주의\n\n회전 약 4° 부족 관측\n바퀴 미끄러짐·바닥 영향\nground truth 필요", 1430, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(0.95, 4.50, 10.12, 1.18,
                "우선 개선\n① 실제 battery/payload 배선  ② typed medirun_msgs  ③ 단일 조정자·패키징 정리\n④ teleop 안전 정책 적용  ⑤ 모의 도킹 → 실제 도킹",
                1250, "334155", WHITE, "CBD5E1", True, "ctr", True),
        ],
    ))

    slides.append(SlideSpec(
        title="결론 · 호출에서 기록까지 하나의 흐름",
        subtitle="중앙 대합실 BTN_01 상황을 코드가 어떻게 처리했는지 다시 연결합니다",
        bg_color="F0FDFA", header_accent=CYAN, footer_text="AURIX MEDIRUN",
        text_boxes=[
            box(0.75, 2.15, 1.75, 1.35, "CALL\nMQTT", 1750, WHITE, BLUE, None, True, "ctr", True),
            label(2.48, 2.60, 0.35, 0.35, "→", 1900, "7DD3FC", "ctr", True),
            box(2.82, 2.15, 1.75, 1.35, "DECIDE\nQueue", 1750, WHITE, PURPLE, None, True, "ctr", True),
            label(4.55, 2.60, 0.35, 0.35, "→", 1900, "7DD3FC", "ctr", True),
            box(4.89, 2.15, 1.75, 1.35, "DRIVE\nNav2", 1750, WHITE, TEAL, None, True, "ctr", True),
            label(6.62, 2.60, 0.35, 0.35, "→", 1900, "7DD3FC", "ctr", True),
            box(6.96, 2.15, 1.75, 1.35, "PROTECT\n15cm", 1750, NAVY, "FCD34D", None, True, "ctr", True),
            label(8.69, 2.60, 0.35, 0.35, "→", 1900, "7DD3FC", "ctr", True),
            box(9.03, 2.15, 2.04, 1.35, "RECORD\nDB · Video", 1750, WHITE, "0E7490", None, True, "ctr", True),
            box(0.95, 4.22, 10.12, 1.10,
                "현재 핵심\nGUI의 단일 미션 조정자가 DB 좌표와 Nav2 Action을 연결하고,\nArduino 초음파 안전 게이트가 최종 자율주행 속도를 보호합니다.",
                1450, WHITE, "102A3A", "2C6370", True, "ctr", True),
            pill(4.18, 5.72, 3.72, "AED 운반을 돕는 신뢰 가능한 연결", CYAN, NAVY),
        ],
    ))

    # Appendix A1 — independent architecture: call inputs
    slides.append(SlideSpec(
        title="부록 A1 · 호출 입력 아키텍처",
        subtitle="Wi-Fi 버튼은 현재 호출 경로, 유선 Arduino 버튼은 namespace 통합이 필요합니다",
        bg_color="F5FAFF", header_accent=BLUE, footer_text="APPENDIX · CALL INPUT",
        text_boxes=[
            pill(0.95, 1.95, 4.45, "Wi-Fi 호출 · 현재 연결", BLUE),
            box(0.95, 2.48, 1.55, 1.18, "ESP32\nGPIO 25", 1350, WHITE, BLUE, None, True, "ctr", True),
            label(2.52, 2.87, 0.35, 0.3, "→", 1800, BLUE, "ctr", True),
            box(2.88, 2.48, 1.65, 1.18, "200ms\ndebounce", 1260, "0C4A6E", "E0F2FE", "7DD3FC", True, "ctr", True),
            label(4.55, 2.87, 0.35, 0.3, "→", 1800, BLUE, "ctr", True),
            box(4.92, 2.48, 2.05, 1.18, "MQTT Broker\nmedirun/button", 1250, "78350F", "FEF3C7", "FCD34D", True, "ctr", True),
            label(6.99, 2.87, 0.35, 0.3, "→", 1800, BLUE, "ctr", True),
            box(7.36, 2.48, 1.7, 1.18, "mqtt_bridge", 1320, WHITE, TEAL, None, True, "ctr", True),
            label(9.08, 2.87, 0.35, 0.3, "→", 1800, BLUE, "ctr", True),
            box(9.45, 2.35, 1.62, 1.45, "/sos_button\nString", 1270, WHITE, "0E7490", None, True, "ctr", True),
            pill(0.95, 4.18, 4.45, "유선 호출 · 변환 연결 필요", AMBER, "451A03"),
            box(0.95, 4.70, 1.55, 1.05, "Arduino\nA1/A2/A3", 1250, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(2.52, 5.04, 0.35, 0.3, "→", 1800, AMBER, "ctr", True),
            box(2.88, 4.70, 1.65, 1.05, "50ms\ndebounce", 1200, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(4.55, 5.04, 0.35, 0.3, "→", 1800, AMBER, "ctr", True),
            box(4.92, 4.70, 2.05, 1.05, "Serial BTN:*", 1250, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(6.99, 5.04, 0.35, 0.3, "→", 1800, AMBER, "ctr", True),
            box(7.36, 4.70, 1.70, 1.05, "serial_bridge", 1220, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(9.08, 5.04, 0.35, 0.3, "⇢", 1800, RED, "ctr", True),
            box(9.45, 4.55, 1.62, 1.35, "/arduino/button\n→ /sos_button", 1110, "7F1D1D", "FEF2F2", "FCA5A5", True, "ctr", True),
        ],
    ))

    # Appendix A2 — independent architecture: dispatch validation
    slides.append(SlideSpec(
        title="부록 A2 · 출동 검증 아키텍처",
        subtitle="호출을 받았다고 바로 출발하지 않고 네 가지 조건을 순서대로 확인합니다",
        bg_color="FFFDFC", header_accent=AMBER, footer_text="APPENDIX · DISPATCH",
        text_boxes=[
            box(0.95, 2.0, 2.1, 0.72, "SOS · String", 1200, WHITE, BLUE, None, True, "ctr", True),
            box(3.28, 2.0, 2.1, 0.72, "Payload · Bool", 1200, "14532D", "DCFCE7", "86EFAC", True, "ctr", True),
            box(5.61, 2.0, 2.1, 0.72, "Battery · String", 1200, "78350F", "FEF3C7", "FCD34D", True, "ctr", True),
            box(7.94, 2.0, 2.1, 0.72, "Readiness · String", 1200, "4C1D95", "EDE9FE", "C4B5FD", True, "ctr", True),
            label(5.20, 2.77, 0.55, 0.36, "↓", 1900, AMBER, "ctr", True),
            box(4.05, 3.12, 3.0, 0.68, "emergency_dispatcher", 1400, WHITE, AMBER, None, True, "ctr", True),
            label(0.90, 4.12, 0.35, 0.3, "→", 1700, AMBER, "ctr", True),
            box(1.25, 3.93, 2.0, 0.75, "2초 쿨다운?", 1200, "78350F", WHITE, "FCD34D", True, "ctr", True),
            label(3.27, 4.12, 0.35, 0.3, "→", 1700, AMBER, "ctr", True),
            box(3.63, 3.93, 2.0, 0.75, "AED 적재?", 1200, "78350F", WHITE, "FCD34D", True, "ctr", True),
            label(5.65, 4.12, 0.35, 0.3, "→", 1700, AMBER, "ctr", True),
            box(6.01, 3.93, 2.0, 0.75, "배터리 ≥ 30%?", 1200, "78350F", WHITE, "FCD34D", True, "ctr", True),
            label(8.03, 4.12, 0.35, 0.3, "→", 1700, AMBER, "ctr", True),
            box(8.39, 3.93, 2.0, 0.75, "READY?", 1200, "78350F", WHITE, "FCD34D", True, "ctr", True),
            box(0.95, 5.15, 4.75, 0.78, "실패 → dispatch_rejection JSON\nCOOLDOWN · PAYLOAD · BATTERY · NOT_READY", 1110, "7F1D1D", "FEF2F2", "FCA5A5", True, "ctr", True),
            box(6.03, 5.15, 5.04, 0.78, "통과 → UUID + dispatch_request JSON\n→ GUI MissionRosInterface", 1160, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
        ],
    ))

    # Appendix A3 — independent architecture: mission queue
    slides.append(SlideSpec(
        title="부록 A3 · 미션 대기열 아키텍처",
        subtitle="GUI MissionController 하나가 좌표, 대기열, 상태와 Nav2 goal을 조정합니다",
        bg_color="F8FAFC", header_accent=PURPLE, footer_text="APPENDIX · MISSION",
        text_boxes=[
            box(0.95, 2.0, 2.0, 1.0, "dispatch_request\nString JSON", 1230, WHITE, BLUE, None, True, "ctr", True),
            label(2.97, 2.33, 0.45, 0.3, "→", 1800, PURPLE, "ctr", True),
            box(3.44, 1.93, 2.52, 1.15, "MissionRosInterface\nJSON → Qt signal", 1250, "312E81", "EDE9FE", "C4B5FD", True, "ctr", True),
            label(5.98, 2.33, 0.45, 0.3, "→", 1800, PURPLE, "ctr", True),
            box(6.45, 1.80, 3.05, 1.42, "MissionController\n단일 조정자", 1500, WHITE, PURPLE, None, True, "ctr", True),
            box(9.75, 1.93, 1.32, 1.15, "DB 좌표\nBTN_01", 1160, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            label(9.48, 2.33, 0.25, 0.3, "↔", 1600, PURPLE, "ctr", True),
            box(0.95, 3.74, 2.25, 1.05, "동일 waypoint?\n중복 무시", 1210, "164E63", "ECFEFF", "67E8F9", True, "ctr", True),
            label(3.22, 4.08, 0.35, 0.3, "→", 1750, PURPLE, "ctr", True),
            box(3.58, 3.74, 2.25, 1.05, "Waiting Queue\n순서·상태 저장", 1210, "312E81", "EDE9FE", "C4B5FD", True, "ctr", True),
            label(5.85, 4.08, 0.35, 0.3, "→", 1750, PURPLE, "ctr", True),
            box(6.21, 3.74, 2.25, 1.05, "Idle이면 시작\nStarting", 1210, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            label(8.48, 4.08, 0.35, 0.3, "→", 1750, PURPLE, "ctr", True),
            box(8.84, 3.74, 2.23, 1.05, "Nav2 Action\nNavigating", 1210, WHITE, "0E7490", None, True, "ctr", True),
            box(0.95, 5.28, 10.12, 0.66,
                "상태 분기  |  Completed · Cancelling · Diverting · ReturningHome · Failed  → DB 이력",
                1170, "334155", WHITE, "CBD5E1", True, "ctr", True),
        ],
    ))

    # Appendix A4 — independent architecture: GUI layers
    slides.append(SlideSpec(
        title="부록 A4 · GUI 내부 계층 아키텍처",
        subtitle="화면과 ROS 2·DB를 직접 섞지 않고 Controller와 Service 계층으로 분리합니다",
        bg_color="F4FAFC", header_accent=CYAN, footer_text="APPENDIX · GUI",
        text_boxes=[
            pill(0.95, 1.95, 10.12, "Qt Widgets · 사용자 화면", CYAN, NAVY),
            box(0.95, 2.42, 10.12, 0.72,
                "로그인  |  운영 현황  |  실시간 관제  |  미션 관제  |  원격 제어  |  SLAM  |  웨이포인트  |  관리",
                1170, INK, WHITE, "A5F3FC", True, "ctr", True),
            label(5.65, 3.18, 0.55, 0.3, "↓", 1900, CYAN, "ctr", True),
            box(0.95, 3.50, 10.12, 0.72,
                "DashboardCentralWidget · GuiApplicationController · MissionController · TeleopController",
                1230, WHITE, "0E7490", None, True, "ctr", True),
            label(5.65, 4.27, 0.55, 0.3, "↓", 1900, CYAN, "ctr", True),
            box(0.95, 4.58, 4.90, 0.92,
                "ROS Service 계층\nNav2 · Mission · Camera · Battery · Sensor", 1190, "0C4A6E", WHITE, "7DD3FC", True, "ctr", True),
            box(6.17, 4.58, 4.90, 0.92,
                "DB Service 계층\nAuth · Robot · MissionStorage · Operations · Record", 1190, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            box(0.95, 5.73, 4.90, 0.45, "ROS 2 Graph · 별도 executor thread", 1070, WHITE, BLUE, None, True, "ctr", True),
            box(6.17, 5.73, 4.90, 0.45, "PostgreSQL · Qt SQL", 1070, WHITE, PURPLE, None, True, "ctr", True),
        ],
    ))

    # Appendix A5 — independent architecture: Arduino
    slides.append(SlideSpec(
        title="부록 A5 · Arduino 센서·출력 아키텍처",
        subtitle="센서 입력은 펌웨어에서 필터링된 뒤 Pi serial_bridge가 typed topic으로 변환합니다",
        bg_color="FFF9F3", header_accent=AMBER, footer_text="APPENDIX · ARDUINO",
        text_boxes=[
            box(0.75, 2.0, 1.75, 0.90, "전·후방\n초음파", 1200, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(2.67, 2.0, 1.75, 0.90, "홀 센서\nAED 적재", 1200, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(4.59, 2.0, 1.75, 0.90, "버튼\nA1/A2/A3", 1200, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(6.51, 2.0, 1.75, 0.90, "RGB LED\n명령 출력", 1200, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(8.43, 2.0, 2.0, 0.90, "100ms / debounce\n중앙값 3개", 1120, "78350F", "FEF3C7", "FCD34D", True, "ctr", True),
            label(5.45, 2.94, 0.55, 0.3, "↓", 1900, AMBER, "ctr", True),
            box(3.25, 3.28, 5.30, 0.80, "all_in_one_module.ino  →  USB Serial 9600", 1320, WHITE, AMBER, None, True, "ctr", True),
            label(5.45, 4.13, 0.55, 0.3, "↓", 1900, AMBER, "ctr", True),
            box(4.18, 4.48, 3.45, 0.72, "Raspberry Pi · serial_bridge", 1280, WHITE, "0E7490", None, True, "ctr", True),
            label(1.45, 5.42, 0.4, 0.3, "↓", 1700, TEAL, "ctr", True),
            label(4.05, 5.42, 0.4, 0.3, "↓", 1700, TEAL, "ctr", True),
            label(6.65, 5.42, 0.4, 0.3, "↓", 1700, TEAL, "ctr", True),
            label(9.25, 5.42, 0.4, 0.3, "↑", 1700, TEAL, "ctr", True),
            box(0.75, 5.72, 2.25, 0.47, "distance · Int32", 1030, "0C4A6E", "ECFEFF", "67E8F9", True, "ctr", True),
            box(3.35, 5.72, 2.25, 0.47, "payload · Bool", 1030, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(5.95, 5.72, 2.25, 0.47, "button · String", 1030, "4C1D95", "F5F3FF", "C4B5FD", True, "ctr", True),
            box(8.55, 5.72, 2.25, 0.47, "LED / buzzer command", 1010, "78350F", "FEF3C7", "FCD34D", True, "ctr", True),
        ],
    ))

    # Appendix A6 — independent architecture: safety gate
    slides.append(SlideSpec(
        title="부록 A6 · 자율주행 안전 게이트",
        subtitle="속도 방향과 전·후방 거리를 함께 보고 정지 또는 통과를 결정합니다",
        bg_color="FFFDFC", header_accent=RED, footer_text="APPENDIX · SAFETY",
        text_boxes=[
            box(0.95, 2.0, 2.30, 1.05, "Nav2 smoother +\nBackUp / Spin\ncmd_vel_raw · Twist", 1130, WHITE, BLUE, None, True, "ctr", True),
            box(0.95, 3.45, 2.30, 0.82, "front · Int32", 1150, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            box(0.95, 4.55, 2.30, 0.82, "rear · Int32", 1150, "7C2D12", "FFF7ED", "FDBA74", True, "ctr", True),
            label(3.27, 3.45, 0.55, 0.35, "→", 2000, RED, "ctr", True),
            box(3.84, 2.55, 3.25, 2.60,
                "ultrasonic_safety_stop\n\n전진 + front ≤ 15cm?\n후진 + rear ≤ 15cm?\n회전 + 둘 중 하나 ≤ 15cm?\n\n위험: linear.x / angular.z = 0", 1180, "7F1D1D", "FEF2F2", "FCA5A5", True, "ctr", True),
            label(7.11, 3.45, 0.55, 0.35, "→", 2000, RED, "ctr", True),
            box(7.68, 2.60, 3.39, 1.30, "안전\n원래 Twist 통과", 1370, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            box(7.68, 4.18, 3.39, 1.30, "위험\n15cm 초과까지 정지", 1370, "7F1D1D", "FEF2F2", "FCA5A5", True, "ctr", True),
            pill(3.84, 5.65, 7.23, "최종 /cmd_vel → TurtleBot3 · 현재 GUI/PS4 teleop은 gate 우회", RED),
        ],
    ))

    # Appendix A7 — independent architecture: SLAM and waypoints
    slides.append(SlideSpec(
        title="부록 A7 · SLAM·웨이포인트 아키텍처",
        subtitle="센서 준비와 TF 충돌을 확인하고 새 지도를 저장한 뒤 목적지 좌표로 사용합니다",
        bg_color="F5FAFF", header_accent=CYAN, footer_text="APPENDIX · SLAM",
        text_boxes=[
            box(0.60, 2.18, 1.68, 1.12, "/scan\n/odom\n/tf · tf_static", 1180, WHITE, BLUE, None, True, "ctr", True),
            label(2.30, 2.58, 0.40, 0.3, "→", 1800, CYAN, "ctr", True),
            box(2.72, 2.18, 1.70, 1.12, "센서 health\n준비 확인", 1210, "0C4A6E", "E0F2FE", "7DD3FC", True, "ctr", True),
            label(4.44, 2.58, 0.40, 0.3, "→", 1800, CYAN, "ctr", True),
            box(4.86, 2.18, 1.88, 1.12, "Nav2/AMCL\nLifecycle pause", 1160, "4C1D95", "EDE9FE", "C4B5FD", True, "ctr", True),
            label(6.76, 2.58, 0.40, 0.3, "→", 1800, CYAN, "ctr", True),
            box(7.18, 2.18, 1.70, 1.12, "Cartographer\n/map", 1210, WHITE, "0E7490", None, True, "ctr", True),
            label(8.90, 2.58, 0.40, 0.3, "→", 1800, CYAN, "ctr", True),
            box(9.32, 2.18, 1.75, 1.12, "map_saver\nPGM + YAML", 1160, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            label(5.65, 3.50, 0.55, 0.3, "↓", 1900, CYAN, "ctr", True),
            box(1.10, 3.88, 3.0, 1.42, "Waypoint Management\n지도 클릭 · 방향 드래그\n좌표 편집", 1260, "0C4A6E", WHITE, "7DD3FC", True, "ctr", True),
            box(4.58, 3.88, 3.0, 1.42, "호출구역 · 경유점 · HOME\n\nHOME은 정확히 1개", 1260, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            box(8.06, 3.88, 3.0, 1.42, "저장·사용\nYAML + DB call_button\nNav2 시험 주행", 1260, "14532D", WHITE, "86EFAC", True, "ctr", True),
            label(4.12, 4.42, 0.42, 0.3, "→", 1800, CYAN, "ctr", True),
            label(7.60, 4.42, 0.42, 0.3, "→", 1800, CYAN, "ctr", True),
            pill(3.90, 5.70, 4.40, "TF 충돌 방지: localization/navigation pause 후 매핑", PURPLE),
        ],
    ))

    # Appendix A8 — independent architecture: recording
    slides.append(SlideSpec(
        title="부록 A8 · 주행·영상 기록 아키텍처",
        subtitle="실시간 데이터와 MP4를 같은 주행 세션에 묶어 장애 분석과 재생에 사용합니다",
        bg_color="F8FAFC", header_accent=PURPLE, footer_text="APPENDIX · RECORDING",
        text_boxes=[
            box(0.75, 2.0, 1.75, 0.75, "/amcl_pose", 1150, WHITE, BLUE, None, True, "ctr", True),
            box(0.75, 2.95, 1.75, 0.75, "/odom", 1150, WHITE, BLUE, None, True, "ctr", True),
            box(0.75, 3.90, 1.75, 0.75, "/battery_state", 1150, WHITE, BLUE, None, True, "ctr", True),
            box(0.75, 4.85, 1.75, 0.75, "camera compressed", 1090, WHITE, BLUE, None, True, "ctr", True),
            label(2.52, 3.65, 0.45, 0.3, "→", 1900, PURPLE, "ctr", True),
            box(2.99, 2.55, 2.65, 2.40,
                "DrivingRecordService\n\n1Hz 위치·속도·배터리\n주행 세션 start/finish\n\nVideoRecorder\nFPS warm-up → MP4", 1220, "312E81", "EDE9FE", "C4B5FD", True, "ctr", True),
            label(5.66, 3.65, 0.45, 0.3, "→", 1900, PURPLE, "ctr", True),
            box(6.13, 2.12, 2.15, 1.0, "driving_session\ndriving_record", 1140, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            box(6.13, 3.45, 2.15, 1.0, "camera_session\ncamera_recording", 1110, "4C1D95", WHITE, "C4B5FD", True, "ctr", True),
            box(6.13, 4.78, 2.15, 0.82, "MP4 file", 1200, "14532D", "F0FDF4", "86EFAC", True, "ctr", True),
            label(8.30, 3.65, 0.45, 0.3, "→", 1900, PURPLE, "ctr", True),
            box(8.77, 2.60, 2.30, 2.05, "Records Playback\n\n위치·시간·영상 재생\n\n현재 사이드바 숨김", 1260, "334155", WHITE, "CBD5E1", True, "ctr", True),
        ],
    ))

    # Appendix A9 — independent architecture: launches
    slides.append(SlideSpec(
        title="부록 A9 · 시스템 기동 아키텍처",
        subtitle="PC/VM과 Raspberry Pi의 launch 책임을 분리하고 같은 CycloneDDS graph에서 연결합니다",
        bg_color="F5FAFF", header_accent=BLUE, footer_text="APPENDIX · LAUNCH",
        text_boxes=[
            pill(0.75, 1.95, 5.30, "PC / VM", BLUE),
            box(0.75, 2.43, 2.40, 1.20, "scripts/run.sh\n↓\nbringup.launch.py", 1210, WHITE, BLUE, None, True, "ctr", True),
            box(3.35, 2.43, 2.70, 1.20, "Nav2 · safety_stop · GUI\nmap · use_rviz · use_gui", 1170, "0C4A6E", WHITE, "7DD3FC", True, "ctr", True),
            box(0.75, 4.08, 2.40, 1.20, "run_callzone.sh\n↓\ncallzone_bringup", 1210, WHITE, "0E7490", None, True, "ctr", True),
            box(3.35, 4.08, 2.70, 1.20, "mqtt_bridge\nemergency_dispatcher", 1210, "164E63", WHITE, "67E8F9", True, "ctr", True),
            pill(6.35, 1.95, 4.72, "Raspberry Pi", TEAL),
            box(6.35, 2.43, 4.72, 1.20, "robot_with_arduino.launch.py", 1320, WHITE, TEAL, None, True, "ctr", True),
            box(6.35, 4.08, 4.72, 1.20,
                "TurtleBot3 driver · Camera · serial_bridge\nLED · E-stop · Audio · optional MQTT bridge", 1140, "14532D", WHITE, "86EFAC", True, "ctr", True),
            pill(3.53, 5.70, 5.95, "CycloneDDS UDP · 동일 ROS_DOMAIN · Pi ↔ PC topic/action/service", PURPLE),
        ],
    ))

    # Appendix A10
    slides.append(SlideSpec(
        title="부록 A10 · 핵심 Pub / Sub / Type",
        subtitle="질의응답용 · 현재 실행 경로 중심",
        bg_color="F8FAFC", header_accent=BLUE, footer_text="APPENDIX",
        text_boxes=[
            pill(0.95, 1.94, 5.0, "호출 · 미션", BLUE),
            box(0.95, 2.42, 5.0, 3.52,
                "mqtt_bridge\nPUB  /medirun/sensor/sos_button  String\nPUB  /medirun/sensor/sos_status  String\n\nemergency_dispatcher\nSUB  sos_button String\nSUB  payload_loaded Bool\nSUB  readiness String · battery String\nPUB  dispatch_request / rejection String JSON\n\nGUI Mission bridge\nSUB  dispatch_request · state · nav/status String\nPUB  ui/mission_command String JSON",
                1030, INK, WHITE, "BFDBFE", True, "l", False),
            pill(6.08, 1.94, 4.99, "주행 · 센서", TEAL),
            box(6.08, 2.42, 4.99, 3.52,
                "GUI / Nav2\nSUB  amcl_pose PoseWithCovarianceStamped\nSUB  costmap OccupancyGrid · scan LaserScan\nSUB  odom Odometry · tf TFMessage\nPUB  initialpose · speed_limit · cmd_vel\nACTION  NavigateToPose · FollowWaypoints\n\nserial_bridge\nPUB  front/rear Int32 · payload Bool\nPUB  button/status String\nSUB  led String · buzzer Bool\n\nsafety_stop\nSUB  distance Int32 · cmd_vel_raw Twist\nPUB  /cmd_vel Twist",
                990, INK, WHITE, "99F6E4", True, "l", False),
        ],
    ))

    # Appendix A11
    slides.append(SlideSpec(
        title="부록 A11 · Parameter 빠른 참조",
        subtitle="질의응답용 · 실제 current path와 선택형 값을 구분",
        bg_color="FFFCF7", header_accent=AMBER, footer_text="APPENDIX",
        text_boxes=[
            pill(0.95, 1.94, 5.0, "현재 실행 경로", AMBER, "451A03"),
            box(0.95, 2.42, 5.0, 3.52,
                "mqtt_bridge\n broker_host: 192.168.230.9\n broker_port: 1883\n mqtt_topic: medirun/button\n\nemergency_dispatcher\n battery_reject_percent: 30.0\n call_cooldown_sec: 2.0\n\nultrasonic_safety_stop\n front_stop_distance_cm: 15\n rear_stop_distance_cm: 15\n hold_duration_sec: 0.0\n\nserial_bridge\n port: /dev/ttyACM0 또는 by-id\n baud: 9600",
                1040, INK, WHITE, "FCD34D", True, "l", False),
            pill(6.08, 1.94, 4.99, "선택형 · 시험", PURPLE),
            box(6.08, 2.42, 4.99, 3.52,
                "mission_manager\n arrival_timeout_sec: 180.0\n return_delay: 15.0\n home: HOME · docking: DOCK\n simulate_navigation: true\n\nPS4 teleop\n linear/angular: 0.22 / 2.84\n turbo: 0.26 / 3.2\n\nT-00 rotation\n angle: 90° · speed: 0.35\n tolerance: 3° · loop: 20Hz\n\nT-01 straight\n distance: 0.45m · speed: 0.10\n tolerance: 0.01m · heading_kp: 1.5",
                1020, INK, WHITE, "C4B5FD", True, "l", False),
        ],
    ))

    return slides


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    output = root / "ppt" / "AURIX_MediRun_코드기반_발표자료.pptx"
    builder = PptxBuilder(output)
    for slide in build_slides(root):
        builder.add_slide(slide)
    builder.build()
    print(output)


if __name__ == "__main__":
    main()
