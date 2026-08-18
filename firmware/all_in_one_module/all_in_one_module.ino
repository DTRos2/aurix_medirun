// 단일 보드 통합 펌웨어 (Arduino Uno R3)
//
// 한 개 보드에서 아래 기능을 모두 함께 검증한다.
// - SOS 버튼 3개
// - 전방/후방 초음파
// - 홀센서 적재 감지
// - RGB LED 상태 표시
//
// 시리얼 출력
// - READY
// - BTN_01 / BTN_02 / BTN_03
// - DIST:<cm>
// - DIST_REAR:<cm>
// - HALL:<0|1>
//
// 시리얼 입력
// - LED:IDLE|RUN|ARRIVE|STOP|ERROR|DOCKED
// - CAL:<0-255>
//
// 핀맵
// - 버튼: A1, A2, A3 (INPUT_PULLUP)
// - 전방 초음파: D2 TRIG, D3 ECHO
// - 후방 초음파: D5 TRIG, D6 ECHO
// - RGB LED: D9, D10, D11
// - 홀센서: A0

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum LedMode {
  MODE_IDLE,
  MODE_RUN,
  MODE_ARRIVE,
  MODE_STOP,
  MODE_ERROR,
  MODE_DOCKED,
  MODE_CAL
};

// SOS 버튼
const uint8_t BUTTON_COUNT = 3;
const uint8_t BUTTON_PINS[BUTTON_COUNT] = {A1, A2, A3};
const char *BUTTON_IDS[BUTTON_COUNT] = {"BTN_01", "BTN_02", "BTN_03"};
const unsigned long BUTTON_DEBOUNCE_MS = 50;
bool buttonRawState[BUTTON_COUNT];
bool buttonStableState[BUTTON_COUNT];
unsigned long buttonLastChangeAt[BUTTON_COUNT];

// 초음파
const uint8_t FRONT_TRIG_PIN = 2;
const uint8_t FRONT_ECHO_PIN = 3;
const uint8_t REAR_TRIG_PIN = 5;
const uint8_t REAR_ECHO_PIN = 6;
const unsigned long SAMPLE_INTERVAL_MS = 100;
const uint8_t MEDIAN_WINDOW = 3;
const unsigned long ECHO_TIMEOUT_US = 30000UL;
const int MAX_RANGE_CM = 400;
// HC-SR04류는 물체가 몇 cm 이내로 바짝 붙으면 반사 펄스가 너무 짧아 pulseIn()이 아예
// 못 잡고 타임아웃난다 — "사거리 밖(멀다)"과 "너무 가까워 못 잡음"이 똑같이 duration==0으로
// 나와 이 신호 하나로는 구분이 안 된다. 물체가 순간이동하듯 사라지진 않으므로, 직전
// 실측값이 이미 가까웠다면(RECENT_CLOSE_THRESHOLD_CM 이내) 그 다음 타임아웃은 "더
// 가까워져서 못 잡힌 것"으로 보고 작은 값을 보고한다. 직전 값이 멀었거나 아직 없으면
// (기동 직후) 기존대로 "사거리 밖"으로 본다 — 열린 공간에서 매번 안전정지가 걸리는
// 걸 막기 위함. (실제로 로봇 바로 앞에 바짝 붙였는데 DIST:400이 나와 안전정지가 안
// 걸리는 게 실측으로 확인된 문제 — 이 이력 기반 판단으로 고친다.)
const int RECENT_CLOSE_THRESHOLD_CM = 25;
const int ECHO_LOST_FALLBACK_CM = 5;
int frontHistory[MEDIAN_WINDOW];
int rearHistory[MEDIAN_WINDOW];
int frontLastCm = MAX_RANGE_CM;
int rearLastCm = MAX_RANGE_CM;
uint8_t historyIndex = 0;
bool historyFilled = false;
unsigned long lastSampleAt = 0;

// LED
const uint8_t RED_PIN = 9;
const uint8_t GREEN_PIN = 10;
const uint8_t BLUE_PIN = 11;
const uint8_t RED_DUTY = 255;
const uint8_t GREEN_DUTY = 255;
const uint8_t ORANGE_GREEN_DUTY = 18;
const unsigned long RUN_BLINK_MS = 500;
const unsigned long ARRIVE_BLINK_MS = 500;
const unsigned long ERROR_BLINK_MS = 200;
const unsigned long DOCKED_BLINK_MS = 300;

LedMode currentMode = MODE_IDLE;
unsigned long lastToggleAt = 0;
bool blinkOn = true;
uint8_t dockedBlinkCount = 0;
bool dockedSequenceDone = false;

// 홀센서
const uint8_t HALL_PIN = A0;
const int MAGNET_RAW_MAX = 506;
const unsigned long HALL_DEBOUNCE_MS = 500;
// HALL:은 원래 상태가 바뀔 때만 보내는 edge-triggered 값이었는데, 그러면 어쩌다
// 한 줄이 시리얼 노이즈로 깨지거나 유실됐을 때 다음 변화가 생기기 전까지(자석이
// 계속 그대로면 영영) GUI가 값을 못 받는다. DIST/DIST_REAR처럼 주기적으로도
// 같이 보내서 한 번 놓쳐도 최대 HALL_REPORT_INTERVAL_MS 안에 자동 복구되게 한다.
const unsigned long HALL_REPORT_INTERVAL_MS = 1000;
bool hallRawState = false;
bool hallStableState = false;
unsigned long hallLastChangeAt = 0;
unsigned long hallLastReportAt = 0;

// 시리얼
const unsigned long BAUD_RATE = 9600;
const uint8_t CMD_BUFFER_SIZE = 24;
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdLength = 0;

int readDistanceCm(uint8_t trigPin, uint8_t echoPin, int &lastKnownCm) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    if (lastKnownCm <= RECENT_CLOSE_THRESHOLD_CM) {
      return ECHO_LOST_FALLBACK_CM;
    }
    return MAX_RANGE_CM;
  }

  int distanceCm = (int)(duration / 58);
  if (distanceCm > MAX_RANGE_CM) {
    distanceCm = MAX_RANGE_CM;
  }
  lastKnownCm = distanceCm;
  return distanceCm;
}

int medianOf3(int a, int b, int c) {
  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }
  if (b > c) {
    int temp = b;
    b = c;
    c = temp;
  }
  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }
  return b;
}

bool isMagnetPresent(int value) {
  return value <= MAGNET_RAW_MAX;
}

int readSmoothedHallValue() {
  const uint8_t SAMPLES = 8;
  long sum = 0;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sum += analogRead(HALL_PIN);
  }
  return (int)(sum / SAMPLES);
}

void reportHallState(bool magnetPresent) {
  Serial.print("HALL:");
  Serial.println(magnetPresent ? 1 : 0);
}

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
      blinkOn = true;
      break;
    case MODE_ARRIVE:
      setColor(RED_DUTY, ORANGE_GREEN_DUTY);
      blinkOn = true;
      break;
    case MODE_ERROR:
      setColor(RED_DUTY, 0);
      blinkOn = true;
      break;
    case MODE_DOCKED:
      setColor(0, 0);
      blinkOn = false;
      break;
    case MODE_CAL:
      blinkOn = true;
      break;
  }
}

void handleCommand(const char *line) {
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

  if (strcmp(mode, "IDLE") == 0) {
    applyModeEntry(MODE_IDLE);
    Serial.println("LED_MODE:IDLE");
  }
  else if (strcmp(mode, "RUN") == 0) {
    applyModeEntry(MODE_RUN);
    Serial.println("LED_MODE:RUN");
  }
  else if (strcmp(mode, "ARRIVE") == 0) {
    applyModeEntry(MODE_ARRIVE);
    Serial.println("LED_MODE:ARRIVE");
  }
  else if (strcmp(mode, "STOP") == 0) {
    applyModeEntry(MODE_STOP);
    Serial.println("LED_MODE:STOP");
  }
  else if (strcmp(mode, "ERROR") == 0) {
    applyModeEntry(MODE_ERROR);
    Serial.println("LED_MODE:ERROR");
  }
  else if (strcmp(mode, "DOCKED") == 0) {
    applyModeEntry(MODE_DOCKED);
    Serial.println("LED_MODE:DOCKED");
  }
}

void readSerialCommand() {
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
      break;

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
        // 3회 점멸 완료 후 계속 소등 상태 유지
        setColor(0, 0);
        break;
      }

      if (now - lastToggleAt >= DOCKED_BLINK_MS) {
        lastToggleAt = now;
        blinkOn = !blinkOn;

        if (blinkOn) {
          // 초록색 점등
          setColor(0, GREEN_DUTY);
        } else {
          // 초록색 소등
          setColor(0, 0);

          // 켜졌다가 꺼진 시점을 1회 점멸로 계산
          dockedBlinkCount++;

          if (dockedBlinkCount >= 3) {
            dockedSequenceDone = true;
            setColor(0, 0);
            Serial.println("DOCKED_BLINK_DONE");
          }
        }
      }
      break;
  }
}

void updateSosButtons() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    bool reading = digitalRead(BUTTON_PINS[i]);

    if (reading != buttonRawState[i]) {
      buttonRawState[i] = reading;
      buttonLastChangeAt[i] = now;
    }

    if (now - buttonLastChangeAt[i] >= BUTTON_DEBOUNCE_MS &&
        buttonStableState[i] != buttonRawState[i]) {
      buttonStableState[i] = buttonRawState[i];
      if (buttonStableState[i] == LOW) {
        Serial.println(BUTTON_IDS[i]);
      }
    }
  }
}

void updateUltrasonic() {
  unsigned long now = millis();
  if (now - lastSampleAt < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleAt = now;

  frontHistory[historyIndex] = readDistanceCm(FRONT_TRIG_PIN, FRONT_ECHO_PIN, frontLastCm);
  delay(10);
  rearHistory[historyIndex] = readDistanceCm(REAR_TRIG_PIN, REAR_ECHO_PIN, rearLastCm);

  historyIndex = (historyIndex + 1) % MEDIAN_WINDOW;
  if (historyIndex == 0) {
    historyFilled = true;
  }
  if (!historyFilled) {
    return;
  }

  int frontCm = medianOf3(frontHistory[0], frontHistory[1], frontHistory[2]);
  int rearCm = medianOf3(rearHistory[0], rearHistory[1], rearHistory[2]);

  Serial.print("DIST:");
  Serial.println(frontCm);
  Serial.print("DIST_REAR:");
  Serial.println(rearCm);
}

void updateHall() {
  int value = readSmoothedHallValue();
  unsigned long now = millis();
  bool reading = isMagnetPresent(value);

  if (reading != hallRawState) {
    hallRawState = reading;
    hallLastChangeAt = now;
  }

  if (now - hallLastChangeAt >= HALL_DEBOUNCE_MS && hallStableState != hallRawState) {
    hallStableState = hallRawState;
    reportHallState(hallStableState);
    hallLastReportAt = now;
  } else if (now - hallLastReportAt >= HALL_REPORT_INTERVAL_MS) {
    reportHallState(hallStableState);
    hallLastReportAt = now;
  }
}

void setup() {
  Serial.begin(BAUD_RATE);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    buttonRawState[i] = digitalRead(BUTTON_PINS[i]);
    buttonStableState[i] = buttonRawState[i];
    buttonLastChangeAt[i] = millis();
  }

  pinMode(FRONT_TRIG_PIN, OUTPUT);
  pinMode(FRONT_ECHO_PIN, INPUT);
  pinMode(REAR_TRIG_PIN, OUTPUT);
  pinMode(REAR_ECHO_PIN, INPUT);
  digitalWrite(FRONT_TRIG_PIN, LOW);
  digitalWrite(REAR_TRIG_PIN, LOW);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  hallRawState = isMagnetPresent(readSmoothedHallValue());
  hallStableState = hallRawState;
  hallLastChangeAt = millis();
  hallLastReportAt = millis();

  applyModeEntry(MODE_IDLE);

  Serial.println("READY");
  reportHallState(hallStableState);
}

void loop() {
  readSerialCommand();
  updateLed();
  updateSosButtons();
  updateUltrasonic();
  updateHall();
}
