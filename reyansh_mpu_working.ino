//MPU AND MAX WORKING OF REYANSH

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include "MAX30105.h"
#include "heartRate.h"

Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

// ===== PINS =====
#define MOTOR_PIN 6
#define SDA_PIN 12
#define SCL_PIN 13

// ===== SETTINGS =====
#define STABILITY_TIME 15000     // 15 seconds
#define GYRO_THRESHOLD 1.0       // rad/s

// ===== VARIABLES =====
unsigned long lastMovementTime = 0;

// Heart rate variables
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  // ===== MPU6050 INIT =====
  if (!mpu.begin()) {
    Serial.println("MPU6050 NOT FOUND");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // ===== MAX30102 INIT =====
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 NOT FOUND");
    while (1);
  }

  particleSensor.setup();              // Default config
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);
  particleSensor.setPulseAmplitudeGreen(0); // Turn off green LED

  lastMovementTime = millis();

  Serial.println("System Ready: Stability + HR + SpO2");
}

void loop() {
  // ===== MPU6050 =====
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float gyroMagnitude =
    abs(g.gyro.x) +
    abs(g.gyro.y) +
    abs(g.gyro.z);

  if (gyroMagnitude > GYRO_THRESHOLD) {
    lastMovementTime = millis();
    digitalWrite(MOTOR_PIN, LOW);
    Serial.println("Movement detected");
  }

  if (millis() - lastMovementTime >= STABILITY_TIME) {
    digitalWrite(MOTOR_PIN, HIGH);
    Serial.println("STABLE → VIBRATE");
  }

  // ===== MAX30102 =====
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      beatAvg = (beatAvg + beatsPerMinute) / 2;

      Serial.print("Heart Rate: ");
      Serial.print(beatsPerMinute);
      Serial.print(" BPM | Avg: ");
      Serial.println(beatAvg);
    }
  }

  if (irValue < 50000) {
    Serial.println("No finger detected");
  }

  delay(20);
}