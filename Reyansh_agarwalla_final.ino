//IP ADDRESS FOR THIS NETWORK IS 192.168.0.113

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>

/* ========== WIFI CREDENTIALS ========== */
const char* ssid = "OMOTEC";        
const char* password = "Omotech@23"; 

/* ========== PINS ========== */
#define SDA_PIN 12
#define SCL_PIN 13
#define MOTOR_PIN 6
#define RGB_PIN 22
#define NUM_LEDS 1

int flexPins[5] = {11,10,9,8,7};
int fsrPins[5]  = {1,2,3,4,5};

/* ========== OBJECTS ========== */
Adafruit_MPU6050 mpu;
MAX30105 particleSensor;
Adafruit_NeoPixel rgb(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

/* ========== CONSTANTS ========== */
#define STABILITY_TIME 15000        // 15 sec
#define GYRO_THRESHOLD 1.0

#define BREATH_INHALE 4000
#define BREATH_HOLD   7000
#define BREATH_EXHALE 8000

#define BUFFER_SIZE 100

/* ========== VARIABLES ========== */
unsigned long lastMovementTime = 0;
unsigned long breathStart = 0;

String breathPhase = "INHALE";

// MAX30102
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int32_t heartRate;
int8_t validSpO2;
int8_t validHR;

// Flex / FSR
int flex[5], fsr[5];
const char* fingers[5] = {"Thumb","Index","Middle","Ring","Little"};

float gyroMagnitude = 0;
bool stable = false;

/* ========== HTML DASHBOARD ========== */
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title> Pistol Shooting Stability Dashboard</title>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: #231942;
  color: #ffffff;
  overflow-x: hidden;
}

.header {
  background: #5e548e;
  padding: 20px 40px;
  display: flex;
  align-items: center;
  box-shadow: 0 4px 15px rgba(0,0,0,0.3);
}

.header-icon {
  width: 50px;
  height: 50px;
  background: #e0b1cb;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 28px;
  color: #231942;
  margin-right: 20px;
}

.header h1 {
  font-size: 25px;
  font-weight: 600;
  flex-grow: 1;
}

.live-indicator {
  display: flex;
  align-items: center;
  gap: 10px;
}

.live-dot {
  width: 15px;
  height: 15px;
  background: #00FF88;
  border-radius: 50%;
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

.container {
  padding: 30px;
  max-width: 1600px;
  margin: 0 auto;
}

.grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
  margin-bottom: 20px;
}

.grid-full {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 20px;
}

.card {
  background: #5e548e;
  border-radius: 15px;
  border: 2px solid #9f86c0;
  box-shadow: 5px 5px 20px rgba(0,0,0,0.4);
  overflow: hidden;
}

.card-header {
  background: #231942;
  padding: 15px 20px;
  border-bottom: 2px solid #9f86c0;
}

.card-header h2 {
  color: #e0b1cb;
  font-size: 18px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 1px;
}

.card-body {
  padding: 25px;
}

.metric-value {
  font-size: 48px;
  font-weight: bold;
  color: #ffffff;
  line-height: 1;
}

.metric-label {
  font-size: 14px;
  color: #be95c4;
  margin-top: 5px;
  text-transform: uppercase;
  letter-spacing: 1px;
}

.status-badge {
  display: inline-block;
  padding: 8px 16px;
  border-radius: 20px;
  font-size: 14px;
  font-weight: 600;
  margin-top: 10px;
}

.status-stable {
  background: #00FF88;
  color: #231942;
}

.status-moving {
  background: #FFD700;
  color: #231942;
}

.progress-bar {
  background: #231942;
  height: 30px;
  border-radius: 15px;
  overflow: hidden;
  margin: 10px 0;
  position: relative;
}

.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #9f86c0, #e0b1cb);
  transition: width 0.3s ease;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: bold;
  color: #231942;
}

.breathing-circle {
  width: 150px;
  height: 150px;
  border-radius: 50%;
  margin: 20px auto;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 24px;
  font-weight: bold;
  box-shadow: 0 0 30px rgba(255,255,255,0.3);
  transition: all 0.5s ease;
}

.breath-inhale {
  background: #00FF88;
  color: #231942;
  animation: breathe-in 4s infinite;
}

.breath-hold {
  background: #FFD700;
  color: #231942;
  animation: breathe-hold 7s infinite;
}

.breath-exhale {
  background: #FF4466;
  color: #ffffff;
  animation: breathe-out 8s infinite;
}

@keyframes breathe-in {
  0%, 100% { transform: scale(1); }
  50% { transform: scale(1.1); }
}

@keyframes breathe-hold {
  0%, 100% { transform: scale(1); }
  50% { transform: scale(1.05); }
}

@keyframes breathe-out {
  0%, 100% { transform: scale(1); }
  50% { transform: scale(0.95); }
}

.vitals-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 20px;
}

.vital-item {
  text-align: center;
  padding: 15px;
  background: rgba(35, 25, 66, 0.5);
  border-radius: 10px;
  border: 1px solid #9f86c0;
}

.vital-icon {
  font-size: 36px;
  margin-bottom: 10px;
}

.sensor-list {
  display: flex;
  flex-direction: column;
  gap: 15px;
}

.sensor-item {
  background: rgba(35, 25, 66, 0.5);
  padding: 15px;
  border-radius: 10px;
  border: 1px solid #9f86c0;
}

.sensor-name {
  font-size: 14px;
  color: #be95c4;
  margin-bottom: 8px;
  text-transform: uppercase;
  letter-spacing: 1px;
}

.sensor-bar {
  background: #231942;
  height: 25px;
  border-radius: 12px;
  overflow: hidden;
  position: relative;
}

.sensor-bar-fill {
  height: 100%;
  background: linear-gradient(90deg, #be95c4, #e0b1cb);
  transition: width 0.3s ease;
  display: flex;
  align-items: center;
  justify-content: flex-end;
  padding-right: 10px;
  font-size: 12px;
  font-weight: bold;
  color: #231942;
}

.heart-icon {
  color: #FF4466;
  animation: heartbeat 1.5s infinite;
}

@keyframes heartbeat {
  0%, 100% { transform: scale(1); }
  10% { transform: scale(1.2); }
  20% { transform: scale(1); }
}

@media (max-width: 1200px) {
  .grid {
    grid-template-columns: 1fr;
  }
  .grid-full {
    grid-template-columns: 1fr;
  }
}
</style>
</head>
<body>

<div class="header">
  <div class="header-icon">♥</div>
  <h1>PISTOL SHOOTING STABILITY DASHBOARD</h1>
  <div class="live-indicator">
    <div class="live-dot"></div>
    <span>LIVE</span>
  </div>
</div>

<div class="container">
  
  <div class="grid">
    
    <!-- Motion Stability -->
    <div class="card">
      <div class="card-header">
        <h2>🔄 Motion Stability</h2>
      </div>
      <div class="card-body">
        <div class="metric-value" id="gyroValue">0.00</div>
        <div class="metric-label">Gyroscope Magnitude</div>
        <div class="progress-bar">
          <div class="progress-fill" id="gyroBar" style="width: 0%;">0%</div>
        </div>
        <div id="stabilityStatus" class="status-badge status-moving">MOVEMENT DETECTED</div>
      </div>
    </div>

    <!-- Breathing Guide -->
    <div class="card">
      <div class="card-header">
        <h2>🌬️ Breathing Phase</h2>
      </div>
      <div class="card-body">
        <div id="breathCircle" class="breathing-circle breath-inhale">INHALE</div>
        <div style="text-align: center; margin-top: 15px;">
          <div class="metric-label" id="breathInstruction">Breathe In</div>
        </div>
      </div>
    </div>

    <!-- Vital Signs -->
    <div class="card">
      <div class="card-header">
        <h2>❤️ Vital Signs</h2>
      </div>
      <div class="card-body">
        <div class="vitals-grid">
          <div class="vital-item">
            <div class="vital-icon heart-icon">♥</div>
            <div class="metric-value" style="font-size: 32px;" id="hrValue">--</div>
            <div class="metric-label">Heart Rate (bpm)</div>
          </div>
          <div class="vital-item">
            <div class="vital-icon">◉</div>
            <div class="metric-value" style="font-size: 32px;" id="spo2Value">--</div>
            <div class="metric-label">SpO2 (%)</div>
          </div>
        </div>
      </div>
    </div>

  </div>

  <div class="grid-full">
    
    <!-- Flex Sensors -->
    <div class="card">
      <div class="card-header">
        <h2>🖐️ Flex Sensors</h2>
      </div>
      <div class="card-body">
        <div class="sensor-list">
          <div class="sensor-item">
            <div class="sensor-name">Thumb</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="flex0" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Index Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="flex1" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Middle Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="flex2" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Ring Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="flex3" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Little Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="flex4" style="width: 0%;">0</div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- FSR Sensors -->
    <div class="card">
      <div class="card-header">
        <h2>✋ Force Sensors</h2>
      </div>
      <div class="card-body">
        <div class="sensor-list">
          <div class="sensor-item">
            <div class="sensor-name">Thumb</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="fsr0" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Index Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="fsr1" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Middle Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="fsr2" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Ring Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="fsr3" style="width: 0%;">0</div>
            </div>
          </div>
          <div class="sensor-item">
            <div class="sensor-name">Little Finger</div>
            <div class="sensor-bar">
              <div class="sensor-bar-fill" id="fsr4" style="width: 0%;">0</div>
            </div>
          </div>
        </div>
      </div>
    </div>

  </div>

</div>

<script>
function updateDashboard() {
  fetch('/data')
    .then(response => response.json())
    .then(data => {
      // Gyro & Stability
      document.getElementById('gyroValue').innerText = data.gyro.toFixed(2);
      let gyroPercent = Math.min((data.gyro / 5) * 100, 100);
      document.getElementById('gyroBar').style.width = gyroPercent + '%';
      document.getElementById('gyroBar').innerText = gyroPercent.toFixed(0) + '%';
      
      let statusBadge = document.getElementById('stabilityStatus');
      if (data.stable) {
        statusBadge.className = 'status-badge status-stable';
        statusBadge.innerText = 'STABLE - VIBRATING';
      } else {
        statusBadge.className = 'status-badge status-moving';
        statusBadge.innerText = 'MOVEMENT DETECTED';
      }

      // Breathing
      let breathCircle = document.getElementById('breathCircle');
      let breathInstruction = document.getElementById('breathInstruction');
      
      breathCircle.className = 'breathing-circle breath-' + data.breath.toLowerCase();
      breathCircle.innerText = data.breath;
      
      if (data.breath === 'INHALE') {
        breathInstruction.innerText = 'Breathe In';
      } else if (data.breath === 'HOLD') {
        breathInstruction.innerText = 'Hold';
      } else {
        breathInstruction.innerText = 'Breathe Out';
      }

      // Vitals
      document.getElementById('hrValue').innerText = data.hr;
      document.getElementById('spo2Value').innerText = data.spo2;

      // Flex Sensors
      for (let i = 0; i < 5; i++) {
        let percent = (data.flex[i] / 4095) * 100;
        let elem = document.getElementById('flex' + i);
        elem.style.width = percent + '%';
        elem.innerText = data.flex[i];
      }

      // FSR Sensors
      for (let i = 0; i < 5; i++) {
        let percent = (data.fsr[i] / 4095) * 100;
        let elem = document.getElementById('fsr' + i);
        elem.style.width = percent + '%';
        elem.innerText = data.fsr[i];
      }
    });
}

// Update every 100ms
setInterval(updateDashboard, 100);
updateDashboard();
</script>

</body>
</html>
)rawliteral";

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  rgb.begin();
  rgb.clear();
  rgb.show();

  /* WiFi Setup */
  Serial.println("\n🌐 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 Dashboard URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
  }

  /* Web Server Routes */
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });

  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"gyro\":" + String(gyroMagnitude, 2) + ",";
    json += "\"stable\":" + String(stable ? "true" : "false") + ",";
    json += "\"breath\":\"" + breathPhase + "\",";
    json += "\"hr\":\"" + (validHR ? String(heartRate) : "No Finger") + "\",";
    json += "\"spo2\":\"" + (validSpO2 ? String(spo2) : "--") + "\",";
    
    json += "\"flex\":[";
    for (int i = 0; i < 5; i++) {
      json += String(flex[i]);
      if (i < 4) json += ",";
    }
    json += "],";
    
    json += "\"fsr\":[";
    for (int i = 0; i < 5; i++) {
      json += String(fsr[i]);
      if (i < 4) json += ",";
    }
    json += "]";
    
    json += "}";
    
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("🌐 Web Server Started");

  /* MPU6050 */
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 NOT FOUND");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  /* MAX30102 */
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 NOT FOUND");
    while (1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0);

  lastMovementTime = millis();
  breathStart = millis();

  Serial.println("✅ System Ready (WiFi Dashboard Mode)");
}

/* ========== LOOP ========== */
void loop() {

  // Handle web server
  server.handleClient();

  /* ===== MPU6050 ===== */
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  gyroMagnitude =
    abs(g.gyro.x) +
    abs(g.gyro.y) +
    abs(g.gyro.z);

  if (gyroMagnitude > GYRO_THRESHOLD) {
    lastMovementTime = millis();
    digitalWrite(MOTOR_PIN, LOW);
  }

  stable = (millis() - lastMovementTime) >= STABILITY_TIME;
  digitalWrite(MOTOR_PIN, stable);

  /* ===== FLEX & FSR ===== */
  for (int i = 0; i < 5; i++) {
    flex[i] = analogRead(flexPins[i]);
    fsr[i]  = analogRead(fsrPins[i]);
  }

  /* ===== MAX30102 BUFFER ===== */
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.available())
      particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i]  = particleSensor.getIR();
    particleSensor.nextSample();
  }

  if (irBuffer[BUFFER_SIZE - 1] > 50000) {
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, BUFFER_SIZE,
      redBuffer,
      &spo2, &validSpO2,
      &heartRate, &validHR
    );
  } else {
    validHR = 0;
    validSpO2 = 0;
  }

  /* ===== BREATHING LOGIC ===== */
  unsigned long elapsed = millis() - breathStart;

  if (breathPhase == "INHALE" && elapsed >= BREATH_INHALE) {
    breathPhase = "HOLD";
    breathStart = millis();
  }
  else if (breathPhase == "HOLD" && elapsed >= BREATH_HOLD) {
    breathPhase = "EXHALE";
    breathStart = millis();
  }
  else if (breathPhase == "EXHALE" && elapsed >= BREATH_EXHALE) {
    breathPhase = "INHALE";
    breathStart = millis();
  }

  /* RGB LED */
  if (breathPhase == "INHALE")
    rgb.setPixelColor(0, rgb.Color(0,255,0));
  else if (breathPhase == "HOLD")
    rgb.setPixelColor(0, rgb.Color(255,200,0));
  else
    rgb.setPixelColor(0, rgb.Color(255,0,0));

  rgb.show();

  /* ===== SERIAL OUTPUT ===== */
  Serial.println("\n==============================");
  Serial.printf("Gyro Mag: %.2f | Stable: %s\n",
                gyroMagnitude,
                stable ? "YES → VIBRATE" : "NO");

  Serial.printf("Breathing Phase: %s\n", breathPhase.c_str());

  Serial.print("Heart Rate: ");
  if (validHR) Serial.print(heartRate);
  else Serial.print("No Finger");

  Serial.print(" | SpO2: ");
  if (validSpO2) Serial.print(spo2);
  else Serial.print("--");
  Serial.println();

  Serial.println("Flex Sensors:");
  for (int i = 0; i < 5; i++)
    Serial.printf("  %s: %d\n", fingers[i], flex[i]);

  Serial.println("FSR Sensors:");
  for (int i = 0; i < 5; i++)
    Serial.printf("  %s: %d\n", fingers[i], fsr[i]);

  Serial.println("==============================");

  delay(10);
}
