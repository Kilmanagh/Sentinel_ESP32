#include <Arduino.h>

namespace {
constexpr char kNodeId[] = "sentinel-node-01";
constexpr uint8_t kStatusLedPin = 2;
constexpr uint8_t kSensorPin = 34;
constexpr uint32_t kSampleIntervalMs = 2000;
constexpr uint32_t kHeartbeatIntervalMs = 30000;
constexpr float kAdcReferenceVoltage = 3.3F;
constexpr uint16_t kAdcMax = 4095;
constexpr uint16_t kAlertThreshold = 2800;

uint32_t lastSampleMs = 0;
uint32_t lastHeartbeatMs = 0;
}

static float rawToVoltage(const uint16_t raw) {
  return (static_cast<float>(raw) * kAdcReferenceVoltage) / static_cast<float>(kAdcMax);
}

static bool intervalElapsed(const uint32_t now, const uint32_t since, const uint32_t intervalMs) {
  return static_cast<uint32_t>(now - since) >= intervalMs;
}

static void publishTelemetry(const uint16_t raw, const float voltage) {
  const bool alert = raw >= kAlertThreshold;

  digitalWrite(kStatusLedPin, alert ? HIGH : LOW);

  Serial.print("{\"node_id\":\"");
  Serial.print(kNodeId);
  Serial.print("\",\"sensor_raw\":");
  Serial.print(raw);
  Serial.print(",\"sensor_voltage\":");
  Serial.print(voltage, 3);
  Serial.print(",\"alert\":");
  Serial.print(alert ? "true" : "false");
  Serial.println("}");
}

void setup() {
  Serial.begin(115200);
  pinMode(kStatusLedPin, OUTPUT);
  pinMode(kSensorPin, INPUT);
  analogReadResolution(12);

  Serial.println("Sentinel ESP32 node booting...");
  Serial.print("Node ID: ");
  Serial.println(kNodeId);

  const uint32_t now = millis();
  lastSampleMs = now;
  lastHeartbeatMs = now;
}

void loop() {
  const uint32_t now = millis();
  const bool sampleDue = intervalElapsed(now, lastSampleMs, kSampleIntervalMs);
  const bool heartbeatDue = intervalElapsed(now, lastHeartbeatMs, kHeartbeatIntervalMs);

  if (sampleDue || heartbeatDue) {
    const uint16_t raw = analogRead(kSensorPin);
    const float voltage = rawToVoltage(raw);

    if (sampleDue) {
      lastSampleMs = now;
      publishTelemetry(raw, voltage);
    }

    if (heartbeatDue) {
      lastHeartbeatMs = now;
      Serial.print("Heartbeat: ");
      Serial.print(kNodeId);
      Serial.print(" | sensor_voltage=");
      Serial.println(voltage, 3);
    }
  }

  delay(10);
}
