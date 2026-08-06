#include <string.h>
#include <stdlib.h>

// =====================================================
// AURIX MediRun Arduino Uno 통합 펌웨어
//
// 포함 기능
// 1. RGB LED
// 2. 전방 초음파 센서
// 3. 후방 단일 신호 초음파 센서
// 4. SS49E 아날로그 홀센서
// 5. 호출 버튼 2개
//
// Arduino -> Raspberry Pi
// BUTTON:1
// BUTTON:2
// FRONT:25.3
// REAR:14.8
// HALL:1
// HALL:0
//
// Raspberry Pi -> Arduino
// LED:IDLE
// LED:RUN
// LED:ARRIVE
// LED:STOP
// LED:ERROR
// LED:DOCKED
// STATUS?
// =====================================================


// =====================================================
// 핀 설정
// =====================================================

// 전방 초음파 센서
constexpr uint8_t FRONT_TRIG_PIN = 4;
constexpr uint8_t FRONT_ECHO_PIN = 5;

// 후방 단일 신호 초음파 센서
constexpr uint8_t REAR_SIGNAL_PIN = 6;

// 버튼
constexpr uint8_t BUTTON_1_PIN = 7;
constexpr uint8_t BUTTON_2_PIN = 8;

// RGB LED
// 기존 배선:
// G = D9
// R = D10
// B = D11
constexpr uint8_t GREEN_LED_PIN = 9;
constexpr uint8_t RED_LED_PIN = 10;
constexpr uint8_t BLUE_LED_PIN = 11;

// 홀센서
constexpr uint8_t HALL_SENSOR_PIN = A0;


// =====================================================
// 버튼 설정
// =====================================================

// 일반적인 버튼 센서 모듈은 눌렀을 때 HIGH를 출력합니다.
// 반대로 작동하면 해당 값을 LOW로 바꾸세요.
constexpr uint8_t BUTTON_1_PRESSED_LEVEL = HIGH;
constexpr uint8_t BUTTON_2_PRESSED_LEVEL = HIGH;

// 접점 튐 방지
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;

// 버튼을 연속으로 눌렀을 때 중복 호출 방지
constexpr unsigned long BUTTON_LOCKOUT_MS = 1000;

struct ButtonState
{
  uint8_t pin;
  uint8_t buttonId;
  uint8_t pressedLevel;

  uint8_t stableState;
  uint8_t previousRawState;

  unsigned long lastRawChangeTime;
  unsigned long lastPressedTime;
};

ButtonState buttons[] = {
  {
    BUTTON_1_PIN,
    1,
    BUTTON_1_PRESSED_LEVEL,
    LOW,
    LOW,
    0,
    0
  },
  {
    BUTTON_2_PIN,
    2,
    BUTTON_2_PRESSED_LEVEL,
    LOW,
    LOW,
    0,
    0
  }
};

constexpr size_t BUTTON_COUNT =
  sizeof(buttons) / sizeof(buttons[0]);


// =====================================================
// 초음파 센서 설정
// =====================================================

constexpr unsigned long ULTRASONIC_INTERVAL_MS = 300;
constexpr unsigned long ULTRASONIC_TIMEOUT_US = 30000;

constexpr float MIN_DISTANCE_CM = 2.0f;
constexpr float MAX_DISTANCE_CM = 400.0f;

unsigned long lastUltrasonicTime = 0;

float frontDistanceCm = -1.0f;
float rearDistanceCm = -1.0f;


// =====================================================
// 홀센서 설정
// =====================================================

// 자석이 없는 상태에서 자동 측정하는 횟수
constexpr int HALL_CALIBRATION_COUNT = 100;

// 기준값과 현재 값의 차이가 이 값 이상이면 자석 감지
// 실제 센서 값에 맞춰 30~150 사이로 조정할 수 있습니다.
constexpr int HALL_THRESHOLD = 80;

// 상태가 500ms 이상 유지되어야 변경 확정
constexpr unsigned long HALL_DEBOUNCE_MS = 500;

// true로 바꾸면 홀센서 원시값을 출력합니다.
constexpr bool PRINT_HALL_RAW = false;

int hallBaseline = 512;

bool hallDetected = false;
bool hallCandidateState = false;

unsigned long hallCandidateStartTime = 0;
unsigned long lastHallRawPrintTime = 0;


// =====================================================
// LED 설정
// =====================================================

enum LedMode
{
  MODE_IDLE,
  MODE_RUN,
  MODE_ARRIVE,
  MODE_STOP,
  MODE_ERROR,
  MODE_DOCKED
};

LedMode currentLedMode = MODE_IDLE;

unsigned long lastErrorBlinkTime = 0;
bool errorLedOn = false;

unsigned long dockedBlinkTime = 0;
uint8_t dockedBlinkCount = 0;
bool dockedLedOn = false;
bool dockedSequenceCompleted = false;


// =====================================================
// 시리얼 입력 버퍼
// =====================================================

constexpr size_t SERIAL_BUFFER_SIZE = 64;

char serialBuffer[SERIAL_BUFFER_SIZE];
size_t serialBufferIndex = 0;


// =====================================================
// RGB LED 함수
// 공통 캐소드 방식
// 0 = 꺼짐, 255 = 최대 밝기
// =====================================================

void setRgb(
  uint8_t red,
  uint8_t green,
  uint8_t blue)
{
  analogWrite(RED_LED_PIN, red);
  analogWrite(GREEN_LED_PIN, green);
  analogWrite(BLUE_LED_PIN, blue);
}

void turnOffLed()
{
  setRgb(0, 0, 0);
}

void setLedMode(LedMode mode)
{
  currentLedMode = mode;

  errorLedOn = false;
  lastErrorBlinkTime = millis();

  dockedBlinkCount = 0;
  dockedLedOn = false;
  dockedSequenceCompleted = false;
  dockedBlinkTime = millis();

  switch (currentLedMode) {
    case MODE_IDLE:
      // 대기: 파란색
      setRgb(0, 0, 150);
      break;

    case MODE_RUN:
      // 운행: 주황색
      setRgb(255, 100, 0);
      break;

    case MODE_ARRIVE:
      // 도착: 초록색
      setRgb(0, 255, 0);
      break;

    case MODE_STOP:
      // 정지: 노란색
      setRgb(255, 180, 0);
      break;

    case MODE_ERROR:
      // 오류: 빨간색 점멸
      turnOffLed();
      break;

    case MODE_DOCKED:
      // 도킹: 초록색 3회 점멸 후 소등
      turnOffLed();
      break;
  }
}

void updateLed()
{
  const unsigned long now = millis();

  // 오류 모드: 빨간색 반복 점멸
  if (currentLedMode == MODE_ERROR) {
    if (now - lastErrorBlinkTime >= 300) {
      lastErrorBlinkTime = now;
      errorLedOn = !errorLedOn;

      if (errorLedOn) {
        setRgb(255, 0, 0);
      } else {
        turnOffLed();
      }
    }

    return;
  }

  // 도킹 모드가 아니면 추가 처리 없음
  if (currentLedMode != MODE_DOCKED) {
    return;
  }

  // 초록색 3회 점멸이 끝난 상태
  if (dockedSequenceCompleted) {
    turnOffLed();
    return;
  }

  if (now - dockedBlinkTime < 300) {
    return;
  }

  dockedBlinkTime = now;
  dockedLedOn = !dockedLedOn;

  if (dockedLedOn) {
    setRgb(0, 255, 0);
  } else {
    turnOffLed();
    dockedBlinkCount++;

    if (dockedBlinkCount >= 3) {
      dockedSequenceCompleted = true;
      turnOffLed();
    }
  }
}


// =====================================================
// 버튼 처리
// =====================================================

void initializeButtons()
{
  // OUT/VCC/GND 또는 S/+/− 모듈이므로
  // 모듈에 내장된 저항을 사용하여 INPUT 설정
  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT);

  delay(20);

  for (size_t i = 0; i < BUTTON_COUNT; ++i) {
    const uint8_t initialState =
      digitalRead(buttons[i].pin);

    buttons[i].stableState = initialState;
    buttons[i].previousRawState = initialState;
    buttons[i].lastRawChangeTime = millis();
    buttons[i].lastPressedTime = 0;
  }
}

void sendButtonEvent(uint8_t buttonId)
{
  Serial.print("BUTTON:");
  Serial.println(buttonId);
}

void updateButtons()
{
  const unsigned long now = millis();

  for (size_t i = 0; i < BUTTON_COUNT; ++i) {
    ButtonState &button = buttons[i];

    const uint8_t rawState =
      digitalRead(button.pin);

    // 입력값이 변하면 디바운싱 시간 측정 시작
    if (rawState != button.previousRawState) {
      button.previousRawState = rawState;
      button.lastRawChangeTime = now;
    }

    // 입력이 50ms 이상 안정적으로 유지되는지 확인
    if (
      now - button.lastRawChangeTime <
      BUTTON_DEBOUNCE_MS)
    {
      continue;
    }

    // 기존 안정 상태와 같으면 변화 없음
    if (rawState == button.stableState) {
      continue;
    }

    button.stableState = rawState;

    const bool isPressed =
      button.stableState == button.pressedLevel;

    // 버튼을 놓은 상태는 전송하지 않음
    if (!isPressed) {
      continue;
    }

    // 1초 이내 중복 입력 방지
    if (
      button.lastPressedTime != 0 &&
      now - button.lastPressedTime <
      BUTTON_LOCKOUT_MS)
    {
      continue;
    }

    button.lastPressedTime = now;

    sendButtonEvent(button.buttonId);
  }
}


// =====================================================
// 전방 초음파 센서
// TRIG와 ECHO가 분리된 일반 HC-SR04 방식
// =====================================================

float readFrontUltrasonic()
{
  digitalWrite(FRONT_TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(FRONT_TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(FRONT_TRIG_PIN, LOW);

  const unsigned long duration = pulseIn(
    FRONT_ECHO_PIN,
    HIGH,
    ULTRASONIC_TIMEOUT_US);

  if (duration == 0) {
    return -1.0f;
  }

  const float distance =
    static_cast<float>(duration) * 0.0343f / 2.0f;

  if (
    distance < MIN_DISTANCE_CM ||
    distance > MAX_DISTANCE_CM)
  {
    return -1.0f;
  }

  return distance;
}


// =====================================================
// 후방 초음파 센서
// Signal 핀 하나를 OUTPUT과 INPUT으로 전환
// =====================================================

float readRearUltrasonic()
{
  pinMode(REAR_SIGNAL_PIN, OUTPUT);

  digitalWrite(REAR_SIGNAL_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(REAR_SIGNAL_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(REAR_SIGNAL_PIN, LOW);

  pinMode(REAR_SIGNAL_PIN, INPUT);

  const unsigned long duration = pulseIn(
    REAR_SIGNAL_PIN,
    HIGH,
    ULTRASONIC_TIMEOUT_US);

  if (duration == 0) {
    return -1.0f;
  }

  const float distance =
    static_cast<float>(duration) * 0.0343f / 2.0f;

  if (
    distance < MIN_DISTANCE_CM ||
    distance > MAX_DISTANCE_CM)
  {
    return -1.0f;
  }

  return distance;
}

void printDistance(
  const char *sensorName,
  float distance)
{
  Serial.print(sensorName);
  Serial.print(":");

  if (distance < 0.0f) {
    Serial.println("-1");
  } else {
    Serial.println(distance, 1);
  }
}

void updateUltrasonicSensors()
{
  const unsigned long now = millis();

  if (
    now - lastUltrasonicTime <
    ULTRASONIC_INTERVAL_MS)
  {
    return;
  }

  lastUltrasonicTime = now;

  frontDistanceCm = readFrontUltrasonic();

  // 전방과 후방 초음파 간섭 방지
  delay(30);

  rearDistanceCm = readRearUltrasonic();

  printDistance("FRONT", frontDistanceCm);
  printDistance("REAR", rearDistanceCm);
}


// =====================================================
// 홀센서 처리
// =====================================================

void calibrateHallSensor()
{
  long total = 0;

  // 전원을 켤 때 자석을 센서에서 떨어뜨린 상태로 유지
  for (int i = 0; i < HALL_CALIBRATION_COUNT; ++i) {
    total += analogRead(HALL_SENSOR_PIN);
    delay(5);
  }

  hallBaseline =
    static_cast<int>(
      total / HALL_CALIBRATION_COUNT);

  Serial.print("HALL_BASELINE:");
  Serial.println(hallBaseline);
}

bool readHallSensor()
{
  const int rawValue =
    analogRead(HALL_SENSOR_PIN);

  const int difference =
    abs(rawValue - hallBaseline);

  return difference >= HALL_THRESHOLD;
}

void updateHallSensor()
{
  const unsigned long now = millis();

  const int rawValue =
    analogRead(HALL_SENSOR_PIN);

  const bool detected =
    abs(rawValue - hallBaseline) >=
    HALL_THRESHOLD;

  if (PRINT_HALL_RAW) {
    if (now - lastHallRawPrintTime >= 500) {
      lastHallRawPrintTime = now;

      Serial.print("HALL_RAW:");
      Serial.print(rawValue);

      Serial.print(" BASE:");
      Serial.print(hallBaseline);

      Serial.print(" DIFF:");
      Serial.println(
        abs(rawValue - hallBaseline));
    }
  }

  // 후보 상태가 변경되면 유지 시간 다시 측정
  if (detected != hallCandidateState) {
    hallCandidateState = detected;
    hallCandidateStartTime = now;
    return;
  }

  // 상태가 500ms 이상 유지됐는지 확인
  if (
    now - hallCandidateStartTime <
    HALL_DEBOUNCE_MS)
  {
    return;
  }

  // 이전 확정 상태와 같으면 출력하지 않음
  if (hallDetected == hallCandidateState) {
    return;
  }

  hallDetected = hallCandidateState;

  Serial.print("HALL:");
  Serial.println(hallDetected ? 1 : 0);
}


// =====================================================
// 현재 상태 출력
// =====================================================

void printCurrentStatus()
{
  Serial.print("STATUS:");

  Serial.print("BUTTON1=");
  Serial.print(digitalRead(BUTTON_1_PIN));

  Serial.print(",BUTTON2=");
  Serial.print(digitalRead(BUTTON_2_PIN));

  Serial.print(",HALL=");
  Serial.print(hallDetected ? 1 : 0);

  Serial.print(",FRONT=");
  Serial.print(frontDistanceCm, 1);

  Serial.print(",REAR=");
  Serial.println(rearDistanceCm, 1);
}


// =====================================================
// Raspberry Pi에서 들어오는 명령 처리
// =====================================================

void processSerialCommand(const char *command)
{
  if (strcmp(command, "LED:IDLE") == 0) {
    setLedMode(MODE_IDLE);
    Serial.println("ACK:LED:IDLE");
    return;
  }

  if (strcmp(command, "LED:RUN") == 0) {
    setLedMode(MODE_RUN);
    Serial.println("ACK:LED:RUN");
    return;
  }

  if (strcmp(command, "LED:ARRIVE") == 0) {
    setLedMode(MODE_ARRIVE);
    Serial.println("ACK:LED:ARRIVE");
    return;
  }

  if (strcmp(command, "LED:STOP") == 0) {
    setLedMode(MODE_STOP);
    Serial.println("ACK:LED:STOP");
    return;
  }

  if (strcmp(command, "LED:ERROR") == 0) {
    setLedMode(MODE_ERROR);
    Serial.println("ACK:LED:ERROR");
    return;
  }

  if (strcmp(command, "LED:DOCKED") == 0) {
    setLedMode(MODE_DOCKED);
    Serial.println("ACK:LED:DOCKED");
    return;
  }

  if (strcmp(command, "STATUS?") == 0) {
    printCurrentStatus();
    return;
  }

  Serial.print("ERROR:UNKNOWN_COMMAND:");
  Serial.println(command);
}


// =====================================================
// 시리얼 입력 처리
// =====================================================

void updateSerialInput()
{
  while (Serial.available() > 0) {
    const char received = Serial.read();

    // CR 문자는 무시
    if (received == '\r') {
      continue;
    }

    // LF가 들어오면 한 명령 완성
    if (received == '\n') {
      serialBuffer[serialBufferIndex] = '\0';

      if (serialBufferIndex > 0) {
        processSerialCommand(serialBuffer);
      }

      serialBufferIndex = 0;
      continue;
    }

    if (
      serialBufferIndex <
      SERIAL_BUFFER_SIZE - 1)
    {
      serialBuffer[serialBufferIndex++] =
        received;
    } else {
      serialBufferIndex = 0;

      Serial.println(
        "ERROR:SERIAL_BUFFER_OVERFLOW");
    }
  }
}


// =====================================================
// 초기화
// =====================================================

void setup()
{
  Serial.begin(115200);

  // RGB LED 초기화
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  turnOffLed();

  // 버튼 초기화
  initializeButtons();

  // 전방 초음파 초기화
  pinMode(FRONT_TRIG_PIN, OUTPUT);
  pinMode(FRONT_ECHO_PIN, INPUT);

  digitalWrite(FRONT_TRIG_PIN, LOW);

  // 후방 초음파 초기화
  pinMode(REAR_SIGNAL_PIN, INPUT);

  // 홀센서 초기화
  pinMode(HALL_SENSOR_PIN, INPUT);

  delay(500);

  // 자석이 없는 상태를 기준값으로 측정
  calibrateHallSensor();

  hallDetected = readHallSensor();
  hallCandidateState = hallDetected;
  hallCandidateStartTime = millis();

  // 초기 LED 상태
  setLedMode(MODE_IDLE);

  Serial.println("AURIX_READY");
  Serial.println(
    "BUTTON_MAP:1=WAYPOINT_1,2=WAYPOINT_2");

  printCurrentStatus();
}


// =====================================================
// 반복 실행
// =====================================================

void loop()
{
  updateSerialInput();

  updateButtons();

  updateHallSensor();

  updateUltrasonicSensors();

  updateLed();
}
