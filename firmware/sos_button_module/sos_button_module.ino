// SOS 버튼 모듈 (Arduino Nano/Uno, 현장 설치)
// 시리얼 프로토콜: 부팅 시 "READY\n" 1회, 버튼 확정 입력마다 "BTN_01\n" / "BTN_02\n" / "BTN_03\n"
// 참고: docs/AURIX_MediRun_04_ROS2_소프트웨어_설계.pdf §1.3, §3.1 / 05_시스템_아키텍처 §3.2

const uint8_t BUTTON_COUNT = 3;
const uint8_t BUTTON_PINS[BUTTON_COUNT] = {2, 3, 4};
const char *BUTTON_IDS[BUTTON_COUNT] = {"BTN_01", "BTN_02", "BTN_03"};

// 2026-07-25 벤치 테스트: 문서 스펙은 300ms이나 버튼을 0.3초 눌러야 반응해 체감이 느림 →
// 50ms로 낮춰 시험 중. 아직 PDF(04_ROS2_소프트웨어_설계, 05_시스템_아키텍처)와
// emergency_dispatcher의 debounce_ms 파라미터는 300ms 그대로라 값이 어긋나 있음.
// PDF 동기화 전까지 임시값. CONVENTIONS.md §6.3 절차대로 문서 반영 필요.
const unsigned long DEBOUNCE_MS = 50;
const unsigned long BAUD_RATE = 9600;

bool rawState[BUTTON_COUNT];
bool stableState[BUTTON_COUNT];
unsigned long lastChangeAt[BUTTON_COUNT];

void setup() {
  Serial.begin(BAUD_RATE);
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    rawState[i] = digitalRead(BUTTON_PINS[i]);
    stableState[i] = rawState[i];
    lastChangeAt[i] = millis();
  }
  Serial.println("READY");
}

void loop() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    bool reading = digitalRead(BUTTON_PINS[i]);

    if (reading != rawState[i]) {
      rawState[i] = reading;
      lastChangeAt[i] = now;
    }

    if (now - lastChangeAt[i] >= DEBOUNCE_MS && stableState[i] != rawState[i]) {
      stableState[i] = rawState[i];
      // 버튼은 GND로 연결(INPUT_PULLUP) → 눌림은 LOW
      if (stableState[i] == LOW) {
        Serial.println(BUTTON_IDS[i]);
      }
    }
  }
}
