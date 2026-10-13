# 🔔 Smart Dose & Buzzer Monitor

An automated medicine reminder and schedule tracker built with **Node.js**, **Express**, **SQLite**, and a mobile-first **Tailwind CSS** dashboard.

The system tracks doses based on **Indian Standard Time (IST)**, activates buzzer alerts during scheduled dose windows, automatically transitions unacknowledged doses to missed status at cutoff, and persists logs in a local SQLite database.

---

## 📋 Features

- **IST Time-Zone Synchronization:** Operates strictly on `Asia/Kolkata` time regardless of server or client location.
- **Automated Daily Schedule & Reset:**
  - `05:00 AM – 10:59 AM`: Morning waiting period (`status: PENDING`, `buzzer: false`).
  - `11:00 AM – 04:59 PM`: Morning dose window (`buzzer: true` until marked `TAKEN`).
  - `05:00 PM – 07:59 PM`: Evening reset & waiting period (`status: PENDING`, `buzzer: false`).
  - `08:00 PM – 09:59 PM`: Night dose window (`buzzer: true` until marked `TAKEN`).
  - `10:00 PM – 04:59 AM`: Night cutoff (`status: MISSED`, `buzzer: false`).
- **Persistent Storage:** Local, zero-configuration SQLite database (`better-sqlite3`).
- **Device & IoT Ready:** Clean JSON API endpoints easily consumed by microcontrollers like ESP8266, ESP32, or Raspberry Pi.
- **Mobile-First Dashboard:**
  - 4-digit PIN authentication lock for isolated profile access.
  - Automatic focus-shifting PIN input boxes.
  - Audio synth beep and native mobile haptics (`navigator.vibrate`) during active buzzers.
  - Sticky bottom touch actions adhering to mobile ergonomics.

---

## 📂 Project Structure

```text
smart-buzzer-monitor/
├── doses.db               # SQLite database file (auto-generated)
├── index.js               # Express server, SQLite queries & schedule logic
├── package.json           # Dependencies and scripts
└── public/
    └── index.html         # Mobile-first Tailwind CSS UI

```

---

## 🚀 Getting Started

### Prerequisites

* [Node.js](https://nodejs.org/) (version 18.0.0 or higher recommended)
* `npm` (packaged with Node.js)

### 1. Installation

Clone this repository and navigate to the directory:

```bash
git clone [https://github.com/](https://github.com/)<your-username>/smart-buzzer-monitor.git
cd smart-buzzer-monitor

```

Install the dependencies:

```bash
npm install

```

Ensure your `package.json` contains `"type": "module"`. If not, add it:

```json
{
  "type": "module"
}

```

### 2. Run the Application

Start the development server with live watch mode:

```bash
npm run dev

```

Or run standard production mode:

```bash
npm start

```

The application will be live at:

```text
http://localhost:3000

```

---

## 🌐 Making It Publicly Accessible

To connect hardware devices or allow remote access from outside your local Wi-Fi:

### Fast Local Tunneling (Development / Testing)

Run either of the following without additional router configuration:

```bash
# Option A: localtunnel
npx localtunnel --port 3000

# Option B: Cloudflare Tunnel
npx cloudflared tunnel --url http://localhost:3000

```

Use the generated HTTPS URL as your API endpoint on external clients or microcontrollers.

---

## 📡 API Reference

### 1. Get Status

Retrieve current state, window, and buzzer status for a given device code.

* **URL:** `/api/get_status`
* **Method:** `GET`
* **Query Parameters:**
* `code` (required): 4-digit identifier (e.g., `1552`).



**Example Request:**

```bash
curl "http://localhost:3000/api/get_status?code=1552"

```

**Success Response (`200 OK`):**

```json
{
  "code": "1552",
  "buzzer": true,
  "status": "NOT_TAKEN",
  "active_window": "night",
  "ist_time": "20:14:05"
}

```

---

### 2. Update Status

Mark a dose window as taken to silence the buzzer.

* **URL:** `/api/post_status`
* **Method:** `POST`
* **Parameters (Query string or JSON body):**
* `code` (required): 4-digit identifier.
* `status` (required): Must be `TAKEN`.
* `time` (required): Active window (`morning` or `night`).



**Example Request (Query String):**

```bash
curl -X POST "http://localhost:3000/api/post_status?code=1552&status=TAKEN&time=morning"

```

**Example Request (JSON Body):**

```bash
curl -X POST "http://localhost:3000/api/post_status" \
  -H "Content-Type: application/json" \
  -d '{"code": "1552", "status": "TAKEN", "time": "night"}'

```

**Success Response (`200 OK`):**

```json
{
  "success": true,
  "code": "1552",
  "status": "TAKEN",
  "time": "night",
  "date": "2026-10-13"
}

```

---

### 3. Fetch History Logs

Retrieve historical logs stored in SQLite for a specific profile.

* **URL:** `/api/logs`
* **Method:** `GET`
* **Query Parameters:**
* `code` (required): 4-digit identifier.



**Example Request:**

```bash
curl "http://localhost:3000/api/logs?code=1552"

```

**Success Response (`200 OK`):**

```json
[
  {
    "code": "1552",
    "window": "morning",
    "date_taken": "2026-10-13",
    "status": "TAKEN",
    "created_at": "2026-10-13 11:05:22"
  }
]

```

---

## 🗄️ Database Schema

The system automatically initializes an SQLite database file named `doses.db` on launch:

```sql
CREATE TABLE IF NOT EXISTS dose_logs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT NOT NULL,
  window TEXT NOT NULL,
  date_taken TEXT NOT NULL,
  status TEXT NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  UNIQUE(code, window, date_taken)
);

```

---

## 📱 Hardware Client Example (ESP8266 / ESP32)

Microcontrollers can poll the server over Wi-Fi and trigger a GPIO buzzer when `buzzer: true`:

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASS";
const char* serverUrl = "http://YOUR_SERVER_IP:3000/api/get_status?code=1552";

const int BUZZER_PIN = D1;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverUrl);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      deserializeJson(doc, payload);

      bool buzzer = doc["buzzer"];
      digitalWrite(BUZZER_PIN, buzzer ? HIGH : LOW);
    }
    http.end();
  }
  delay(3000); // Poll every 3 seconds
}

```

---

## 📄 License

This project is licensed under the MIT License - feel free to modify and adapt it for personal or commercial use.


