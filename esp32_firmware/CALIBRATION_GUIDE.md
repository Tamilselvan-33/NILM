# EnergyGuard Hardware Calibration Guide

> [!CAUTION]
> **HIGH VOLTAGE WARNING**
> You will be working with mains AC voltage (230V). Always ensure connections are secure, use insulated tools, and NEVER touch exposed wires while the circuit is live. 

## Hardware Components

| Component | Purpose |
|-----------|---------|
| ESP32 DevKit V1 | Microcontroller with Wi-Fi |
| ZMPT101B | AC voltage transformer (connected to mains) |
| SCT-013-000 | Split-core current transformer (100A:50mA) |
| Burden resistor (33Ω) | Required if using SCT-013-000 (not the SCT-013-030 which has built-in) |

## Wiring

```
ZMPT101B:
  VCC → ESP32 3.3V
  GND → ESP32 GND
  OUT → ESP32 GPIO34 (ADC1)

SCT-013-000:
  One lead → ESP32 GPIO35 (ADC1)
  Other lead → ESP32 GND
  (with burden resistor + bias circuit — see OpenEnergyMonitor docs)
```

> **Important:** Use ADC1 pins only (GPIO 32–39). ADC2 pins do not work when Wi-Fi is active.

## 1. Voltage Calibration (ZMPT101B)

### Procedure:
1. Upload the firmware with Serial debug output enabled.
2. Measure your actual wall AC voltage with a reliable multimeter (e.g., 229.5V).
3. Read the `Vrms` value from the ESP32 Serial Monitor.
4. Adjust the `VOLTAGE_CAL` constant:
   ```
   New VOLTAGE_CAL = (Multimeter Vrms / ESP32 Vrms) × Current VOLTAGE_CAL
   ```
5. Optionally fine-tune using the ZMPT101B onboard trimpot.

## 2. Current Calibration (SCT-013-000)

### Procedure:
1. Clamp the SCT-013 around **only ONE** wire (Live or Neutral). If clamped around the whole cable, the fields cancel and you read 0A.
2. Turn on a known load (e.g., 9W LED bulb → ~0.040A at 230V).
3. Read the `Irms` value from the ESP32 Serial Monitor.
4. Adjust the `CURRENT_CAL` constant:
   ```
   New CURRENT_CAL = (Actual Current / ESP32 Current) × Current CURRENT_CAL
   ```

For SCT-013-000 with 33Ω burden: theoretical starting value ≈ 60.6

## 3. Low-Power Appliance Detection (10-Turn Winding Technique)

For low-power loads like a 9W LED bulb (~0.039A), a 100A current transformer (SCT-013-000) produces a very small voltage across the 33Ω burden resistor that can be submerged under ADC quantization noise.

### Solution:
Loop the Live wire through the SCT-013 clamp **10 times**:
- 1 single wire = 0.039 A effective magnetic flux.
- 10 turns = **0.390 A effective magnetic flux (10× signal amplification)**.

### Calibration Results:
With high-speed True RMS sampling (800 samples):
- **Bulb OFF (Noise Floor)**: AC RMS ≈ **13 – 18**
- **Bulb ON (Active Load)**: AC RMS ≈ **56 – 60**
- **Threshold**: `RMS_ON_THRESHOLD = 35.0` cleanly separates OFF (0 W) and ON (9 W) with zero overlap.

## 4. Final Verification

Once calibrated and wired:
- When the 9W bulb is ON, the ESP32 measures RMS > 35 and transmits ~9.0 W and ~0.039 A.
- When the bulb is OFF, the ESP32 measures RMS < 35 and transmits 0.0 W and 0.000 A.
- The React dashboard receives live updates via SSE from the FastAPI backend and disaggregates the appliance via the Random Forest + Isolation Forest ensemble.

