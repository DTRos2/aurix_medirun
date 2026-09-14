// estop_firmware.ino — v1 (버튼 + 3색 LED + 부저), 9600 baud
const int LED_R  = 9;
const int LED_G  = 10;
const int LED_B  = 11;
const int BTN    = 2;
const int BUZZER = 8;

int lastBtn = HIGH;
String rxBuf = "";

void setColor(char c) {
  bool r = (c == 'R' || c == 'Y');
  bool g = (c == 'G' || c == 'Y');
  bool b = false;
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

void handleLine(const String &line) {
  if (line.startsWith("LED:") && line.length() >= 5) {
    String state = line.substring(4);
    if (state == "IDLE" || state == "RUN") {
      setColor('G');
    } else if (state == "ARRIVE") {
      setColor('Y');
    } else if (state == "STOP" || state == "ERROR") {
      setColor('R');
    } else {
      setColor(state.charAt(0));  // 기존 LED:R/G/Y 명령도 지원
    }
  } else if (line.startsWith("BUZZER:") && line.length() >= 8) {
    digitalWrite(BUZZER, line.charAt(7) == '1' ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(9600);          // ★ 9600 — serial_bridge와 일치
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  setColor('G');
  Serial.println("READY");     // ★ 부팅 1회 — 이번엔 추가함
}

void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') {
      handleLine(rxBuf);
      rxBuf = "";
    } else if (ch != '\r') {
      rxBuf += ch;
    }
  }
  int b = digitalRead(BTN);
  if (b != lastBtn) {
    Serial.print("BTN:");
    Serial.println(b == LOW ? 1 : 0);
    lastBtn = b;
    delay(20);
  }
}
