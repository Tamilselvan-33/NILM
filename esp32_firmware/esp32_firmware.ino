/*
 * =============================================================
 *  EnergyGuard AI — ESP32 Real-Hardware & Demo Firmware
 *  For: ESP32 DevKit V1 + SCT-013-000 + ZMPT101B
 * =============================================================
 *
 *  Features:
 *    1. REAL SENSOR DATA:
 *       Reads calibrated physical AC measurements via:
 *       - ZMPT101B (voltage) on GPIO34
 *       - SCT-013-000 (current) on GPIO35
 *
 *    2. DEMO / MOCK SOFTWARE MODE:
 *       Polls backend /api/demo/state periodically (~1s).
 *       When demoBulbOn is set via Mobile UI:
 *       - ON: generates realistic 9W LED bulb values (~228-230V, ~0.038A, ~7-10W)
 *       - OFF: sets current to 0.0000A and power to 0.00W.
 *
 *  Dependencies (install via Arduino Library Manager):
 *    - EmonLib (by OpenEnergyMonitor)
 *    - WiFi (built into ESP32 Arduino core)
 *    - HTTPClient (built into ESP32 Arduino core)
 *
 *  Board: ESP32 Dev Module (or ESP32 DevKit V1)
 *  Upload Speed: 115200
 * =============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "EmonLib.h"

// =============================================================
//  USER CONFIGURATION — EDIT THESE FOR YOUR SETUP
// =============================================================

// ------- Wi-Fi Credentials -------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ------- FastAPI Server Endpoints -------
// Set this to your laptop/PC's LOCAL IP address on the Wi-Fi network.
// Example: "http://10.47.59.1:8000" or "http://192.168.1.100:8000"
const char* API_URL   = "http://10.47.59.1:8000/api/sensor";
const char* STATE_URL = "http://10.47.59.1:8000/api/demo/state";

// ------- Mode Toggle: Demo Software Mode vs Live Sensors -------
// true  = Mobile-controlled Software Demo Mode (active for presentation)
// false = Direct physical ZMPT101B + SCT-013 hardware readings
const bool USE_DEMO_SOFTWARE_MODE = true;

// ------- Sensor Pins (ADC1 only — ADC2 conflicts with Wi-Fi) -------
const int VOLTAGE_PIN = 34;   // ZMPT101B signal output → GPIO34
const int CURRENT_PIN = 35;   // SCT-013-000 signal output → GPIO35

// ------- Calibration Constants (For Real Sensor Hardware) -------
const float VOLTAGE_CAL = 234.26;
const float CURRENT_CAL = 30.0;
const float PHASE_SHIFT = 1.7;

// ------- Noise Floor Thresholds (For Real Sensor Hardware) -------
const float CURRENT_NOISE_FLOOR = 0.01;  // Amps
const float POWER_NOISE_FLOOR   = 1.0;   // Watts

// ------- Timing Intervals -------
const unsigned long POLL_INTERVAL_MS = 1000;  // Poll demo state every 1 second
const unsigned long POST_INTERVAL_MS = 2000;  // Broadcast sensor data every 2 seconds
const unsigned long WIFI_RETRY_MS    = 5000;  // Retry Wi-Fi every 5 seconds

// =============================================================
//  GLOBALS
// =============================================================
EnergyMonitor emon;
bool demoBulbOn = false;           // Current demo bulb state (synced with mobile UI)
unsigned long lastPollTime = 0;
unsigned long lastPostTime = 0;
double cumulativeEnergyKwh = 0.0;
unsigned long lastEnergyCalcTime = 0;
unsigned long bootTime = 0;

// =============================================================
//  FORWARD DECLARATIONS
// =============================================================
void connectWiFi();
void pollDemoState();
void postSensorData(float voltage, float current, float power, double energy);

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=========================================");
  Serial.println("  EnergyGuard AI — ESP32 Dual Firmware   ");
  Serial.println("  Real Sensor + Mobile Demo State Control");
  Serial.println("=========================================");
  Serial.println();

  // ---- Connect to Wi-Fi ----
  connectWiFi();

  // ---- Initialize EmonLib physical sensors (Intact) ----
  emon.voltage(VOLTAGE_PIN, VOLTAGE_CAL, PHASE_SHIFT);
  emon.current(CURRENT_PIN, CURRENT_CAL);

  Serial.println("[SENSORS] ZMPT101B on GPIO" + String(VOLTAGE_PIN));
  Serial.println("[SENSORS] SCT-013  on GPIO" + String(CURRENT_PIN));
  Serial.println("[CONFIG] Mode: " + String(USE_DEMO_SOFTWARE_MODE ? "DEMO SOFTWARE CONTROL" : "PHYSICAL SENSORS"));
  Serial.println("[CONFIG] Demo State URL: " + String(STATE_URL));
  Serial.println("[CONFIG] Sensor Ingest URL: " + String(API_URL));
  Serial.println();

  lastEnergyCalcTime = millis();
  bootTime = millis();
}

// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {
  unsigned long now = millis();

  // ---- Ensure Wi-Fi is connected ----
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWiFiCheck = 0;
    if (now - lastWiFiCheck >= WIFI_RETRY_MS) {
      lastWiFiCheck = now;
      Serial.println("[WIFI] Connection lost. Reconnecting...");
      connectWiFi();
    }
    return;
  }

  // ---- 1. Poll Demo State from Backend (every ~1s, non-blocking) ----
  if (now - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = now;
    pollDemoState();
  }

  // ---- 2. Read REAL Sensor Data (Kept completely intact) ----
  // calcVI(crossings, timeout_ms)
  emon.calcVI(20, 2000);
  float real_voltage = emon.Vrms;
  float real_current = emon.Irms;
  float real_power   = emon.realPower;

  if (real_current < CURRENT_NOISE_FLOOR) real_current = 0.0;
  if (real_power < POWER_NOISE_FLOOR) real_power = 0.0;

  // ---- 3. Select Active Output Values ----
  float voltage = 0.0;
  float current = 0.0;
  float power   = 0.0;

  if (USE_DEMO_SOFTWARE_MODE) {
    // ==========================================
    // DEMO / MOCK ELECTRICAL VALUES
    // ==========================================
    // Voltage: 228.5 + small random variation (-2.0 to +2.0 V)
    voltage = 228.5 + (random(-20, 21) / 10.0);

    if (demoBulbOn) {
      // Current: 0.038 + small random variation (-0.005 to +0.005 A)
      current = 0.038 + (random(-5, 6) / 1000.0);
      // Realistic 9W Bulb Power
      power   = voltage * current * 0.95;
    } else {
      current = 0.0;
      power   = 0.0;
    }

    // Formatted Serial Monitor Output
    Serial.println("--------------------------------");
    Serial.print("BULB STATE : ");
    Serial.println(demoBulbOn ? "ON" : "OFF");
    Serial.print("Voltage    : approximately ");
    Serial.print(voltage, 2);
    Serial.println(" V");
    Serial.print("Current    : ");
    Serial.print(current, 4);
    Serial.println(" A");
    Serial.print("Power      : approximately ");
    Serial.print(power, 2);
    Serial.println(" W");
    Serial.println("--------------------------------");
  } else {
    // ==========================================
    // REAL SENSOR DATA (Physical Hardware Mode)
    // ==========================================
    voltage = real_voltage;
    current = real_current;
    power   = real_power;

    Serial.println("------ LIVE SENSOR READING ------");
    Serial.print("  Voltage:  "); Serial.print(voltage, 2); Serial.println(" V");
    Serial.print("  Current:  "); Serial.print(current, 4); Serial.println(" A");
    Serial.print("  Power:    "); Serial.print(power, 2);   Serial.println(" W");
    Serial.print("  Energy:   "); Serial.print(cumulativeEnergyKwh, 7); Serial.println(" kWh");
    Serial.println("---------------------------------");
  }

  // ---- 4. Accumulate Energy (kWh) ----
  if (lastEnergyCalcTime > 0) {
    double hoursPassed = (now - lastEnergyCalcTime) / 3600000.0;
    if (power > 0) {
      cumulativeEnergyKwh += (power * hoursPassed) / 1000.0;
    }
  }
  lastEnergyCalcTime = now;

  // ---- 5. POST to FastAPI /api/sensor at configured interval ----
  if (now - lastPostTime >= POST_INTERVAL_MS) {
    lastPostTime = now;
    postSensorData(voltage, current, power, cumulativeEnergyKwh);
  }
}

// =============================================================
//  Wi-Fi Connection (with retry)
// =============================================================
void connectWiFi() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WIFI] Connecting to: ");
  Serial.println(WIFI_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[WIFI] Connected successfully!");
    Serial.print("[WIFI] ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("[WIFI] Failed to connect. Will retry automatically.");
  }
}

// =============================================================
//  Poll Backend Demo State: GET /api/demo/state
// =============================================================
void pollDemoState() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.begin(STATE_URL);
  http.setTimeout(2000);  // 2s timeout for non-blocking loop

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == 200) {
    String payload = http.getString();
    
    // Check if state is ON
    if (payload.indexOf("\"on\":true") >= 0 || payload.indexOf("\"on\": true") >= 0) {
      if (!demoBulbOn) {
        demoBulbOn = true;
        Serial.println("\n>>> [DEMO STATE CHANGED] Bulb turned ON via Mobile Control <<<\n");
      }
    } 
    // Check if state is OFF
    else if (payload.indexOf("\"on\":false") >= 0 || payload.indexOf("\"on\": false") >= 0) {
      if (demoBulbOn) {
        demoBulbOn = false;
        Serial.println("\n>>> [DEMO STATE CHANGED] Bulb turned OFF via Mobile Control <<<\n");
      }
    }
  } else {
    // Backend unreachable or error
    // Serial.println("[DEMO POLL] Warning: Unable to poll " + String(STATE_URL));
  }

  http.end();
}

// =============================================================
//  HTTP POST to FastAPI /api/sensor
// =============================================================
void postSensorData(float voltage, float current, float power, double energy) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);

  // Build ISO 8601 timestamp from uptime
  unsigned long uptimeSec = (millis() - bootTime) / 1000;
  unsigned long hours   = uptimeSec / 3600;
  unsigned long minutes = (uptimeSec % 3600) / 60;
  unsigned long seconds = uptimeSec % 60;

  char timestamp[30];
  snprintf(timestamp, sizeof(timestamp),
           "2026-01-01T%02lu:%02lu:%02lu",
           hours, minutes, seconds);

  // Construct JSON payload
  String json = "{";
  json += "\"voltage\":" + String(voltage, 2) + ",";
  json += "\"current\":" + String(current, 4) + ",";
  json += "\"power\":" + String(power, 2) + ",";
  json += "\"energy\":" + String(energy, 7) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"source\":\"ESP32\"";
  json += "}";

  int httpCode = http.POST(json);

  if (httpCode > 0) {
    // 200 OK
  } else {
    Serial.print("[HTTP] Sensor POST failed. Error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}
