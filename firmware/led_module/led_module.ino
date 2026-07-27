// 3색 LED 모듈 (Arduino Uno R3, 로봇 탑재 — 최종적으로 초음파·홀센서와 한 보드로 통합 예정)
// 실제 하드웨어: 3색 SMD RGB LED 모듈, 공통 캐소드(GND) — LED 다리 기준 G=D9, R=D10, B=D11 (실측 배선 확정)
// 아래 RED_PIN/GREEN_PIN 이름은 LED 다리 색이 아니라 "이 핀에 쓰면 빨강/초록으로 보인다"는 기능 기준
// 공통 캐소드라 각 채널 analogWrite 값이 클수록(255에 가까울수록) 밝게 켜짐.
// 주황은 빨강 100% + 초록 약 40% 동시 점등으로 근사 (요구사항 02번 §7.1). B 채널은 사용하지 않음(항상 0).
// 시리얼 프로토콜: Pi -> Arduino "LED:<mode>\n", mode = IDLE|RUN|ARRIVE|STOP|ERROR|DOCKED (요구사항 02번 §8.3)
// 상태별 점멸 패턴: 요구사항 02번 §7.2 / 아키텍처 05번 §4.2

#include <string.h>
#include <stdlib.h>

enum LedMode {
  MODE_IDLE,
  MODE_RUN,
  MODE_ARRIVE,
  MODE_STOP,
  MODE_ERROR,
  MODE_DOCKED,
  MODE_CAL // 벤치 테스트 전용: 주황 초록 duty 실측 교정용, §8.3 프로토콜에는 없음
};

const uint8_t RED_PIN = 9;    // LED 다리 기준으로는 G — 이 핀에 duty를 주면 빨강으로 보임 (실측 확인됨)
const uint8_t GREEN_PIN = 10; // LED 다리 기준으로는 R — 이 핀에 duty를 주면 초록으로 보임 (실측 확인됨)
const uint8_t BLUE_PIN = 11;  // 실측 배선 확인됨, 현재 상태 표시에는 미사용

const uint8_t RED_DUTY = 255;
const uint8_t GREEN_DUTY = 255;
const uint8_t ORANGE_GREEN_DUTY = 18; // 빨강 100% / 초록 실측 교정값 (§7.1 최초 40% 추정치는 육안상 초록에 가까워 낮춤)

const unsigned long RUN_BLINK_MS = 500;    // 주행 중: 빨강<->주황 교대
const unsigned long ARRIVE_BLINK_MS = 500; // 도착 대기: 주황 점멸
const unsigned long ERROR_BLINK_MS = 200;  // 오류: 빠른 점멸
const unsigned long DOCKED_BLINK_MS = 300; // 도킹 완료: 문서에 시간 미지정, 벤치 테스트 임시값

const unsigned long BAUD_RATE = 9600;

const uint8_t CMD_BUFFER_SIZE = 16;
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdLength = 0;

LedMode currentMode = MODE_IDLE;
unsigned long lastToggleAt = 0;
bool blinkOn = true;

uint8_t dockedBlinkCount = 0;
bool dockedSequenceDone = false;

void setColor(uint8_t red, uint8_t green) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, 0);
}

void applyModeEntry(LedMode mode) {
  currentMode = mode;
  lastToggleAt = millis();
  dockedBlinkCount = 0;
  dockedSequenceDone = false;

  switch (mode) {
    case MODE_IDLE:
      setColor(0, GREEN_DUTY);
      blinkOn = true;
      break;
    case MODE_STOP:
      setColor(RED_DUTY, 0);
      blinkOn = true;
      break;
    case MODE_RUN:
      setColor(RED_DUTY, 0);
      blinkOn = true; // true = 빨강 단계
      break;
    case MODE_ARRIVE:
      setColor(RED_DUTY, ORANGE_GREEN_DUTY);
      blinkOn = true; // true = 점등 단계
      break;
    case MODE_ERROR:
      setColor(RED_DUTY, 0);
      blinkOn = true;
      break;
    case MODE_DOCKED:
      setColor(0, 0);
      blinkOn = false; // 소등에서 시작, 첫 토글에 점등
      break;
  }
}

void handleCommand(const char *line) {
  // 벤치 테스트 전용: "CAL:<0-255>" 입력 시 초록 duty만 그 값으로 고정하고 빨강은 100%로
  // 계속 점등 — 재업로드 없이 주황색 배합을 눈으로 맞춰볼 수 있게 함. 값이 정해지면
  // 그 숫자를 ORANGE_GREEN_DUTY에 반영하고 이 커맨드는 지워도 된다.
  const char *calPrefix = "CAL:";
  if (strncmp(line, calPrefix, strlen(calPrefix)) == 0) {
    int value = atoi(line + strlen(calPrefix));
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    currentMode = MODE_CAL;
    setColor(RED_DUTY, (uint8_t)value);
    Serial.print("CAL_GREEN=");
    Serial.println(value);
    return;
  }

  const char *prefix = "LED:";
  if (strncmp(line, prefix, strlen(prefix)) != 0) {
    return;
  }
  const char *mode = line + strlen(prefix);

  if (strcmp(mode, "IDLE") == 0) applyModeEntry(MODE_IDLE);
  else if (strcmp(mode, "RUN") == 0) applyModeEntry(MODE_RUN);
  else if (strcmp(mode, "ARRIVE") == 0) applyModeEntry(MODE_ARRIVE);
  else if (strcmp(mode, "STOP") == 0) applyModeEntry(MODE_STOP);
  else if (strcmp(mode, "ERROR") == 0) applyModeEntry(MODE_ERROR);
  else if (strcmp(mode, "DOCKED") == 0) applyModeEntry(MODE_DOCKED);
  // 정의되지 않은 mode는 무시
}

void readSerialCommand() {
  // '\n'과 '\r' 모두 줄 종료로 처리 (Arduino IDE 시리얼 모니터의 줄바꿈 설정,
  // screen 등 터미널 프로그램마다 보내는 개행 문자가 달라 둘 다 허용)
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLength > 0) {
        cmdBuffer[cmdLength] = '\0';
        handleCommand(cmdBuffer);
        cmdLength = 0;
      }
    } else if (cmdLength < CMD_BUFFER_SIZE - 1) {
      cmdBuffer[cmdLength++] = c;
    }
  }
}

void updateLed() {
  unsigned long now = millis();

  switch (currentMode) {
    case MODE_IDLE:
    case MODE_STOP:
    case MODE_CAL:
      break; // 상시 점등, 점멸 없음

    case MODE_RUN:
      if (now - lastToggleAt >= RUN_BLINK_MS) {
        lastToggleAt = now;
        blinkOn = !blinkOn;
        setColor(RED_DUTY, blinkOn ? 0 : ORANGE_GREEN_DUTY);
      }
      break;

    case MODE_ARRIVE:
      if (now - lastToggleAt >= ARRIVE_BLINK_MS) {
        lastToggleAt = now;
        blinkOn = !blinkOn;
        setColor(blinkOn ? RED_DUTY : 0, blinkOn ? ORANGE_GREEN_DUTY : 0);
      }
      break;

    case MODE_ERROR:
      if (now - lastToggleAt >= ERROR_BLINK_MS) {
        lastToggleAt = now;
        blinkOn = !blinkOn;
        setColor(blinkOn ? RED_DUTY : 0, 0);
      }
      break;

    case MODE_DOCKED:
      if (dockedSequenceDone) {
        break; // 3회 점멸 후 상시 점등 상태 유지
      }
      if (now - lastToggleAt >= DOCKED_BLINK_MS) {
        lastToggleAt = now;
        blinkOn = !blinkOn;
        setColor(0, blinkOn ? GREEN_DUTY : 0);
        if (!blinkOn) {
          dockedBlinkCount++;
          if (dockedBlinkCount >= 3) {
            dockedSequenceDone = true;
            setColor(0, GREEN_DUTY);
          }
        }
      }
      break;
  }
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  applyModeEntry(MODE_IDLE);
  Serial.println("READY");
}

void loop() {
  readSerialCommand();
  updateLed();
}
