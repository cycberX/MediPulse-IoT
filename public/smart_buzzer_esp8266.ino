#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ==========================================
// CONFIGURATION
// ==========================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Replace with your Laptop/Server IP or public tunnel URL
// If running locally, make sure your NodeMCU and laptop are on the same Wi-Fi!
// e.g., "http://192.168.1.15:3000/api/get_status?code=1552"
const char* SERVER_URL    = "http://192.168.1.X:3000/api/get_status?code=1552";

// Pin definitions
const int BUZZER_PIN = D1; // GPIO 5
const int LED_PIN    = D2; // GPIO 4

// Timing variables
unsigned long lastPollTime = 0;
const unsigned long pollInterval = 30000; // Poll API every 3 seconds

bool isBuzzerActive = false;

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize GPIO pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Start with both OFF
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // Quick startup test: 1 short beep + blink
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to Wi-Fi
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected successfully!");
  Serial.print("NodeMCU IP Address: ");
  Serial.println(WiFi.localIP());
}

void checkServerStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Waiting to reconnect...");
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;

  // Begin HTTP connection
  if (http.begin(client, SERVER_URL)) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.print("Response: ");
      Serial.println(payload);

      // Parse JSON response: {"code":"1552","buzzer":true,"status":"NOT_TAKEN",...}
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        isBuzzerActive = doc["buzzer"].as<bool>();
        const char* status = doc["status"];
        
        Serial.print("Dose Status: ");
        Serial.print(status);
        Serial.print(" | Buzzer Active: ");
        Serial.println(isBuzzerActive ? "YES" : "NO");
      } else {
        Serial.print("JSON parse error: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.printf("HTTP GET failed, code: %d, error: %s\n", httpCode, http.errorToString(httpCode).c_str());
      isBuzzerActive = false; // Turn off on communication error
    }
    http.end();
  } else {
    Serial.println("Unable to connect to server URL");
    isBuzzerActive = false;
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Non-blocking timer to poll server every 3 seconds
  if (currentMillis - lastPollTime >= pollInterval) {
    lastPollTime = currentMillis;
    checkServerStatus();
  }

  // Handle buzzer and light alerting
  if (isBuzzerActive) {
    // Intermittent alarm pattern: Beep/Flash 300ms ON, 300ms OFF
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  } else {
    // Both stay completely silent and dark
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(50);
  }
}