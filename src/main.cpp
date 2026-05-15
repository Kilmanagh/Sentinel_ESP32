/*
 * ============================================================================
 * SENTINEL FIRMWARE v2.0
 * ESP32 Multi-Sensor Environmental & Security Monitor
 * ============================================================================
 *
 * Features:
 * - LAN-only networking (no cloud dependencies)
 * - MAC-based unique device identity
 * - MQTT with Home Assistant auto-discovery
 * - BME280 environmental sensors (temperature °F, humidity, pressure)
 * - Comfort Index (composite thermal comfort score)
 * - Indoor Air Quality (IAQ) Score
 * - PIR motion detection
 * - Sound-level intrusion detection
 * - BLE device scanning & presence tracking
 * - Door/window reed switch sensor (GPIO 32)
 * - Onboard diagnostics (uptime, heap, WiFi RSSI, reconnect counts)
 *
 * Hardware:
 * - ESP32 DevKit or equivalent
 * - BME280 via I2C (SDA=21, SCL=22)
 * - PIR sensor on GPIO 27 (AM312)
 * - Analog sound sensor on GPIO 34 (MAX9814)
 * - Door reed switch on GPIO 32 (MC-38 NC, pulled HIGH = OPEN)
 *
 * Libraries required (install via Arduino Library Manager):
 * - WiFi (built-in)
 * - PubSubClient by Nick O'Leary
 * - Adafruit BME280 Library
 * - Adafruit Unified Sensor
 * - ArduinoJson by Benoit Blanchon (v6+)
 * - BLE (built-in ESP32 BLE Arduino)
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ============================================================================
//  USER CONFIGURATION — Edit these values for your environment
// ============================================================================

// WiFi credentials (LAN-only — no internet required)
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT broker (local network)
const char* MQTT_SERVER   = "192.168.1.100";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";         // leave blank if no auth
const char* MQTT_PASS     = "";         // leave blank if no auth

// ============================================================================
//  PIN DEFINITIONS
// ============================================================================

#define PIN_PIR           27
#define PIN_SOUND         34    // analog input
#define PIN_DOOR          32    // reed switch (NC, pulled HIGH internally)
#define PIN_LED           2     // onboard status LED
#define BME_SDA           21
#define BME_SCL           22

// ============================================================================
//  TIMING INTERVALS & THRESHOLDS (milliseconds)
// ============================================================================

#define INTERVAL_ENV        30000   // environmental readings every 30s
#define INTERVAL_DIAG       60000   // diagnostics every 60s
#define INTERVAL_BLE        120000  // BLE scan every 2 minutes
#define SOUND_SAMPLE_WINDOW 100     // sound sampling window (ms)
#define SOUND_THRESHOLD     1500    // Calibrated for MAX9814
#define PIR_COOLDOWN        10000   // motion re-trigger cooldown (ms)
#define DOOR_DEBOUNCE       50      // door switch debounce (ms)

// ============================================================================
//  GLOBAL OBJECTS & STATE
// ============================================================================

WiFiClient   espClient;
PubSubClient mqttClient(espClient);
Adafruit_BME280 bme;
BLEScan* pBLEScan = nullptr;

// MAC-based identity
String deviceMAC;
String deviceID;
String deviceName;
// MQTT topic base
String topicBase;

// Timing trackers
unsigned long lastEnvRead    = 0;
unsigned long lastDiagPub    = 0;
unsigned long lastBLEScan    = 0;
unsigned long lastMotionPub  = 0;

// Sensor state
bool     bmeAvailable       = false;
bool     lastDoorState      = HIGH;
// HIGH = CLOSED (pull-up) for NO switch; logic flipped for NC in publishDoorState
bool     currentDoorState   = HIGH;
unsigned long lastDoorChange = 0;
bool     motionDetected     = false;

// Diagnostics counters
unsigned long wifiReconnects = 0;
unsigned long mqttReconnects = 0;
unsigned long bootTime       = 0;

// ============================================================================
//  FORWARD DECLARATIONS
// ============================================================================

void setupIdentity();
void setupWiFi();
void setupMQTT();
void setupBME280();
void setupPIR();
void setupDoor();
void setupBLE();
void setupLED();

void reconnectWiFi();
void reconnectMQTT();
void publishAutoDiscovery();

void readAndPublishEnvironment();
void publishDoorState(bool state);
void checkMotion();
void checkSound();
void checkDoor();
void runBLEScan();
void publishDiagnostics();

float  computeComfortIndex(float tempF, float humidity);
int    computeIAQScore(float humidity, float pressureHPa);
String comfortLabel(float index);
String iaqLabel(int score);
void   mqttPublish(const char* topic, const char* payload, bool retained = false);

// ============================================================================
//  SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("  SENTINEL FIRMWARE v2.0"));
  Serial.println(F("  Multi-Sensor Security & Environment"));
  Serial.println(F("========================================"));

  setupIdentity();
  setupLED();
  setupWiFi();
  setupMQTT();
  setupBME280();
  setupPIR();
  setupDoor();
  setupBLE();

  bootTime = millis();
  // Connect MQTT and publish auto-discovery
  reconnectMQTT();
  if (mqttClient.connected()) {
    publishAutoDiscovery();
    // Publish initial door state at boot
    currentDoorState = digitalRead(PIN_DOOR);
    lastDoorState = currentDoorState;
    publishDoorState(currentDoorState);
    Serial.printf("[DOOR] Boot state: %s\n", currentDoorState == LOW ? "CLOSED" : "OPEN");

    // Publish initial environment reading
    readAndPublishEnvironment();
    // Publish initial diagnostics
    publishDiagnostics();
  }

  Serial.println(F("[SYSTEM] Setup complete — entering main loop"));
  Serial.println();
}

// ============================================================================
//  MAIN LOOP
// ============================================================================

void loop() {
  // Maintain connections
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  // Environmental readings (BME280 + Comfort + IAQ)
  if (now - lastEnvRead >= INTERVAL_ENV) {
    lastEnvRead = now;
    readAndPublishEnvironment();
  }

  // Motion detection (interrupt-style polling)
  checkMotion();

  // Sound intrusion detection
  checkSound();

  // Door state monitoring
  checkDoor();

  // BLE presence scan
  if (now - lastBLEScan >= INTERVAL_BLE) {
    lastBLEScan = now;
    runBLEScan();
  }

  // Diagnostics
  if (now - lastDiagPub >= INTERVAL_DIAG) {
    lastDiagPub = now;
    publishDiagnostics();
  }

  delay(10);  // yield
}

// ============================================================================
//  IDENTITY — MAC-based unique device ID
// ============================================================================

void setupIdentity() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  deviceMAC = String(macStr);

  char idStr[13];
  snprintf(idStr, sizeof(idStr), "%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  deviceID = String(idStr);

  deviceName = "sentinel_" + deviceID;
  topicBase  = "sentinel/" + deviceID;

  Serial.printf("[IDENTITY] MAC: %s\n", deviceMAC.c_str());
  Serial.printf("[IDENTITY] Device ID: %s\n", deviceID.c_str());
  Serial.printf("[IDENTITY] Name: %s\n", deviceName.c_str());
  Serial.printf("[IDENTITY] Topic base: %s\n", topicBase.c_str());
}

// ============================================================================
//  LED SETUP
// ============================================================================

void setupLED() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
}

void blinkLED(int times, int interval) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(interval);
    digitalWrite(PIN_LED, LOW);
    delay(interval);
  }
}

// ============================================================================
//  WiFi — LAN-Only Networking
// ============================================================================

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  reconnectWiFi();
}

void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiReconnects++;
    Serial.println();
    Serial.printf("[WIFI] Connected — IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    blinkLED(3, 100);
  } else {
    Serial.println();
    Serial.println(F("[WIFI] Connection failed — will retry"));
  }
}

// ============================================================================
//  MQTT — Connection & Publishing
// ============================================================================

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(60);
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  String willTopic = topicBase + "/status";
  Serial.print(F("[MQTT] Connecting to broker... "));

  bool connected = false;
  if (strlen(MQTT_USER) > 0) {
    connected = mqttClient.connect(deviceName.c_str(),
                                    MQTT_USER, MQTT_PASS,
                                    willTopic.c_str(), 1, true, "offline");
  } else {
    connected = mqttClient.connect(deviceName.c_str(),
                                    willTopic.c_str(), 1, true, "offline");
  }

  if (connected) {
    mqttReconnects++;
    Serial.println(F("connected"));
    mqttPublish(willTopic.c_str(), "online", true);
    blinkLED(2, 150);
  } else {
    Serial.printf("failed (rc=%d) — will retry in 5s\n", mqttClient.state());
    delay(5000);
  }
}

void mqttPublish(const char* topic, const char* payload, bool retained) {
  if (!mqttClient.connected()) return;
  mqttClient.publish(topic, payload, retained);
}

// ============================================================================
//  HOME ASSISTANT AUTO-DISCOVERY
// ============================================================================

void publishAutoDiscovery() {
  Serial.println(F("[DISCOVERY] Publishing Home Assistant auto-discovery configs..."));

  struct DiscoveryEntity {
    const char* component;   // "sensor", "binary_sensor"
    const char* objectSuffix;
    const char* name;
    const char* stateTopic;
    const char* valueTemplate;
    const char* deviceClass;
    const char* unit;
    const char* icon;
    const char* payloadOn;
    const char* payloadOff;
  };

  String stateTopicEnv   = topicBase + "/environment";
  String stateTopicMotion = topicBase + "/motion";
  String stateTopicSound = topicBase + "/sound";
  String stateTopicDoor  = topicBase + "/door";
  String stateTopicBLE   = topicBase + "/ble";
  String stateTopicDiag  = topicBase + "/diagnostics";

  DiscoveryEntity entities[] = {
    // Environmental sensors
    {"sensor", "temperature", "Temperature",
     stateTopicEnv.c_str(), "{{ value_json.temperature_f }}", "temperature", "°F", nullptr, nullptr, nullptr},
    {"sensor", "humidity", "Humidity",
     stateTopicEnv.c_str(), "{{ value_json.humidity }}", "humidity", "%", nullptr, nullptr, nullptr},
    {"sensor", "pressure", "Pressure",
     stateTopicEnv.c_str(), "{{ value_json.pressure_hpa }}", "atmospheric_pressure", "hPa", nullptr, nullptr, nullptr},
    {"sensor", "comfort_index", "Comfort Index",
     stateTopicEnv.c_str(), "{{ value_json.comfort_index }}", nullptr, nullptr, "mdi:thermometer-check", nullptr, nullptr},
    {"sensor", "comfort_label", "Comfort Level",
     stateTopicEnv.c_str(), "{{ value_json.comfort_label }}", nullptr, nullptr, "mdi:emoticon-outline", nullptr, nullptr},
    {"sensor", "iaq_score", "IAQ Score",
     stateTopicEnv.c_str(), "{{ value_json.iaq_score }}", nullptr, nullptr, "mdi:air-filter", nullptr, nullptr},
    {"sensor", "iaq_label", "Air Quality",
     stateTopicEnv.c_str(), "{{ value_json.iaq_label }}", nullptr, nullptr, "mdi:leaf", nullptr, nullptr},

    // Motion
    {"binary_sensor", "motion", "Motion",
     stateTopicMotion.c_str(), "{{ value_json.motion }}", "motion", nullptr, nullptr, "detected", "clear"},

    // Sound
    {"binary_sensor", "sound_intrusion", "Sound Intrusion",
     stateTopicSound.c_str(), "{{ value_json.intrusion }}", "sound", nullptr, nullptr, "detected", "clear"},
    {"sensor", "sound_level", "Sound Level",
     stateTopicSound.c_str(), "{{ value_json.peak_adc }}", nullptr, nullptr, "mdi:microphone", nullptr, nullptr},

    // Door
    {"binary_sensor", "door", "Door",
     stateTopicDoor.c_str(), "{{ value_json.state }}", "door", nullptr, nullptr, "OPEN", "CLOSED"},

    // BLE
    {"sensor", "ble_device_count", "BLE Devices",
     stateTopicBLE.c_str(), "{{ value_json.device_count }}", nullptr, nullptr, "mdi:bluetooth", nullptr, nullptr},

    // Diagnostics
    {"sensor", "uptime", "Uptime",
     stateTopicDiag.c_str(), "{{ value_json.uptime_sec }}", "duration", "s", nullptr, nullptr, nullptr},
    {"sensor", "free_heap", "Free Heap",
     stateTopicDiag.c_str(), "{{ value_json.free_heap }}", nullptr, "bytes", "mdi:memory", nullptr, nullptr},
    {"sensor", "wifi_rssi", "WiFi RSSI",
     stateTopicDiag.c_str(), "{{ value_json.rssi }}", "signal_strength", "dBm", nullptr, nullptr, nullptr},
  };

  int entityCount = sizeof(entities) / sizeof(entities[0]);

  for (int i = 0; i < entityCount; i++) {
    DiscoveryEntity& e = entities[i];
    String configTopic = String("homeassistant/") + e.component + "/" +
                         deviceID + "/" + e.objectSuffix + "/config";

    StaticJsonDocument<768> doc;
    doc["name"]        = String("Sentinel ") + e.name;
    doc["unique_id"]   = deviceID + "_" + e.objectSuffix;
    doc["state_topic"] = e.stateTopic;

    if (e.valueTemplate)  doc["value_template"] = e.valueTemplate;
    if (e.deviceClass)    doc["device_class"]   = e.deviceClass;
    if (e.unit)           doc["unit_of_measurement"] = e.unit;
    if (e.icon)           doc["icon"]           = e.icon;
    if (e.payloadOn)      doc["payload_on"]     = e.payloadOn;
    if (e.payloadOff)     doc["payload_off"]    = e.payloadOff;

    // Availability
    doc["availability_topic"] = topicBase + "/status";
    doc["payload_available"]  = "online";
    doc["payload_not_available"] = "offline";

    // Device block (shared across all entities)
    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(deviceID);
    dev["name"]         = deviceName;
    dev["manufacturer"] = "Sentinel DIY";
    dev["model"]        = "Sentinel Multi-Sensor v2";
    dev["sw_version"]   = "2.0.0";
    dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(configTopic.c_str(), payload, true);

    delay(50); // throttle to avoid overwhelming broker
  }

  Serial.printf("[DISCOVERY] Published %d entities\n", entityCount);
}

// ============================================================================
//  BME280 — Environmental Sensors
// ============================================================================

void setupBME280() {
  Wire.begin(BME_SDA, BME_SCL);
  if (bme.begin(0x76)) {
    bmeAvailable = true;
    Serial.println(F("[BME280] Sensor found at 0x76"));
  } else if (bme.begin(0x77)) {
    bmeAvailable = true;
    Serial.println(F("[BME280] Sensor found at 0x77"));
  } else {
    bmeAvailable = false;
    Serial.println(F("[BME280] Sensor NOT found — environmental readings disabled"));
  }

  if (bmeAvailable) {
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X2,   // temperature
                    Adafruit_BME280::SAMPLING_X16,  // pressure
                    Adafruit_BME280::SAMPLING_X1,   // humidity
                    Adafruit_BME280::FILTER_X16,
                    Adafruit_BME280::STANDBY_MS_500);
  }
}

void readAndPublishEnvironment() {
  if (!bmeAvailable) return;

  bme.takeForcedMeasurement();

  float tempC       = bme.readTemperature();
  float tempF       = (tempC * 9.0 / 5.0) + 32.0;
  float humidity     = bme.readHumidity();
  float pressureHPa = bme.readPressure() / 100.0;

  float comfort  = computeComfortIndex(tempF, humidity);
  int   iaq      = computeIAQScore(humidity, pressureHPa);

  StaticJsonDocument<384> doc;
  doc["temperature_f"]  = serialized(String(tempF, 1));
  doc["temperature_c"]  = serialized(String(tempC, 1));
  doc["humidity"]       = serialized(String(humidity, 1));
  doc["pressure_hpa"]   = serialized(String(pressureHPa, 1));
  doc["comfort_index"]  = serialized(String(comfort, 1));
  doc["comfort_label"]  = comfortLabel(comfort);
  doc["iaq_score"]      = iaq;
  doc["iaq_label"]      = iaqLabel(iaq);

  char payload[384];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/environment";
  mqttPublish(topic.c_str(), payload, true);

  Serial.printf("[ENV] %.1f°F  %.1f%%RH  %.1fhPa  Comfort:%.1f(%s)  IAQ:%d(%s)\n",
                tempF, humidity, pressureHPa,
                comfort, comfortLabel(comfort).c_str(),
                iaq, iaqLabel(iaq).c_str());
}

// ============================================================================
//  COMFORT INDEX — Composite thermal comfort score
// ============================================================================

float computeComfortIndex(float tempF, float humidity) {
  // Scale: 0 (very uncomfortable) to 100 (perfect comfort)
  // Ideal: 70-76°F at 40-50% humidity

  float tempScore;
  if (tempF >= 70.0 && tempF <= 76.0) {
    tempScore = 100.0;
  } else if (tempF < 70.0) {
    tempScore = max(0.0f, 100.0f - (70.0f - tempF) * 5.0f);
  } else {
    tempScore = max(0.0f, 100.0f - (tempF - 76.0f) * 5.0f);
  }

  float humScore;
  if (humidity >= 35.0 && humidity <= 55.0) {
    humScore = 100.0;
  } else if (humidity < 35.0) {
    humScore = max(0.0f, 100.0f - (35.0f - humidity) * 3.0f);
  } else {
    humScore = max(0.0f, 100.0f - (humidity - 55.0f) * 3.0f);
  }

  return (tempScore * 0.6) + (humScore * 0.4);
}

String comfortLabel(float index) {
  if (index >= 85.0) return "Excellent";
  if (index >= 70.0) return "Good";
  if (index >= 50.0) return "Fair";
  if (index >= 30.0) return "Poor";
  return "Critical";
}

// ============================================================================
//  IAQ SCORE — Indoor Air Quality estimation
// ============================================================================

int computeIAQScore(float humidity, float pressureHPa) {
  // Scale: 0 (worst) to 100 (best)
  // Without a VOC sensor, estimate from humidity and pressure stability

  int humidityScore;
  if (humidity >= 40.0 && humidity <= 60.0) {
    humidityScore = 100;
  } else if (humidity < 40.0) {
    humidityScore = max(0, (int)(100 - (40.0 - humidity) * 3.0));
  } else {
    humidityScore = max(0, (int)(100 - (humidity - 60.0) * 3.5));
  }

  // Pressure contribution: stable sea-level range ~1005-1025 hPa is ideal
  int pressureScore;
  if (pressureHPa >= 1005.0 && pressureHPa <= 1025.0) {
    pressureScore = 100;
  } else if (pressureHPa < 1005.0) {
    pressureScore = max(0, (int)(100 - (1005.0 - pressureHPa) * 2.0));
  } else {
    pressureScore = max(0, (int)(100 - (pressureHPa - 1025.0) * 2.0));
  }

  return (humidityScore * 70 + pressureScore * 30) / 100;
}

String iaqLabel(int score) {
  if (score >= 85) return "Excellent";
  if (score >= 70) return "Good";
  if (score >= 50) return "Moderate";
  if (score >= 30) return "Poor";
  return "Hazardous";
}

// ============================================================================
//  PIR — Motion Detection
// ============================================================================

void setupPIR() {
  pinMode(PIN_PIR, INPUT);
  Serial.println(F("[PIR] Motion sensor initialized on GPIO 27"));
}

void checkMotion() {
  int pirState = digitalRead(PIN_PIR);
  unsigned long now = millis();
  if (pirState == HIGH && !motionDetected) {
    if (now - lastMotionPub >= PIR_COOLDOWN) {
      motionDetected = true;
      lastMotionPub = now;

      StaticJsonDocument<64> doc;
      doc["motion"] = "detected";
      doc["timestamp"] = now / 1000;

      char payload[64];
      serializeJson(doc, payload, sizeof(payload));

      String topic = topicBase + "/motion";
      mqttPublish(topic.c_str(), payload);

      Serial.println(F("[PIR] Motion DETECTED"));
      blinkLED(1, 50);
    }
  } else if (pirState == LOW && motionDetected) {
    motionDetected = false;

    StaticJsonDocument<64> doc;
    doc["motion"] = "clear";
    doc["timestamp"] = now / 1000;

    char payload[64];
    serializeJson(doc, payload, sizeof(payload));

    String topic = topicBase + "/motion";
    mqttPublish(topic.c_str(), payload);

    Serial.println(F("[PIR] Motion CLEARED"));
  }
}

// ============================================================================
//  SOUND — Intrusion Detection
// ============================================================================

void checkSound() {
  unsigned long startMillis = millis();
  int peakValue = 0;
  int sample;

  // Sample analog sound sensor over the window period
  while (millis() - startMillis < SOUND_SAMPLE_WINDOW) {
    sample = analogRead(PIN_SOUND);
    if (sample > peakValue) {
      peakValue = sample;
    }
  }

  if (peakValue >= SOUND_THRESHOLD) {
    StaticJsonDocument<96> doc;
    doc["intrusion"]  = "detected";
    doc["peak_adc"]   = peakValue;
    doc["timestamp"]  = millis() / 1000;

    char payload[96];
    serializeJson(doc, payload, sizeof(payload));

    String topic = topicBase + "/sound";
    mqttPublish(topic.c_str(), payload);

    Serial.printf("[SOUND] Intrusion detected — peak ADC: %d\n", peakValue);
    blinkLED(2, 50);
  }
}

// ============================================================================
//  DOOR SENSOR — Reed Switch on GPIO 32
// ============================================================================

void setupDoor() {
  pinMode(PIN_DOOR, INPUT_PULLUP);
  currentDoorState = digitalRead(PIN_DOOR);
  lastDoorState = currentDoorState;
  lastDoorChange = millis();
  Serial.printf("[DOOR] Reed switch initialized on GPIO %d — state: %s\n",
                PIN_DOOR, currentDoorState == LOW ? "CLOSED" : "OPEN");
}

void checkDoor() {
  bool reading = digitalRead(PIN_DOOR);
  unsigned long now = millis();

  // Debounce: only accept state change after stable period
  if (reading != currentDoorState) {
    if (now - lastDoorChange >= DOOR_DEBOUNCE) {
      currentDoorState = reading;
      lastDoorChange = now;

      // Publish only on actual state change
      if (currentDoorState != lastDoorState) {
        lastDoorState = currentDoorState;
        publishDoorState(currentDoorState);
      }
    }
  } else {
    lastDoorChange = now;
  }
}

void publishDoorState(bool state) {
  // NC LOGIC: LOW (circuit closed) = CLOSED, HIGH (circuit open) = OPEN
  const char* doorStr = (state == LOW) ? "CLOSED" : "OPEN";

  StaticJsonDocument<96> doc;
  doc["state"]     = doorStr;
  doc["raw"]       = state;
  doc["timestamp"] = millis() / 1000;

  char payload[96];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/door";
  mqttPublish(topic.c_str(), payload, true);  // retained

  Serial.printf("[DOOR] State changed → %s\n", doorStr);
  blinkLED(1, 100);
}

// ============================================================================
//  BLE — Bluetooth Low Energy Scanning
// ============================================================================

class SentinelBLECallbacks : public BLEAdvertisedDeviceCallbacks {
  public:
    int deviceCount = 0;
    String deviceList;

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
      deviceCount++;
      if (deviceCount <= 20) {  // cap tracked devices
        if (deviceList.length() > 0) deviceList += ",";
        deviceList += "\"" + String(advertisedDevice.getAddress().toString().c_str()) + "\"";
      }
    }
};

void setupBLE() {
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(80);
  Serial.println(F("[BLE] Scanner initialized"));
}

void runBLEScan() {
  if (!pBLEScan) return;

  Serial.println(F("[BLE] Starting scan..."));

  SentinelBLECallbacks callbacks;
  pBLEScan->setAdvertisedDeviceCallbacks(&callbacks, false);
  BLEScanResults results = pBLEScan->start(10, false);  // 10-second scan
  pBLEScan->clearResults();

  // Build JSON payload
  String payload = "{\"device_count\":" + String(callbacks.deviceCount) +
                   ",\"devices\":[" + callbacks.deviceList +
                   "],\"scan_duration_sec\":10" +
                   ",\"timestamp\":" + String(millis() / 1000) + "}";

  String topic = topicBase + "/ble";
  mqttPublish(topic.c_str(), payload.c_str());

  Serial.printf("[BLE] Scan complete — %d devices found\n", callbacks.deviceCount);
}

// ============================================================================
//  DIAGNOSTICS — System Health
// ============================================================================

void publishDiagnostics() {
  unsigned long uptimeSec = (millis() - bootTime) / 1000;
  unsigned long days      = uptimeSec / 86400;
  unsigned long hours     = (uptimeSec % 86400) / 3600;
  unsigned long minutes   = (uptimeSec % 3600) / 60;

  StaticJsonDocument<256> doc;
  doc["uptime_sec"]      = uptimeSec;
  doc["uptime_fmt"]      = String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
  doc["free_heap"]       = ESP.getFreeHeap();
  doc["min_free_heap"]   = ESP.getMinFreeHeap();
  doc["rssi"]            = WiFi.RSSI();
  doc["wifi_reconnects"] = wifiReconnects;
  doc["mqtt_reconnects"] = mqttReconnects;
  doc["ip_address"]      = WiFi.localIP().toString();
  doc["mac"]             = deviceMAC;
  doc["bme280_ok"]       = bmeAvailable;
  doc["firmware"]        = "2.0.0";

  char payload[256];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/diagnostics";
  mqttPublish(topic.c_str(), payload, true);

  Serial.printf("[DIAG] Uptime:%s  Heap:%u  RSSI:%ddBm  WiFi↻:%lu  MQTT↻:%lu\n",
                doc["uptime_fmt"].as<const char*>(),
                ESP.getFreeHeap(), WiFi.RSSI(),
                wifiReconnects, mqttReconnects);
}
