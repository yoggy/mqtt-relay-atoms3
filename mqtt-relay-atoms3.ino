#include <M5Unified.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include "config.h"

WiFiClient wifi_client;
void mqtt_sub_callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqtt_client(mqtt_host, mqtt_port, mqtt_sub_callback, wifi_client);

unsigned long last_updated_time = 0;

#define OUTPUT_PIN_G1 1

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 9600;
  M5.begin(cfg);

  M5.Display.setTextSize(3);
  M5.Display.setBrightness(32);

  //
  pinMode(OUTPUT_PIN_G1, OUTPUT);
  digitalWrite(OUTPUT_PIN_G1, LOW);

  // Wifi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setSleep(false);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    switch (count % 4) {
      case 0:
        Serial.println("|");
        M5.Display.fillRect(0, 0, 16, 16, TFT_YELLOW);
        break;
      case 1:
        Serial.println("/");
        break;
      case 2:
        M5.Display.fillRect(0, 0, 16, 16, TFT_BLACK);
        Serial.println("-");
        break;
      case 3:
        Serial.println("\\");
        break;
    }
    count++;
    if (count >= 240) reboot();  // 240 / 4 = 60sec
  }
  M5.Display.fillRect(0, 0, 16, 16, TFT_YELLOW);
  Serial.println("WiFi connected!");
  delay(1000);

  // MQTT
  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  } else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    Serial.println("mqtt connecting failed...");
    reboot();
  }
  Serial.println("MQTT connected!");
  delay(1000);

  M5.Display.fillRect(0, 0, 16, 16, TFT_BLUE);

  mqtt_client.subscribe(mqtt_onair_data_topic);

  // configTime
  configTime(9 * 3600, 0, "ntp.nict.jp");
  struct tm t;
  if (!getLocalTime(&t)) {
    Serial.println("getLocalTime() failed...");
    delay(1000);
    reboot();
  }
  Serial.println("configTime() success!");
  M5.Display.fillRect(0, 0, 16, 16, TFT_GREEN);
  M5.Display.drawString("MQTT", 16, 30);
  M5.Display.drawString("RELAY", 16, 60);

  delay(1000);

  last_updated_time = millis();
}

void reboot() {
  Serial.println("REBOOT!!!!!");
  for (int i = 0; i < 30; ++i) {
    M5.Display.fillRect(0, 0, 16, 16, TFT_MAGENTA);
    delay(100);
    M5.Display.fillRect(0, 0, 16, 16, TFT_BLACK);
    delay(100);
  }

  ESP.restart();
}

static bool is_onair = false;

long remaining_time = 0;
bool relay_enable = false;
long relay_enable_time = 3000; // 3sec

void loop() {
  delay(100); // We need slow loop intervals.

  unsigned long delta_time = millis() - last_updated_time;
  if (delta_time > 1000000000) {
    last_updated_time = millis();
    return;
  }

  if (remaining_time > 0) {
    remaining_time -= delta_time;
    if (remaining_time < 0) remaining_time = 0;

    if (relay_enable == false) {
      Serial.println("relay on");
      M5.Display.clear(TFT_RED);
      M5.Display.setBrightness(255);
    }
    relay_enable = true;
  }
  else {
    if (relay_enable == true) {
      Serial.println("relay off");
      M5.Display.clear(TFT_BLACK);
      M5.Display.fillRect(0, 0, 16, 16, TFT_GREEN);
      M5.Display.drawString("MQTT", 16, 30);
      M5.Display.drawString("RELAY", 16, 60);
      M5.Display.setBrightness(32);
    }
    relay_enable = false;
  }

  if (relay_enable) {
    digitalWrite(OUTPUT_PIN_G1, HIGH);
  }
  else {
    digitalWrite(OUTPUT_PIN_G1, LOW);
  }

  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected...");
    reboot();
  }

  M5.update();

  if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000)) {
    remaining_time += relay_enable_time;
  }

  last_updated_time = millis();
}

#define BUF_LEN 256
char buf[BUF_LEN];

void mqtt_sub_callback(char* topic, byte* payload, unsigned int length) {
  int len = BUF_LEN - 1 < length ? (BUF_LEN - 1) : length;
  memset(buf, 0, BUF_LEN);
  strncpy(buf, (const char*)payload, len);

  String cmd = String(buf);
  Serial.print("payload=");
  Serial.println(cmd);

  remaining_time += relay_enable_time;
}