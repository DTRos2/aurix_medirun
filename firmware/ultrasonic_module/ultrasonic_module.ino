// 초음파 센서 모듈 (Arduino Uno R3, 로봇 탑재 — 최종적으로 LED·홀센서와 한 보드로 통합 예정)
// 실제 하드웨어: HC-SR04 x2, 5V 동작 — 전방/후방이 서로 다른 배선이라 드라이버 함수가 다름.
//   전방: 표준 4핀(Vcc/Trig/Echo/Gnd) 모듈. 양옆 바퀴 사이 중앙, 하단 — LiDAR 스캔 평면
//         아래 사각지대 보완용 (요구사항 02번 §6.5)
//   후방: ★ 3핀으로 개조된 모듈 — Trig/Echo가 물리적으로 한 선(S)으로 합쳐진 single-pin 방식.
//         AED 트레일러(리어 트레이) 후단에 부착, 트레일러 장착으로 LiDAR가 못 보는 후진
//         사각지대 보완용 (§6.5 후방 처리 갱신분). V-G 사이 3.3uF 벌크 캐패시터는 전원 안정용,
//         코드와 무관.
//         시퀀스: S를 OUTPUT/LOW → HIGH 10us 트리거 → INPUT 전환 → 같은 S핀에서 에코 펄스폭
//         측정. NewPing 라이브러리 없이 pinMode 전환 + pulseIn으로 직접 구현.
//
// 요구사항 02번 §6.5/§8.1은 전방 좌우 2개(DIST:<L>,<R>)를 전제하는데, 지금은 전방 1개(중앙)만
// 장착해 회피 개선 여부를 먼저 확인하는 단계. 전방 1개로 충분하지 않으면 그때 좌/우 센서를
// 추가하고 전방 프로토콜을 "DIST:<L>,<R>"로 되돌린다. 지금은 전방 1개뿐이라 "DIST:<cm>\n"
// 단일값으로 임시 단순화함 — §8.3 정식 포맷과 다르니 주의.
//
// 시리얼 프로토콜: Arduino -> Pi
//   "DIST:<cm>\n"       전방 거리
//   "DIST_REAR:<cm>\n"  후방(트레일러) 거리
//
// 측정 주기 10Hz, 3회 이동 중앙값 필터, 감지 임계 0.15m(1차 벤치 테스트 값, 실주행 후 조정 가능,
// §6.5) — 임계값 판단·회피 자체는 Pi/Nav2 쪽 책임이라 이 스케치는 필터링된 거리(cm)만 보고한다.
// 타임아웃(사거리 밖/무반사)은 "장애물 없음"으로 해석해야 안전해서 0이 아니라 MAX_RANGE_CM으로
// 처리한다 — 0을 쓰면 "센서 바로 앞에 붙어있는 장애물"로 오인될 수 있음.
//
// LED 핀(D9/D10/D11)과 겹치지 않게 배정: 전방 Trig=D2/Echo=D3, 후방 S(단일핀)=D6. (D7은 미사용)
// pulseIn()이 블로킹 호출이라 최악의 경우 센서당 ECHO_TIMEOUT_US만큼 멈춘다 — LED·홀센서와
// 한 스케치로 합칠 때는 이 부분을 비차단(상태머신) 방식으로 바꿔야 한다.

const uint8_t FRONT_TRIG_PIN = 2;
const uint8_t FRONT_ECHO_PIN = 3;
const uint8_t REAR_PIN = 6; // Trig/Echo 겸용 단일 신호선(S)

const unsigned long SAMPLE_INTERVAL_MS = 100;  // 10Hz (§6.5)
const uint8_t MEDIAN_WINDOW = 3;               // 3회 이동 중앙값 필터 (§6.5)
const unsigned long ECHO_TIMEOUT_US = 30000UL; // 약 5m 왕복 여유 (HC-SR04 유효 사거리 ~4m)
const int MAX_RANGE_CM = 400;                  // 에코 타임아웃(무반사) 시 "사거리 내 장애물 없음"으로 간주

const unsigned long BAUD_RATE = 9600;

int frontHistory[MEDIAN_WINDOW];
int rearHistory[MEDIAN_WINDOW];
uint8_t historyIndex = 0;
bool historyFilled = false;

unsigned long lastSampleAt = 0;

// 표준 4핀 HC-SR04용 (전방) — Trig/Echo 핀이 분리된 방식
int readDistanceCm(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    return MAX_RANGE_CM; // 타임아웃: 사거리 내 장애물 없음으로 간주
  }
  return (int)(duration / 58); // 왕복 시간(us) -> cm, 음속 약 340m/s 기준 표준 변환식
}

// 3핀 개조(단일 S핀) HC-SR04용 (후방) — 같은 핀으로 트리거를 쏘고 곧바로 에코를 읽는다.
int readDistanceCmSinglePin(uint8_t sPin) {
  pinMode(sPin, OUTPUT);
  digitalWrite(sPin, LOW);
  delayMicroseconds(2);
  digitalWrite(sPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sPin, LOW);

  pinMode(sPin, INPUT);
  unsigned long duration = pulseIn(sPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    return MAX_RANGE_CM; // 타임아웃: 사거리 내 장애물 없음으로 간주 (0 아님 — 안전상 이유는 상단 주석 참고)
  }
  return (int)(duration / 58);
}

int medianOf3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { int t = a; a = b; b = t; }
  return b;
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(FRONT_TRIG_PIN, OUTPUT);
  pinMode(FRONT_ECHO_PIN, INPUT);
  digitalWrite(FRONT_TRIG_PIN, LOW);
  // REAR_PIN은 매 측정마다 readDistanceCmSinglePin()이 OUTPUT/INPUT을 직접 전환하므로
  // 여기서 미리 pinMode를 고정하지 않는다.
  Serial.println("READY");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleAt < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleAt = now;

  // 순차 트리거(전방 완료 후 후방)라 두 센서의 초음파 펄스가 겹치지 않는다.
  frontHistory[historyIndex] = readDistanceCm(FRONT_TRIG_PIN, FRONT_ECHO_PIN);
  rearHistory[historyIndex] = readDistanceCmSinglePin(REAR_PIN);
  historyIndex = (historyIndex + 1) % MEDIAN_WINDOW;
  if (historyIndex == 0) {
    historyFilled = true;
  }

  if (!historyFilled) {
    return; // 3개 샘플이 쌓이기 전에는 중앙값을 낼 수 없음
  }

  int frontCm = medianOf3(frontHistory[0], frontHistory[1], frontHistory[2]);
  int rearCm = medianOf3(rearHistory[0], rearHistory[1], rearHistory[2]);

  Serial.print("DIST:");
  Serial.println(frontCm);
  Serial.print("DIST_REAR:");
  Serial.println(rearCm);
}
