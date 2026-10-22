#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "Ghe Bhikari";
const char* WIFI_PASSWORD = "sanskar19@";
const char* SERVER_URL = "https://smart-buzzer-iot-cloud.onrender.com/api/get_status?code=1552";

const int BUZZER_PIN = D1;
const int LED_PIN = D2;

unsigned long lastPollTime = 0;
const unsigned long pollInterval = 3000;

bool isBuzzerActive = false;
bool alarmOutputState = false;
unsigned long lastAlarmToggle = 0;
const unsigned long alarmInterval = 300;

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("Wi-Fi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Wi-Fi connection timed out");
  }

  lastPollTime = millis() - pollInterval;
}

void checkServerStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.reconnect();
    isBuzzerActive = false;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  HTTPClient http;
  http.setTimeout(12000);

  if (!http.begin(client, SERVER_URL)) {
    Serial.println("Failed to initialize HTTPS connection");
    isBuzzerActive = false;
    return;
  }

  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    Serial.print("Server response: ");
    Serial.println(payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      const char* responseCode = doc["code"] | "";
      const char* status = doc["status"] | "UNKNOWN";
      bool buzzer = doc["buzzer"] | false;

      if (strcmp(responseCode, "1552") == 0) {
        isBuzzerActive = buzzer;

        Serial.print("Status: ");
        Serial.println(status);

        Serial.print("Buzzer: ");
        Serial.println(isBuzzerActive ? "ON" : "OFF");

        if (doc["ist_time"].is<const char*>()) {
          Serial.print("Time: ");
          Serial.println(doc["ist_time"].as<const char*>());
        }
      } else {
        Serial.println("Unexpected response code");
        isBuzzerActive = false;
      }
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      isBuzzerActive = false;
    }
  } else {
    Serial.print("HTTPS request failed: ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.errorToString(httpCode));
    isBuzzerActive = false;
  }

  http.end();
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    isBuzzerActive = false;

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (currentMillis - lastPollTime >= pollInterval) {
      lastPollTime = currentMillis;
      WiFi.reconnect();
    }

    delay(10);
    return;
  }

  if (currentMillis - lastPollTime >= pollInterval) {
    lastPollTime = currentMillis;
    checkServerStatus();
  }

  if (isBuzzerActive) {
    if (currentMillis - lastAlarmToggle >= alarmInterval) {
      lastAlarmToggle = currentMillis;
      alarmOutputState = !alarmOutputState;

      digitalWrite(LED_PIN, alarmOutputState ? HIGH : LOW);
      digitalWrite(BUZZER_PIN, alarmOutputState ? HIGH : LOW);
    }
  } else {
    alarmOutputState = false;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  delay(10);
}