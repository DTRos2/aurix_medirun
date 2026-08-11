// 홀센서 모듈 (Arduino Uno R3, 로봇 탑재 — 최종적으로 LED·초음파와 한 보드로 통합 예정)
// 실제 하드웨어: SS49E 계열 선형(아날로그) 홀센서, 3핀 VCC/GND/AO
// (처음엔 KY-003류 디지털 홀센서로 시도했으나 자석을 대도 모듈 자체 LED조차 반응이 없어
// 아날로그 방식으로 교체함 — 자기장 세기를 연속값으로 읽으므로 약한 자석도 잡힐 가능성이 높음.)
// 용도: AED 거치대 자석 감지 → 적재/반출 판정 (요구사항 02번 §8.1)
// 채터링 방지: 상태 변화가 0.5초 이상 유지될 때만 유효 이벤트로 처리 (§8.3)
// 시리얼 프로토콜: Arduino -> Pi "HALL:<0|1>\n", 1=적재, 0=반출 (§8.3)
//   부팅 직후 현재 상태를 1회 보고하고, 이후 안정적으로 바뀔 때마다 다시 보고하며,
//   변화가 없어도 REPORT_INTERVAL_MS(1초)마다 재전송한다 — 시리얼 노이즈로 한 줄이
//   깨지거나 유실돼도 값이 영영 갱신되지 않는 일이 없게 하기 위함.
//
// 동작 방식: analogRead 값이 MAGNET_RAW_MAX 이하로 내려가면 자석 감지로 판단한다(절대값 기준
// 고정 컷오프). 실측: 자석 없음 507~511, 자석을 0.5cm 근처로 가져가면 506 이하로 떨어짐(최대
// -32까지). 자석을 완전히 밀착시키면 오히려 515~520으로 올라가는 비단조 구간이 있어 이 컷오프로는
// 못 잡는다 — 실제 거치대는 자석-센서 간격을 0.5cm 정도로 두고(완전 밀착 금지) 설계할 것.
// RAW:<analogRead 값> 디버그 줄은 재확인 필요할 때를 위해 남겨둠 — printRawDebug() 호출과
// 함수는 나중에 지워도 된다.
//
// 핀: AO=A0. LED(D9/D10/D11), 전방 초음파(D2/D3), 후방 초음파(D6)와 안 겹침.

const uint8_t HALL_PIN = A0;
const int MAGNET_RAW_MAX = 506; // 이 값 이하면 자석 감지 (실측: 무자석 507~511)

const unsigned long DEBOUNCE_MS = 500; // §8.3 채터링 방지 0.5초
const unsigned long BAUD_RATE = 9600;
// 상태 변화가 없어도 주기적으로 재전송해, 한 줄이 시리얼 노이즈로 유실돼도
// 최대 REPORT_INTERVAL_MS 안에 자동 복구되게 한다 (all_in_one_module과 동일 방식).
const unsigned long REPORT_INTERVAL_MS = 1000;

bool rawState;
bool stableState;
unsigned long lastChangeAt;
unsigned long lastReportAt;

bool isMagnetPresent(int value) {
  return value <= MAGNET_RAW_MAX;
}

// 매 loop()마다 analogRead 1회만 쓰면 RAW: 로그(200ms 간격)엔 안 보이는 순간 노이즈 튐이
// 디바운스 타이머를 계속 리셋시켜 상태가 영영 확정되지 않는 문제가 있었다. 여러 샘플을 평균 내
// 순간 튐의 영향을 줄인다.
int readSmoothedValue() {
  const uint8_t SAMPLES = 8;
  long sum = 0;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sum += analogRead(HALL_PIN);
  }
  return sum / SAMPLES;
}

void reportState(bool magnetPresent) {
  Serial.print("HALL:");
  Serial.println(magnetPresent ? 1 : 0);
}

// 벤치 테스트 전용 디버그: 실제 analogRead 원값을 그대로 출력해 THRESHOLD 튜닝에 쓴다.
unsigned long lastRawPrintAt = 0;
const unsigned long RAW_PRINT_INTERVAL_MS = 200;

void printRawDebug(int value) {
  unsigned long now = millis();
  if (now - lastRawPrintAt < RAW_PRINT_INTERVAL_MS) {
    return;
  }
  lastRawPrintAt = now;
  Serial.print("RAW:");
  Serial.println(value);
}

void setup() {
  Serial.begin(BAUD_RATE);

  rawState = isMagnetPresent(readSmoothedValue());
  stableState = rawState;
  lastChangeAt = millis();
  lastReportAt = millis();

  Serial.println("READY");
  reportState(stableState); // 부팅 시 현재 상태를 1회 보고
}

void loop() {
  int value = readSmoothedValue();
  printRawDebug(value);

  unsigned long now = millis();
  bool reading = isMagnetPresent(value);

  if (reading != rawState) {
    rawState = reading;
    lastChangeAt = now;
  }

  if (now - lastChangeAt >= DEBOUNCE_MS && stableState != rawState) {
    stableState = rawState;
    reportState(stableState);
    lastReportAt = now;
  } else if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    reportState(stableState);
    lastReportAt = now;
  }
}
