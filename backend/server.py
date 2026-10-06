import os
import json
import asyncio
import pandas as pd
import numpy as np
import joblib
from fastapi import FastAPI, Request, BackgroundTasks, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, field_validator
from sse_starlette.sse import EventSourceResponse
from typing import Optional
from datetime import datetime

app = FastAPI(title="EnergyGuard AI API")

# Enable CORS for React frontend and mobile browser
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(BASE_DIR, 'model.pkl')
INFO_PATH = os.path.join(BASE_DIR, 'model_info.json')

# In-memory variable for software demo state
demo_state = {
    "on": False
}

class DemoStateRequest(BaseModel):
    on: bool


# Load models (Random Forest + Isolation Forest ensemble)
rf_model = None
if_model = None

if os.path.exists(MODEL_PATH):
    loaded = joblib.load(MODEL_PATH)
    if isinstance(loaded, dict) and 'classifier' in loaded:
        rf_model = loaded['classifier']
        if_model = loaded.get('anomaly_detector')
    else:
        # Fallback if raw model was loaded
        rf_model = loaded
        if_model = None
    print(f"Models loaded — Classifier: {rf_model is not None}, Isolation Forest: {if_model is not None}")
else:
    print("Warning: model.pkl not found!")
model = rf_model  # alias for backward compatibility

# A queue to hold live predictions for SSE
prediction_queues = []

# Buffer for rolling features
sensor_buffer = []

class SensorReading(BaseModel):
    voltage: float
    current: float
    power: float
    energy: float
    timestamp: str
    source: str
    
    @field_validator('voltage', 'current', 'power', 'energy')
    @classmethod
    def check_non_negative(cls, v):
        if v < 0:
            raise ValueError('Value cannot be negative')
        if not np.isfinite(v):
            raise ValueError('Value must be finite')
        return v
        
    @field_validator('timestamp')
    @classmethod
    def check_timestamp(cls, v):
        if not v or v.strip() == '':
            # Auto-fill with server time if ESP32 sends empty
            return datetime.utcnow().isoformat() + 'Z'
        try:
            datetime.fromisoformat(v.replace('Z', '+00:00'))
        except ValueError:
            raise ValueError('timestamp must be in valid ISO 8601 format')
        return v


def extract_live_features(buffer):
    """
    Extract features matching exact training pipeline:
    mean_power, std_power, min_power, max_power, power_change, current_change
    using a 5-sample window.
    """
    if len(buffer) < 2:
        return None
        
    df = pd.DataFrame(buffer)
    latest = df.iloc[-1]
    
    # Calculate stats over the buffer (up to 5 samples)
    mean_power = df['power'].mean()
    std_power = df['power'].std() if len(df) > 1 else 0.0
    min_power = df['power'].min()
    max_power = df['power'].max()
    
    # Changes (difference between latest and previous)
    power_change = latest['power'] - df.iloc[-2]['power']
    current_change = latest['current'] - df.iloc[-2]['current']
    
    # Ensure feature columns exactly match training:
    features = pd.DataFrame([{
        'voltage': latest['voltage'],
        'current': latest['current'],
        'power': latest['power'],
        'energy': latest['energy'],
        'mean_power': mean_power,
        'std_power': std_power,
        'min_power': min_power,
        'max_power': max_power,
        'power_change': power_change,
        'current_change': current_change
    }])
    
    return features


@app.post("/api/sensor")
async def receive_sensor_data(reading: SensorReading):
    global sensor_buffer
    data = reading.model_dump()
    sensor_buffer.append(data)
    
    # Keep only the last 5 readings for the rolling window
    if len(sensor_buffer) > 5:
        sensor_buffer = sensor_buffer[-5:]
        
    prediction = "Unknown"
    confidence = 0.0
    status = "ok"
    is_anomaly = False
    isolation_forest_score = 0.0
    anomaly_reason = None
    
    if len(sensor_buffer) < 5:
        status = "warming_up"
        
    if rf_model is not None and len(sensor_buffer) >= 2:
        features = extract_live_features(sensor_buffer)
        if features is not None:
            # 1. Random Forest Classifier
            pred = rf_model.predict(features)[0]
            probs = rf_model.predict_proba(features)[0]
            conf = max(probs)
            prediction = str(pred)
            confidence = float(conf)
            
            # 2. Isolation Forest Anomaly Detection
            if if_model is not None:
                raw_score = float(if_model.decision_function(features)[0])
                if_flag = int(if_model.predict(features)[0])  # -1 = anomaly, 1 = normal
                # Sigmoid scaling around decision boundary (0.0): higher score = higher anomaly likelihood
                scaled_score = 1.0 / (1.0 + np.exp(raw_score * 10.0))
                isolation_forest_score = round(float(scaled_score), 2)
                is_anomaly = (if_flag == -1) or (isolation_forest_score >= 0.65)
                
                if is_anomaly:
                    if reading.power > 15.0 and prediction == "LED_Bulb":
                        anomaly_reason = f"LED_Bulb power spike ({reading.power:.1f}W > rated 9W)."
                    elif isolation_forest_score >= 0.8:
                        anomaly_reason = f"Severe baseline divergence detected by Isolation Forest (Score: {isolation_forest_score})."
                    else:
                        anomaly_reason = f"Unusual electrical load signature detected (Score: {isolation_forest_score})."
            
    # Format the payload for the React frontend (matching EnergyRecord)
    payload = {
        "timestamp": reading.timestamp,
        "dataSource": reading.source,
        "aggregatePowerWatts": reading.power,
        "voltage": reading.voltage,
        "current": reading.current,
        "energy": reading.energy,
        "isolationForestScore": isolation_forest_score,
        "isAnomaly": is_anomaly,
        "anomalyReason": anomaly_reason,
        "appliances": []
    }
    
    if prediction not in ["Unknown", "OFF"] and reading.power > 0.5:
        app_id = f"{reading.source.lower()}_{prediction.lower().replace(' ', '_')}"
        app_item = {
            "id": app_id,
            "name": prediction,
            "powerWatts": reading.power,
            "confidence": confidence,
            "isolationForestScore": isolation_forest_score,
            "isAnomaly": is_anomaly,
            "healthStatus": "critical" if (is_anomaly and isolation_forest_score >= 0.8) else ("warning" if is_anomaly else "normal")
        }
        if anomaly_reason:
            app_item["anomalyType"] = "Power Spike" if reading.power > 15 else "Energy Drift"
            app_item["anomalyReason"] = anomaly_reason
        payload["appliances"].append(app_item)
    
    # Push to all SSE clients
    for q in prediction_queues:
        await q.put(payload)
        
    return {
        "status": status,
        "prediction": prediction,
        "confidence": confidence,
        "is_anomaly": is_anomaly,
        "isolation_forest_score": isolation_forest_score,
        "anomaly_reason": anomaly_reason,
        "voltage": reading.voltage,
        "current": reading.current,
        "power": reading.power,
        "energy": reading.energy,
        "timestamp": reading.timestamp,
        "source": reading.source
    }

@app.get("/api/stream")
async def stream(request: Request):
    q = asyncio.Queue()
    prediction_queues.append(q)
    
    async def event_generator():
        try:
            while True:
                # If client closes connection, exit
                if await request.is_disconnected():
                    break
                data = await q.get()
                yield {
                    "event": "message",
                    "data": json.dumps(data)
                }
        except asyncio.CancelledError:
            pass
        finally:
            if q in prediction_queues:
                prediction_queues.remove(q)
            
    return EventSourceResponse(event_generator())

@app.get("/api/health")
def get_health():
    model_info = {}
    if os.path.exists(INFO_PATH):
        try:
            with open(INFO_PATH, 'r') as f:
                model_info = json.load(f)
        except Exception:
            pass
            
    return {
        "status": "ok",
        "models_loaded": {
            "random_forest": rf_model is not None,
            "isolation_forest": if_model is not None
        },
        "model_loaded": rf_model is not None,
        "model_type": model_info.get("model", "Random Forest + Isolation Forest"),
        "model_version": model_info.get("last_trained", "unknown"),
        "connected_clients": len(prediction_queues),
        "buffer_size": len(sensor_buffer)
    }

@app.get("/api/model_status")
def get_model_status():
    if os.path.exists(INFO_PATH):
        with open(INFO_PATH, 'r') as f:
            return json.load(f)
    return {"error": "Model info not found"}

@app.post("/api/demo/state")
async def set_demo_state(req: DemoStateRequest):
    demo_state["on"] = req.on
    return {
        "status": "ok",
        "demo": True,
        "on": req.on,
        "state": "ON" if req.on else "OFF"
    }

@app.get("/api/demo/state")
def get_demo_state():
    is_on = demo_state.get("on", False)
    return {
        "status": "ok",
        "demo": True,
        "on": is_on,
        "state": "ON" if is_on else "OFF"
    }

if __name__ == '__main__':
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=True)

