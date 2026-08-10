#include <WiFi.h>
#include <ArduinoMqttClient.h>

// =====================================================
// ESP32 #1
// 콜존 ID : BTN_01
// =====================================================

// -----------------------------------------------------
// Wi-Fi 설정
// 본인 Wi-Fi 이름 / 비밀번호로 변경
// -----------------------------------------------------
const char* WIFI_SSID = "DYNC_2.4G";
const char* WIFI_PASSWORD = "";

// -----------------------------------------------------
// MQTT Broker 설정
// 라즈베리파이 IP
// -----------------------------------------------------
const char* MQTT_BROKER = "192.168.230.9";
const int MQTT_PORT = 1883;

// MQTT Topic
const char* MQTT_TOPIC = "medirun/button";

// -----------------------------------------------------
// 이 ESP32에 할당된 콜존 웨이포인트 ID
// -----------------------------------------------------
const char* CALLZONE_ID = "BTN_01";

// -----------------------------------------------------
// 버튼 핀
// ESP32 보드의 D25
// -----------------------------------------------------
const int BUTTON_PIN = 25;

// 디바운스
const unsigned long DEBOUNCE_DELAY = 200;

unsigned long lastButtonTime = 0;

// 버튼 이전 상태
int previousButtonState = HIGH;

// -----------------------------------------------------
// Wi-Fi / MQTT 객체
// -----------------------------------------------------
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


// =====================================================
// Wi-Fi 연결
// =====================================================
void connectWiFi()
{
  Serial.println();
  Serial.print("[WiFi] 연결 중: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WiFi] 연결 완료");

  Serial.print("[WiFi] ESP32 IP: ");
  Serial.println(WiFi.localIP());
}


// =====================================================
// MQTT 연결
// =====================================================
void connectMQTT()
{
  Serial.println();
  Serial.print("[MQTT] Broker 연결 중: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  while (!mqttClient.connected())
  {
    if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT))
    {
      Serial.print("[MQTT] 연결 실패 / Error: ");
      Serial.println(mqttClient.connectError());

      delay(2000);
    }
    else
    {
      Serial.println("[MQTT] 연결 완료");
    }
  }
}


// =====================================================
// BTN_01 MQTT 전송
// =====================================================
void publishButtonEvent()
{
  Serial.println();
  Serial.println("=============================");
  Serial.println("[BUTTON] 버튼 1 눌림");

  Serial.print("[MQTT] 전송 Topic : ");
  Serial.println(MQTT_TOPIC);

  Serial.print("[MQTT] 전송 Payload : ");
  Serial.println(CALLZONE_ID);

  Serial.println("=============================");

  mqttClient.beginMessage(MQTT_TOPIC);

  mqttClient.print(CALLZONE_ID);

  mqttClient.endMessage();
}


// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" AURIX MediRun ESP32 Call Button");
  Serial.println(" Device : ESP32 #1");
  Serial.println(" CallZone: BTN_01");
  Serial.println("=================================");

  // D25(GPIO25)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Wi-Fi 연결
  connectWiFi();

  // MQTT 연결
  connectMQTT();

  Serial.println();
  Serial.println("[SYSTEM] BTN_01 호출 버튼 준비 완료");
}


// =====================================================
// LOOP
// =====================================================
void loop()
{
  // MQTT 연결 유지
  mqttClient.poll();


  // ---------------------------------------------------
  // Wi-Fi 연결 확인
  // ---------------------------------------------------
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] 연결 끊김 → 재연결");

    connectWiFi();
  }


  // ---------------------------------------------------
  // MQTT 연결 확인
  // ---------------------------------------------------
  if (!mqttClient.connected())
  {
    Serial.println("[MQTT] 연결 끊김 → 재연결");

    connectMQTT();
  }


  // ---------------------------------------------------
  // 버튼 상태 읽기
  // ---------------------------------------------------
  int buttonState = digitalRead(BUTTON_PIN);


  // ---------------------------------------------------
  // HIGH → LOW 순간 감지
  // ---------------------------------------------------
  if (buttonState == LOW &&
      previousButtonState == HIGH)
  {
    // 채터링 방지
    if (millis() - lastButtonTime > DEBOUNCE_DELAY)
    {
      publishButtonEvent();

      lastButtonTime = millis();
    }
  }


  // 이전 상태 저장
  previousButtonState = buttonState;

  delay(10);
}
