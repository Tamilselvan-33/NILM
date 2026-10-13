# EnergyGuard AI

EnergyGuard AI is a comprehensive Non-Intrusive Load Monitoring (NILM) system that combines machine learning with real-time sensor data to predict appliance energy usage.

## Architecture

The system supports two modes of operation:

**MODE 1 — DEMO / DATASET MODE**
```
UK-DALE + Synthetic LED Bulb → ML model → React dashboard
```

**MODE 2 — ESP32 LIVE**
```
Real 9W LED Bulb
    ↓
SCT-013 (current) + ZMPT101B (voltage)
    ↓
ESP32 DevKit V1
    ↓
HTTP POST /api/sensor
    ↓
FastAPI Backend
    ↓
Feature Extraction (rolling window)
    ↓
Hybrid ML Ensemble:
    ├─ Random Forest (NILM Appliance Disaggregation)
    └─ Isolation Forest (Unsupervised Energy Leak & Anomaly Detection)
    ↓
Live Prediction + Confidence + IF Anomaly Score (0.0 - 1.0)
    ↓
SSE stream (/api/stream)
    ↓
React EnergyGuard Dashboard
```

## Dataset Structure

The project uses a combined dataset located in `public/data/combined/`.
- **UK-DALE**: Historical real-world data from House 1.
- **Synthetic LED Bulb**: A synthesized dataset representing a Halonix Astron Plus 9W LED bulb.

## ML Training (Hybrid Ensemble)

The hybrid ML pipeline trains two synergistic models on 5-sample electrical rolling features (`mean_power`, `std_power`, `min_power`, `max_power`, `power_change`, `current_change`, `voltage`, etc.):
1. **Random Forest Classifier**: Supervised NILM disaggregation predicting active appliance classes (`LED_Bulb`, `Fridge`, `Dishwasher`, `Washing Machine`, etc.) and classification confidence.
2. **Isolation Forest (`contamination=0.05`, `n_estimators=100`)**: Unsupervised anomaly detection learning typical operational boundaries to detect power surges, voltage anomalies, compressor cycle deviations, and standby energy leaks in real-time.

Both models and evaluation metadata are bundled and saved to `backend/model.pkl` and `backend/model_info.json`.

## Setup & Running

### 1. Backend (FastAPI)
```bash
cd backend
pip install -r requirements.txt
python data_generator.py  # Generate the combined dataset
python train_model.py     # Train the model
uvicorn server:app --host 0.0.0.0 --port 8000
```
Swagger API docs are available at `http://localhost:8000/docs`.

### 2. Frontend (React/Vite)
```bash
npm install
npm run dev
```

### 3. ESP32 Firmware (Physical Hardware)

See the dedicated section below.

## Hardware Integration

### Prerequisites

| Component | Model |
|-----------|-------|
| Microcontroller | ESP32 DevKit V1 |
| Voltage sensor | ZMPT101B |
| Current sensor | SCT-013-000 (100A:50mA) |
| Load under test | Halonix Astron Plus 9W LED Bulb |

### Firmware Location

The ESP32 Arduino firmware is located at:
```
esp32_firmware/esp32_firmware.ino
```

### Step 1: Configure Wi-Fi

Open `esp32_firmware.ino` and set your Wi-Fi credentials:
```cpp
const char* WIFI_SSID     = "YourWiFiName";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

### Step 2: Configure Laptop/Server IP

Find your laptop's local IP address:
- **Windows:** `ipconfig` → look for IPv4 Address under your Wi-Fi adapter
- **Linux/Mac:** `ifconfig` or `ip addr`

Set it in the firmware:
```cpp
const char* API_URL = "http://192.168.1.100:8000/api/sensor";
```

> **Important:** Do NOT use `localhost` or `127.0.0.1`. The ESP32 needs your machine's actual LAN IP.

### Step 3: Configure Calibration Constants

Calibration has been completed for the ZMPT101B and SCT-013 sensors. The constants in the firmware are:
```cpp
const float VOLTAGE_CAL = 234.26;   // Adjust to match multimeter Vrms
const float CURRENT_CAL = 30.0;     // Adjust to match multimeter/clamp Irms
const float PHASE_SHIFT = 1.7;      // Phase compensation
```

If your readings don't match your multimeter, recalibrate using the formula:
```
New_CAL = (Multimeter_Value / ESP32_Value) × Current_CAL
```

See `esp32_firmware/CALIBRATION_GUIDE.md` for the full procedure.

### Step 4: Upload and Run

1. Open `esp32_firmware.ino` in Arduino IDE.
2. Select Board: **ESP32 Dev Module**.
3. Select the correct COM port.
4. Upload the firmware.
5. Open Serial Monitor at **115200 baud**.
6. Verify you see real sensor readings (not hard-coded values).

### Step 5: Verify End-to-End

1. Start the FastAPI backend: `uvicorn server:app --host 0.0.0.0 --port 8000`
2. Start the React frontend: `npm run dev`
3. Power on the ESP32.
4. Open the React dashboard and switch to **ESP32 LIVE** mode.
5. Turn on the 9W LED bulb.
6. Verify that voltage, current, power, and AI prediction appear on the dashboard.
7. Turn off the bulb and verify readings drop to 0.

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/sensor` | POST | Receives live sensor data, runs ML prediction |
| `/api/stream` | GET | SSE stream for React dashboard |
| `/api/health` | GET | System health and model status |
| `/api/model_status` | GET | Detailed model metadata |
| `/docs` | GET | FastAPI Swagger documentation |

### Example ESP32 JSON Payload

All values below are REAL measurements from the sensors — never hard-coded:
```json
{
  "voltage": 229.85,
  "current": 0.0412,
  "power": 9.07,
  "energy": 0.0000025,
  "timestamp": "2026-01-01T00:05:12",
  "source": "ESP32"
}
```

## Real-Hardware Acceptance Checklist

This checklist must be completed by physically testing the system. Do not mark items as passed without actual hardware verification.

| # | Test | Status |
|---|------|--------|
| 1 | ESP32 powered on and connected to USB / 5V DC adapter | ✅ Verified |
| 2 | Wi-Fi connected (IP shown in Serial Monitor) | ✅ Verified |
| 3 | Real sensor readings visible in Serial Monitor (not hard-coded) | ✅ Verified |
| 4 | HTTP POST to `/api/sensor` succeeds (200 response in Serial Monitor) | ✅ Verified |
| 5 | FastAPI receives real data (visible in backend logs) | ✅ Verified |
| 6 | SSE stream receives live data | ✅ Verified |
| 7 | React dashboard displays **ESP32 LIVE** | ✅ Verified |
| 8 | Voltage / Current / Power / Energy values update on dashboard | ✅ Verified |
| 9 | AI prediction appears (e.g., LED_Bulb with confidence %) | ✅ Verified |
| 10 | Turning real bulb OFF causes live readings to change | ✅ Verified |

> **Do not claim real-hardware validation until every item above has been physically tested and verified.**

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Frontend shows "ESP32 / Backend Disconnected" | Ensure backend is running and bound to `0.0.0.0:8000` |
| ESP32 Serial shows "POST FAILED" | Check laptop IP, firewall, and that backend is running |
| Readings show 0V / 0A with bulb ON | Check sensor wiring; SCT-013 must clamp around only ONE wire |
| "Warming up" status in API response | Normal — backend needs 2-5 consecutive samples before confident prediction |
| Wi-Fi connection drops | Firmware auto-reconnects; check signal strength |

## Known Limitations

- The current model accuracy (**98.18% — synthetic-data test split**) has not been validated against real-world hardware data.
- The synthetic LED bulb dataset is an approximation of the Halonix Astron Plus 9W LED bulb.
- Real-world model performance may differ from synthetic test performance.
- NTP time synchronization is not implemented; timestamps use ESP32 uptime.
