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
 * - BLE beacon watchlist presence tracking
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
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>

// ============================================================================
//  USER CONFIGURATION
// ============================================================================

// Load credentials from include/secrets.h when present.
#if __has_include("secrets.h")
#include "secrets.h"
#else
// Fallback values to keep firmware buildable until secrets are configured.
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const bool  WIFI_USE_STATIC_IP = false;
const char* WIFI_STATIC_IP = "192.168.1.50";
const char* WIFI_GATEWAY   = "192.168.1.1";
const char* WIFI_SUBNET    = "255.255.255.0";
const char* WIFI_DNS1      = "1.1.1.1";
const char* WIFI_DNS2      = "8.8.8.8";
const bool  TIME_USE_NTP   = true;
const char* TZ_INFO        = "UTC0";
const char* NTP_SERVER_1   = "192.168.1.100";
const char* NTP_SERVER_2   = "";
const char* NTP_SERVER_3   = "";
const char* MQTT_SERVER   = "192.168.1.100";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";         // leave blank if no auth
const char* MQTT_PASS     = "";         // leave blank if no auth
#endif

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
#define INTERVAL_BLE         120000  // BLE scan every 2 minutes
#define BLE_SCAN_DURATION_MS 10000   // each BLE scan runs for 10s
#define BLE_PRESENCE_TIMEOUT_MS 150000 // mark a watched beacon away if unseen for about 2.5 minutes
#define BLE_MAX_WATCH_BEACONS 4      // fixed-size watchlist keeps BLE lightweight
#define BLE_ENROLLMENT_CACHE_SIZE 12
#define BLE_ENROLLMENT_LOG_COOLDOWN_MS 30000
#define SOUND_SAMPLE_WINDOW_MS 100  // sound sampling window (ms)
#define SOUND_THRESHOLD_ADC   1200  // peak-to-peak ADC threshold for MAX9814
#define SOUND_HOLD_MS         3000  // keep sound intrusion latched for HA
#define PIR_COOLDOWN        10000   // motion re-trigger cooldown (ms)
#define DOOR_DEBOUNCE       50      // door switch debounce (ms)
#define PIR_WARMUP_MS       30000   // AM312 settle time after power-up
#define PIR_DETECT_STABLE_MS 150    // HIGH must stay stable this long before detection
#define PIR_CLEAR_STABLE_MS 1500    // LOW must stay stable this long before clear
#define PIR_HOLD_MS         5000    // keep motion latched this long after last HIGH

// ============================================================================
//  RUNTIME CONFIG (NVS + compile-time defaults)
// ============================================================================

struct RuntimeConfig {
  String wifiSsid;
  String wifiPassword;
  bool useStaticIp;
  String staticIp;
  String gateway;
  String subnet;
  String dns1;
  String dns2;
  bool useNtp;
  String tzInfo;
  String ntpServer1;
  String ntpServer2;
  String ntpServer3;
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPass;
  unsigned long pirWarmupMs;
  unsigned long pirDetectStableMs;
  unsigned long pirClearStableMs;
  unsigned long pirHoldMs;
  unsigned long soundSampleWindowMs;
  unsigned long soundHoldMs;
  unsigned long soundThresholdAdc;
  String bleBeaconNames[BLE_MAX_WATCH_BEACONS];
  String bleBeaconUuids[BLE_MAX_WATCH_BEACONS];
  unsigned long bleBeaconMajors[BLE_MAX_WATCH_BEACONS];
  unsigned long bleBeaconMinors[BLE_MAX_WATCH_BEACONS];
};

// ============================================================================
//  GLOBAL OBJECTS & STATE
// ============================================================================

WiFiClient   espClient;
PubSubClient mqttClient(espClient);
Adafruit_BME280 bme;
BLEScan* pBLEScan = nullptr;
Preferences prefs;

RuntimeConfig runtimeConfig;
bool configLoadedFromNvs = false;

const uint16_t CONFIG_VERSION = 2;
const char* CONFIG_NAMESPACE = "sentinel_cfg";

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
bool     soundIntrusionActive = false;
unsigned long pirWarmupUntil = 0;
unsigned long pirHighSince   = 0;
unsigned long pirLowSince    = 0;
unsigned long pirHoldUntil   = 0;
unsigned long soundHoldUntil = 0;
int          pirLastRawState = LOW;

bool beaconPresent[BLE_MAX_WATCH_BEACONS] = {false, false, false, false};
int beaconRssi[BLE_MAX_WATCH_BEACONS] = {0, 0, 0, 0};
unsigned long beaconLastSeenMs[BLE_MAX_WATCH_BEACONS] = {0, 0, 0, 0};
bool bleEnrollmentLoggingEnabled = false;
String bleEnrollmentSeenIds[BLE_ENROLLMENT_CACHE_SIZE];
unsigned long bleEnrollmentSeenMs[BLE_ENROLLMENT_CACHE_SIZE] = {0};

// Diagnostics counters
unsigned long wifiReconnects = 0;
unsigned long mqttReconnects = 0;
unsigned long bootTime       = 0;

// ============================================================================
//  FORWARD DECLARATIONS
// ============================================================================

void setupIdentity();
void loadRuntimeConfigDefaults();
bool validateRuntimeConfig(const RuntimeConfig& cfg);
void loadRuntimeConfig();
bool saveRuntimeConfigToNvs();
bool clearRuntimeConfigFromNvs();
void printRuntimeConfig();
bool parseBoolValue(const String& rawValue, bool& outValue);
bool setRuntimeConfigValue(const String& key, const String& value);
void applyRuntimeConfigNow();
void processSerialCommand(String line);
void handleSerialCommands();
void setupWiFi();
void applyWiFiNetworkConfig();
void setupTimeSync();
void setupMQTT();
void setupBME280();
void setupPIR();
void setupDoor();
void setupBLE();
void setupLED();

void reconnectWiFi();
void reconnectMQTT();
void publishAutoDiscovery();
void publishPirConfigDiscovery();
void publishPirConfigState();
void publishSoundConfigDiscovery();
void publishSoundConfigState();
void publishBeaconDiscovery();
void publishBeaconState(bool initialPublish = false);
void publishBeaconConfigDiscovery();
void publishBeaconConfigState();
void clearLegacyBleTopics();
void publishSoundState(const char* intrusionState, int peakToPeak, int minSample, int maxSample);
void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
bool handlePirConfigCommand(const String& topic, const String& payload);
bool handleSoundConfigCommand(const String& topic, const String& payload);
bool handleBeaconConfigCommand(const String& topic, const String& payload);

void readAndPublishEnvironment();
void publishDoorState(bool state);
void checkMotion();
void checkSound();
void checkDoor();
void runBLEScan();
bool normalizeUuid(const String& rawUuid, String& normalizedUuid);
String defaultBeaconName(size_t index);
String beaconDisplayName(size_t index);
bool isBeaconConfigured(size_t index);
void processBleScanResult(BLEAdvertisedDevice advertisedDevice);
bool parseIBeaconAdvertisement(BLEAdvertisedDevice advertisedDevice, String& uuid, uint16_t& major, uint16_t& minor);
void logIBeaconSighting(const String& uuid, uint16_t major, uint16_t minor, int rssi);
void processBeaconAdvertisement(const String& uuid, uint16_t major, uint16_t minor, int rssi);
void expireBeaconPresence(unsigned long nowMs);
void publishDiagnostics();

float  computeComfortIndex(float tempF, float humidity);
int    computeIAQScore(float humidity, float pressureHPa);
String comfortLabel(float index);
String iaqLabel(int score);
uint32_t currentTimestampSeconds();
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

  loadRuntimeConfig();
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
    clearLegacyBleTopics();
    publishAutoDiscovery();
    publishPirConfigDiscovery();
    publishPirConfigState();
    publishSoundConfigDiscovery();
    publishSoundConfigState();
    publishSoundState("clear", 0, 0, 0);
    publishBeaconDiscovery();
    publishBeaconConfigDiscovery();
    publishBeaconConfigState();
    publishBeaconState(true);
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
  Serial.println(F("[SYSTEM] Serial config: cfg list"));
  Serial.println();
}

// ============================================================================
//  MAIN LOOP
// ============================================================================

void loop() {
  handleSerialCommands();

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

  // BLE beacon watchlist scan
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

void loadRuntimeConfigDefaults() {
  runtimeConfig.wifiSsid = WIFI_SSID;
  runtimeConfig.wifiPassword = WIFI_PASSWORD;
  runtimeConfig.useStaticIp = WIFI_USE_STATIC_IP;
  runtimeConfig.staticIp = WIFI_STATIC_IP;
  runtimeConfig.gateway = WIFI_GATEWAY;
  runtimeConfig.subnet = WIFI_SUBNET;
  runtimeConfig.dns1 = WIFI_DNS1;
  runtimeConfig.dns2 = WIFI_DNS2;
  runtimeConfig.useNtp = TIME_USE_NTP;
  runtimeConfig.tzInfo = TZ_INFO;
  runtimeConfig.ntpServer1 = NTP_SERVER_1;
  runtimeConfig.ntpServer2 = NTP_SERVER_2;
  runtimeConfig.ntpServer3 = NTP_SERVER_3;
  runtimeConfig.mqttServer = MQTT_SERVER;
  runtimeConfig.mqttPort = MQTT_PORT;
  runtimeConfig.mqttUser = MQTT_USER;
  runtimeConfig.mqttPass = MQTT_PASS;
  runtimeConfig.pirWarmupMs = PIR_WARMUP_MS;
  runtimeConfig.pirDetectStableMs = PIR_DETECT_STABLE_MS;
  runtimeConfig.pirClearStableMs = PIR_CLEAR_STABLE_MS;
  runtimeConfig.pirHoldMs = PIR_HOLD_MS;
  runtimeConfig.soundSampleWindowMs = SOUND_SAMPLE_WINDOW_MS;
  runtimeConfig.soundHoldMs = SOUND_HOLD_MS;
  runtimeConfig.soundThresholdAdc = SOUND_THRESHOLD_ADC;
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    runtimeConfig.bleBeaconNames[i] = defaultBeaconName(i);
    runtimeConfig.bleBeaconUuids[i] = "";
    runtimeConfig.bleBeaconMajors[i] = 0;
    runtimeConfig.bleBeaconMinors[i] = 0;
  }
}

bool validateRuntimeConfig(const RuntimeConfig& cfg) {
  if (cfg.wifiSsid.length() == 0 || cfg.mqttServer.length() == 0) {
    return false;
  }
  if (cfg.mqttPort < 1 || cfg.mqttPort > 65535) {
    return false;
  }
  if (cfg.pirWarmupMs > 300000 ||
      cfg.pirDetectStableMs < 10 || cfg.pirDetectStableMs > 10000 ||
      cfg.pirClearStableMs < 50 || cfg.pirClearStableMs > 60000 ||
      cfg.pirHoldMs < 250 || cfg.pirHoldMs > 60000 ||
      cfg.soundSampleWindowMs < 10 || cfg.soundSampleWindowMs > 1000 ||
      cfg.soundHoldMs < 100 || cfg.soundHoldMs > 60000 ||
      cfg.soundThresholdAdc < 10 || cfg.soundThresholdAdc > 4095) {
    return false;
  }
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    if (cfg.bleBeaconUuids[i].length() == 0) {
      continue;
    }

    String normalizedUuid;
    if (!normalizeUuid(cfg.bleBeaconUuids[i], normalizedUuid)) {
      return false;
    }
    if (cfg.bleBeaconMajors[i] > 65535 || cfg.bleBeaconMinors[i] > 65535) {
      return false;
    }
  }
  if (!cfg.useStaticIp) {
    return true;
  }

  IPAddress localIp;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  return localIp.fromString(cfg.staticIp) &&
         gateway.fromString(cfg.gateway) &&
         subnet.fromString(cfg.subnet) &&
         dns1.fromString(cfg.dns1) &&
         dns2.fromString(cfg.dns2);
}

void loadRuntimeConfig() {
  loadRuntimeConfigDefaults();
  configLoadedFromNvs = false;

  if (!prefs.begin(CONFIG_NAMESPACE, true)) {
    Serial.println(F("[CONFIG] NVS unavailable; using compile-time defaults"));
    return;
  }

  const uint16_t storedVersion = prefs.getUShort("cfg_ver", 0);
  if (storedVersion != CONFIG_VERSION) {
    Serial.printf("[CONFIG] No matching config version in NVS (found %u, need %u); using defaults\n",
                  storedVersion, CONFIG_VERSION);
    prefs.end();
    return;
  }

  RuntimeConfig nvsConfig = runtimeConfig;
  nvsConfig.wifiSsid = prefs.getString("wifi_ssid", runtimeConfig.wifiSsid);
  nvsConfig.wifiPassword = prefs.getString("wifi_pass", runtimeConfig.wifiPassword);
  nvsConfig.useStaticIp = prefs.getBool("st_ip_en", runtimeConfig.useStaticIp);
  nvsConfig.staticIp = prefs.getString("st_ip", runtimeConfig.staticIp);
  nvsConfig.gateway = prefs.getString("gw", runtimeConfig.gateway);
  nvsConfig.subnet = prefs.getString("subnet", runtimeConfig.subnet);
  nvsConfig.dns1 = prefs.getString("dns1", runtimeConfig.dns1);
  nvsConfig.dns2 = prefs.getString("dns2", runtimeConfig.dns2);
  nvsConfig.useNtp = prefs.getBool("use_ntp", runtimeConfig.useNtp);
  nvsConfig.tzInfo = prefs.getString("tz", runtimeConfig.tzInfo);
  nvsConfig.ntpServer1 = prefs.getString("ntp1", runtimeConfig.ntpServer1);
  nvsConfig.ntpServer2 = prefs.getString("ntp2", runtimeConfig.ntpServer2);
  nvsConfig.ntpServer3 = prefs.getString("ntp3", runtimeConfig.ntpServer3);
  nvsConfig.mqttServer = prefs.getString("mqtt_srv", runtimeConfig.mqttServer);
  nvsConfig.mqttPort = prefs.getInt("mqtt_prt", runtimeConfig.mqttPort);
  nvsConfig.mqttUser = prefs.getString("mqtt_usr", runtimeConfig.mqttUser);
  nvsConfig.mqttPass = prefs.getString("mqtt_pwd", runtimeConfig.mqttPass);
  nvsConfig.pirWarmupMs = prefs.getULong("pir_warm", runtimeConfig.pirWarmupMs);
  nvsConfig.pirDetectStableMs = prefs.getULong("pir_det", runtimeConfig.pirDetectStableMs);
  nvsConfig.pirClearStableMs = prefs.getULong("pir_clr", runtimeConfig.pirClearStableMs);
  nvsConfig.pirHoldMs = prefs.getULong("pir_hold", runtimeConfig.pirHoldMs);
  nvsConfig.soundSampleWindowMs = prefs.getULong("snd_win", runtimeConfig.soundSampleWindowMs);
  nvsConfig.soundHoldMs = prefs.getULong("snd_hold", runtimeConfig.soundHoldMs);
  nvsConfig.soundThresholdAdc = prefs.getULong("snd_thr", runtimeConfig.soundThresholdAdc);
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    char nameKey[12];
    char uuidKey[12];
    char majorKey[12];
    char minorKey[12];
    snprintf(nameKey, sizeof(nameKey), "bcn%u_name", (unsigned)(i + 1));
    snprintf(uuidKey, sizeof(uuidKey), "bcn%u_uuid", (unsigned)(i + 1));
    snprintf(majorKey, sizeof(majorKey), "bcn%u_maj", (unsigned)(i + 1));
    snprintf(minorKey, sizeof(minorKey), "bcn%u_min", (unsigned)(i + 1));
    nvsConfig.bleBeaconNames[i] = prefs.getString(nameKey, runtimeConfig.bleBeaconNames[i]);
    nvsConfig.bleBeaconUuids[i] = prefs.getString(uuidKey, runtimeConfig.bleBeaconUuids[i]);
    nvsConfig.bleBeaconMajors[i] = prefs.getULong(majorKey, runtimeConfig.bleBeaconMajors[i]);
    nvsConfig.bleBeaconMinors[i] = prefs.getULong(minorKey, runtimeConfig.bleBeaconMinors[i]);
  }
  prefs.end();

  if (!validateRuntimeConfig(nvsConfig)) {
    Serial.println(F("[CONFIG] NVS config invalid; using compile-time defaults"));
    return;
  }

  runtimeConfig = nvsConfig;
  configLoadedFromNvs = true;
  Serial.println(F("[CONFIG] Loaded runtime config from NVS"));
}

bool saveRuntimeConfigToNvs() {
  if (!validateRuntimeConfig(runtimeConfig)) {
    Serial.println(F("[CONFIG] Refusing to save: runtime config is invalid"));
    return false;
  }

  if (!prefs.begin(CONFIG_NAMESPACE, false)) {
    Serial.println(F("[CONFIG] Failed to open NVS for writing"));
    return false;
  }

  prefs.putUShort("cfg_ver", CONFIG_VERSION);
  prefs.putString("wifi_ssid", runtimeConfig.wifiSsid);
  prefs.putString("wifi_pass", runtimeConfig.wifiPassword);
  prefs.putBool("st_ip_en", runtimeConfig.useStaticIp);
  prefs.putString("st_ip", runtimeConfig.staticIp);
  prefs.putString("gw", runtimeConfig.gateway);
  prefs.putString("subnet", runtimeConfig.subnet);
  prefs.putString("dns1", runtimeConfig.dns1);
  prefs.putString("dns2", runtimeConfig.dns2);
  prefs.putBool("use_ntp", runtimeConfig.useNtp);
  prefs.putString("tz", runtimeConfig.tzInfo);
  prefs.putString("ntp1", runtimeConfig.ntpServer1);
  prefs.putString("ntp2", runtimeConfig.ntpServer2);
  prefs.putString("ntp3", runtimeConfig.ntpServer3);
  prefs.putString("mqtt_srv", runtimeConfig.mqttServer);
  prefs.putInt("mqtt_prt", runtimeConfig.mqttPort);
  prefs.putString("mqtt_usr", runtimeConfig.mqttUser);
  prefs.putString("mqtt_pwd", runtimeConfig.mqttPass);
  prefs.putULong("pir_warm", runtimeConfig.pirWarmupMs);
  prefs.putULong("pir_det", runtimeConfig.pirDetectStableMs);
  prefs.putULong("pir_clr", runtimeConfig.pirClearStableMs);
  prefs.putULong("pir_hold", runtimeConfig.pirHoldMs);
  prefs.putULong("snd_win", runtimeConfig.soundSampleWindowMs);
  prefs.putULong("snd_hold", runtimeConfig.soundHoldMs);
  prefs.putULong("snd_thr", runtimeConfig.soundThresholdAdc);
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    char nameKey[12];
    char uuidKey[12];
    char majorKey[12];
    char minorKey[12];
    snprintf(nameKey, sizeof(nameKey), "bcn%u_name", (unsigned)(i + 1));
    snprintf(uuidKey, sizeof(uuidKey), "bcn%u_uuid", (unsigned)(i + 1));
    snprintf(majorKey, sizeof(majorKey), "bcn%u_maj", (unsigned)(i + 1));
    snprintf(minorKey, sizeof(minorKey), "bcn%u_min", (unsigned)(i + 1));
    prefs.putString(nameKey, runtimeConfig.bleBeaconNames[i]);
    prefs.putString(uuidKey, runtimeConfig.bleBeaconUuids[i]);
    prefs.putULong(majorKey, runtimeConfig.bleBeaconMajors[i]);
    prefs.putULong(minorKey, runtimeConfig.bleBeaconMinors[i]);
  }
  prefs.end();

  configLoadedFromNvs = true;
  Serial.println(F("[CONFIG] Saved runtime config to NVS"));
  return true;
}

bool clearRuntimeConfigFromNvs() {
  if (!prefs.begin(CONFIG_NAMESPACE, false)) {
    Serial.println(F("[CONFIG] Failed to open NVS for clearing"));
    return false;
  }
  prefs.clear();
  prefs.end();
  configLoadedFromNvs = false;
  Serial.println(F("[CONFIG] Cleared NVS config namespace"));
  return true;
}

void printRuntimeConfig() {
  Serial.println(F("[CONFIG] Active runtime config"));
  Serial.printf("  source=%s\n", configLoadedFromNvs ? "NVS" : "defaults");
  Serial.printf("  wifi_ssid=%s\n", runtimeConfig.wifiSsid.c_str());
  Serial.printf("  wifi_pass=%s\n", runtimeConfig.wifiPassword.length() ? "***" : "");
  Serial.printf("  static_ip_enabled=%s\n", runtimeConfig.useStaticIp ? "true" : "false");
  Serial.printf("  static_ip=%s\n", runtimeConfig.staticIp.c_str());
  Serial.printf("  gateway=%s\n", runtimeConfig.gateway.c_str());
  Serial.printf("  subnet=%s\n", runtimeConfig.subnet.c_str());
  Serial.printf("  dns1=%s\n", runtimeConfig.dns1.c_str());
  Serial.printf("  dns2=%s\n", runtimeConfig.dns2.c_str());
  Serial.printf("  use_ntp=%s\n", runtimeConfig.useNtp ? "true" : "false");
  Serial.printf("  tz=%s\n", runtimeConfig.tzInfo.c_str());
  Serial.printf("  ntp1=%s\n", runtimeConfig.ntpServer1.c_str());
  Serial.printf("  ntp2=%s\n", runtimeConfig.ntpServer2.c_str());
  Serial.printf("  ntp3=%s\n", runtimeConfig.ntpServer3.c_str());
  Serial.printf("  mqtt_server=%s\n", runtimeConfig.mqttServer.c_str());
  Serial.printf("  mqtt_port=%d\n", runtimeConfig.mqttPort);
  Serial.printf("  mqtt_user=%s\n", runtimeConfig.mqttUser.c_str());
  Serial.printf("  mqtt_pass=%s\n", runtimeConfig.mqttPass.length() ? "***" : "");
  Serial.printf("  pir_warmup_ms=%lu\n", runtimeConfig.pirWarmupMs);
  Serial.printf("  pir_detect_stable_ms=%lu\n", runtimeConfig.pirDetectStableMs);
  Serial.printf("  pir_clear_stable_ms=%lu\n", runtimeConfig.pirClearStableMs);
  Serial.printf("  pir_hold_ms=%lu\n", runtimeConfig.pirHoldMs);
  Serial.printf("  sound_sample_window_ms=%lu\n", runtimeConfig.soundSampleWindowMs);
  Serial.printf("  sound_hold_ms=%lu\n", runtimeConfig.soundHoldMs);
  Serial.printf("  sound_threshold_adc=%lu\n", runtimeConfig.soundThresholdAdc);
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    Serial.printf("  ble_beacon%u_name=%s\n", (unsigned)(i + 1), beaconDisplayName(i).c_str());
    Serial.printf("  ble_beacon%u_uuid=%s\n", (unsigned)(i + 1), runtimeConfig.bleBeaconUuids[i].c_str());
    Serial.printf("  ble_beacon%u_major=%lu\n", (unsigned)(i + 1), runtimeConfig.bleBeaconMajors[i]);
    Serial.printf("  ble_beacon%u_minor=%lu\n", (unsigned)(i + 1), runtimeConfig.bleBeaconMinors[i]);
  }
}

bool normalizeUuid(const String& rawUuid, String& normalizedUuid) {
  String hexDigits;
  hexDigits.reserve(32);

  for (size_t i = 0; i < rawUuid.length(); i++) {
    char c = rawUuid.charAt(i);
    if (isHexadecimalDigit(c)) {
      hexDigits += (char)toupper(c);
    } else if (c == ':' || c == '-' || c == ' ') {
      continue;
    } else {
      return false;
    }
  }

  if (hexDigits.length() != 32) {
    return false;
  }

  normalizedUuid = hexDigits.substring(0, 8) + "-" +
                   hexDigits.substring(8, 12) + "-" +
                   hexDigits.substring(12, 16) + "-" +
                   hexDigits.substring(16, 20) + "-" +
                   hexDigits.substring(20, 32);

  return true;
}

String defaultBeaconName(size_t index) {
  return String("Beacon ") + String(index + 1);
}

String beaconDisplayName(size_t index) {
  if (runtimeConfig.bleBeaconNames[index].length() > 0) {
    return runtimeConfig.bleBeaconNames[index];
  }

  return defaultBeaconName(index);
}

bool isBeaconConfigured(size_t index) {
  return runtimeConfig.bleBeaconUuids[index].length() > 0;
}

bool parseBoolValue(const String& rawValue, bool& outValue) {
  String value = rawValue;
  value.trim();
  value.toLowerCase();
  if (value == "1" || value == "true" || value == "on" || value == "yes") {
    outValue = true;
    return true;
  }
  if (value == "0" || value == "false" || value == "off" || value == "no") {
    outValue = false;
    return true;
  }
  return false;
}

bool setRuntimeConfigValue(const String& key, const String& value) {
  auto parseUnsigned = [&](unsigned long& target) -> bool {
    for (size_t i = 0; i < value.length(); i++) {
      if (!isDigit(value.charAt(i))) return false;
    }
    target = strtoul(value.c_str(), nullptr, 10);
    return true;
  };

  if (key == "wifi_ssid") {
    runtimeConfig.wifiSsid = value;
  } else if (key == "wifi_pass") {
    runtimeConfig.wifiPassword = value;
  } else if (key == "static_ip_enabled") {
    bool parsed;
    if (!parseBoolValue(value, parsed)) return false;
    runtimeConfig.useStaticIp = parsed;
  } else if (key == "static_ip") {
    runtimeConfig.staticIp = value;
  } else if (key == "gateway") {
    runtimeConfig.gateway = value;
  } else if (key == "subnet") {
    runtimeConfig.subnet = value;
  } else if (key == "dns1") {
    runtimeConfig.dns1 = value;
  } else if (key == "dns2") {
    runtimeConfig.dns2 = value;
  } else if (key == "use_ntp") {
    bool parsed;
    if (!parseBoolValue(value, parsed)) return false;
    runtimeConfig.useNtp = parsed;
  } else if (key == "tz") {
    runtimeConfig.tzInfo = value;
  } else if (key == "ntp1") {
    runtimeConfig.ntpServer1 = value;
  } else if (key == "ntp2") {
    runtimeConfig.ntpServer2 = value;
  } else if (key == "ntp3") {
    runtimeConfig.ntpServer3 = value;
  } else if (key == "mqtt_server") {
    runtimeConfig.mqttServer = value;
  } else if (key == "mqtt_port") {
    int port = value.toInt();
    if (port < 1 || port > 65535) return false;
    runtimeConfig.mqttPort = port;
  } else if (key == "mqtt_user") {
    runtimeConfig.mqttUser = value;
  } else if (key == "mqtt_pass") {
    runtimeConfig.mqttPass = value;
  } else if (key == "pir_warmup_ms") {
    if (!parseUnsigned(runtimeConfig.pirWarmupMs)) return false;
  } else if (key == "pir_detect_stable_ms") {
    if (!parseUnsigned(runtimeConfig.pirDetectStableMs)) return false;
  } else if (key == "pir_clear_stable_ms") {
    if (!parseUnsigned(runtimeConfig.pirClearStableMs)) return false;
  } else if (key == "pir_hold_ms") {
    if (!parseUnsigned(runtimeConfig.pirHoldMs)) return false;
  } else if (key == "sound_sample_window_ms") {
    if (!parseUnsigned(runtimeConfig.soundSampleWindowMs)) return false;
  } else if (key == "sound_hold_ms") {
    if (!parseUnsigned(runtimeConfig.soundHoldMs)) return false;
  } else if (key == "sound_threshold_adc") {
    if (!parseUnsigned(runtimeConfig.soundThresholdAdc)) return false;
  } else {
    for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
      String nameKey = String("ble_beacon") + String(i + 1) + "_name";
      String uuidKey = String("ble_beacon") + String(i + 1) + "_uuid";
      String majorKey = String("ble_beacon") + String(i + 1) + "_major";
      String minorKey = String("ble_beacon") + String(i + 1) + "_minor";

      if (key == nameKey) {
        runtimeConfig.bleBeaconNames[i] = value;
        return true;
      }

      if (key == uuidKey) {
        String normalizedUuid;
        String trimmedValue = value;
        trimmedValue.trim();
        trimmedValue.toUpperCase();

        if (trimmedValue == "CLEAR" || trimmedValue == "NONE" || trimmedValue == "-") {
          runtimeConfig.bleBeaconUuids[i] = "";
          runtimeConfig.bleBeaconMajors[i] = 0;
          runtimeConfig.bleBeaconMinors[i] = 0;
          return true;
        }

        if (!normalizeUuid(value, normalizedUuid)) {
          return false;
        }

        runtimeConfig.bleBeaconUuids[i] = normalizedUuid;
        return true;
      }

      if (key == majorKey) {
        if (!parseUnsigned(runtimeConfig.bleBeaconMajors[i])) return false;
        return true;
      }

      if (key == minorKey) {
        if (!parseUnsigned(runtimeConfig.bleBeaconMinors[i])) return false;
        return true;
      }
    }

    return false;
  }

  return true;
}

void applyRuntimeConfigNow() {
  if (!validateRuntimeConfig(runtimeConfig)) {
    Serial.println(F("[CONFIG] Cannot apply: runtime config is invalid"));
    return;
  }

  WiFi.disconnect(false, false);
  mqttClient.disconnect();
  applyWiFiNetworkConfig();
  setupMQTT();
  setupPIR();
  setupBLE();
  reconnectWiFi();
  reconnectMQTT();
  if (mqttClient.connected()) {
    clearLegacyBleTopics();
    publishPirConfigDiscovery();
    publishPirConfigState();
    publishSoundConfigDiscovery();
    publishSoundConfigState();
    publishBeaconDiscovery();
    publishBeaconConfigDiscovery();
    publishBeaconConfigState();
    publishBeaconState(true);
  }
  Serial.println(F("[CONFIG] Applied runtime config to active services"));
}

void processSerialCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("ble help") || line.equalsIgnoreCase("ble ?")) {
    Serial.println(F("[BLE] Commands:"));
    Serial.println(F("  ble enroll on      Enable serial logging of seen iBeacon identifiers"));
    Serial.println(F("  ble enroll off     Disable serial enrollment logging"));
    Serial.println(F("  ble enroll status  Show whether enrollment logging is enabled"));
    return;
  }

  if (line.equalsIgnoreCase("ble enroll on")) {
    bleEnrollmentLoggingEnabled = true;
    memset(bleEnrollmentSeenMs, 0, sizeof(bleEnrollmentSeenMs));
    for (size_t i = 0; i < BLE_ENROLLMENT_CACHE_SIZE; i++) {
      bleEnrollmentSeenIds[i] = "";
    }
    Serial.println(F("[BLE] Enrollment logging enabled. Nearby iBeacons will be printed once per cooldown window."));
    return;
  }

  if (line.equalsIgnoreCase("ble enroll off")) {
    bleEnrollmentLoggingEnabled = false;
    Serial.println(F("[BLE] Enrollment logging disabled"));
    return;
  }

  if (line.equalsIgnoreCase("ble enroll status")) {
    Serial.printf("[BLE] Enrollment logging is %s\n", bleEnrollmentLoggingEnabled ? "ON" : "OFF");
    return;
  }

  if (line.equalsIgnoreCase("cfg help") || line.equalsIgnoreCase("cfg list") || line.equalsIgnoreCase("cfg ?")) {
    Serial.println(F("[CONFIG] Commands:"));
    Serial.println(F("  cfg show"));
    Serial.println(F("  cfg set <key> <value>"));
    Serial.println(F("  cfg save"));
    Serial.println(F("  cfg load"));
    Serial.println(F("  cfg apply"));
    Serial.println(F("  cfg reset"));
    Serial.println(F("[CONFIG] Keys:"));
    Serial.println(F("  wifi_ssid wifi_pass static_ip_enabled static_ip gateway subnet dns1 dns2"));
    Serial.println(F("  use_ntp tz ntp1 ntp2 ntp3 mqtt_server mqtt_port mqtt_user mqtt_pass"));
    Serial.println(F("  pir_warmup_ms pir_detect_stable_ms pir_clear_stable_ms pir_hold_ms"));
    Serial.println(F("  sound_sample_window_ms sound_hold_ms sound_threshold_adc"));
    Serial.println(F("  ble_beacon1_name ble_beacon1_uuid ble_beacon1_major ble_beacon1_minor"));
    Serial.println(F("  ble_beacon2_name ble_beacon2_uuid ble_beacon2_major ble_beacon2_minor"));
    Serial.println(F("  ble_beacon3_name ble_beacon3_uuid ble_beacon3_major ble_beacon3_minor"));
    Serial.println(F("  ble_beacon4_name ble_beacon4_uuid ble_beacon4_major ble_beacon4_minor"));
    Serial.println(F("  Use value CLEAR for ble_beacon*_uuid to empty a slot"));
    Serial.println(F("[BLE] Use 'ble help' for iBeacon enrollment logging commands"));
    return;
  }

  if (line.equalsIgnoreCase("cfg show")) {
    printRuntimeConfig();
    return;
  }

  if (line.equalsIgnoreCase("cfg save")) {
    saveRuntimeConfigToNvs();
    return;
  }

  if (line.equalsIgnoreCase("cfg load")) {
    loadRuntimeConfig();
    Serial.println(F("[CONFIG] Reloaded runtime config from defaults/NVS"));
    return;
  }

  if (line.equalsIgnoreCase("cfg apply")) {
    applyRuntimeConfigNow();
    return;
  }

  if (line.equalsIgnoreCase("cfg reset")) {
    if (clearRuntimeConfigFromNvs()) {
      loadRuntimeConfigDefaults();
      configLoadedFromNvs = false;
      Serial.println(F("[CONFIG] Runtime config reset to compile-time defaults"));
    }
    return;
  }

  if (line.startsWith("cfg set ")) {
    String args = line.substring(8);
    int split = args.indexOf(' ');
    if (split <= 0) {
      Serial.println(F("[CONFIG] Usage: cfg set <key> <value>"));
      return;
    }

    String key = args.substring(0, split);
    String value = args.substring(split + 1);
    key.trim();
    value.trim();

    if (value.length() == 0) {
      Serial.println(F("[CONFIG] Value cannot be empty for this command"));
      return;
    }

    if (!setRuntimeConfigValue(key, value)) {
      Serial.printf("[CONFIG] Invalid key or value: %s\n", key.c_str());
      return;
    }

    Serial.printf("[CONFIG] Updated %s\n", key.c_str());
    if (!validateRuntimeConfig(runtimeConfig)) {
      Serial.println(F("[CONFIG] Warning: current runtime config is invalid until corrected"));
    }
    return;
  }

  Serial.println(F("[CONFIG] Unknown command. Use: cfg help"));
}

void handleSerialCommands() {
  static String lineBuffer;

  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (lineBuffer.length() > 0) {
        processSerialCommand(lineBuffer);
        lineBuffer = "";
      }
      continue;
    }

    if (lineBuffer.length() < 255) {
      lineBuffer += c;
    }
  }
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
  applyWiFiNetworkConfig();
  reconnectWiFi();
}

void applyWiFiNetworkConfig() {
  if (!runtimeConfig.useStaticIp) {
    Serial.println(F("[WIFI] Using DHCP network config"));
    return;
  }

  IPAddress localIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  bool parsed = localIP.fromString(runtimeConfig.staticIp) &&
                gateway.fromString(runtimeConfig.gateway) &&
                subnet.fromString(runtimeConfig.subnet) &&
                dns1.fromString(runtimeConfig.dns1) &&
                dns2.fromString(runtimeConfig.dns2);

  if (!parsed) {
    Serial.println(F("[WIFI] Invalid static network settings; falling back to DHCP"));
    return;
  }

  if (WiFi.config(localIP, gateway, subnet, dns1, dns2)) {
    Serial.printf("[WIFI] Static network config set: IP=%s GW=%s MASK=%s DNS1=%s DNS2=%s\n",
                  runtimeConfig.staticIp.c_str(), runtimeConfig.gateway.c_str(),
                  runtimeConfig.subnet.c_str(), runtimeConfig.dns1.c_str(),
                  runtimeConfig.dns2.c_str());
  } else {
    Serial.println(F("[WIFI] Failed to apply static network config; using DHCP"));
  }
}

void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[WIFI] Connecting to %s (%s config)\n",
                runtimeConfig.wifiSsid.c_str(),
                configLoadedFromNvs ? "NVS" : "defaults");
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPassword.c_str());

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
    setupTimeSync();
    blinkLED(3, 100);
  } else {
    Serial.println();
    Serial.println(F("[WIFI] Connection failed — will retry"));
  }
}

void setupTimeSync() {
  if (!runtimeConfig.useNtp || WiFi.status() != WL_CONNECTED) return;

  time_t now = time(nullptr);
  if (now > 1700000000) return;  // already synced

  Serial.printf("[TIME] Syncing via NTP (tz=%s, servers=%s,%s,%s)\n",
                runtimeConfig.tzInfo.c_str(), runtimeConfig.ntpServer1.c_str(),
                runtimeConfig.ntpServer2.c_str(), runtimeConfig.ntpServer3.c_str());
  configTzTime(runtimeConfig.tzInfo.c_str(), runtimeConfig.ntpServer1.c_str(),
               runtimeConfig.ntpServer2.c_str(), runtimeConfig.ntpServer3.c_str());

  const int maxAttempts = 20;
  int attempt = 0;
  while (attempt < maxAttempts) {
    now = time(nullptr);
    if (now > 1700000000) {
      Serial.printf("[TIME] NTP sync complete: %lu\n", (unsigned long)now);
      return;
    }
    delay(250);
    attempt++;
  }

  Serial.println(F("[TIME] NTP sync timeout; using uptime timestamps until sync succeeds"));
}

uint32_t currentTimestampSeconds() {
  time_t now = time(nullptr);
  if (now > 1700000000) {
    return (uint32_t)now;
  }
  return millis() / 1000;
}

// ============================================================================
//  MQTT — Connection & Publishing
// ============================================================================

void setupMQTT() {
  mqttClient.setServer(runtimeConfig.mqttServer.c_str(), runtimeConfig.mqttPort);
  mqttClient.setBufferSize(4096);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(mqttMessageCallback);
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  String willTopic = topicBase + "/status";
  Serial.printf("[MQTT] Connecting to broker %s:%d... ",
                runtimeConfig.mqttServer.c_str(), runtimeConfig.mqttPort);

  bool connected = false;
  if (runtimeConfig.mqttUser.length() > 0) {
    connected = mqttClient.connect(deviceName.c_str(),
                                    runtimeConfig.mqttUser.c_str(), runtimeConfig.mqttPass.c_str(),
                                    willTopic.c_str(), 1, true, "offline");
  } else {
    connected = mqttClient.connect(deviceName.c_str(),
                                    willTopic.c_str(), 1, true, "offline");
  }

  if (connected) {
    mqttReconnects++;
    Serial.println(F("connected"));
    mqttPublish(willTopic.c_str(), "online", true);
    String pirWarmupSetTopic = topicBase + "/config/pir_warmup_ms/set";
    String pirDetectSetTopic = topicBase + "/config/pir_detect_stable_ms/set";
    String pirClearSetTopic = topicBase + "/config/pir_clear_stable_ms/set";
    String pirHoldSetTopic = topicBase + "/config/pir_hold_ms/set";
    String soundWindowSetTopic = topicBase + "/config/sound_sample_window_ms/set";
    String soundHoldSetTopic = topicBase + "/config/sound_hold_ms/set";
    String soundThresholdSetTopic = topicBase + "/config/sound_threshold_adc/set";
    mqttClient.subscribe(pirWarmupSetTopic.c_str());
    mqttClient.subscribe(pirDetectSetTopic.c_str());
    mqttClient.subscribe(pirClearSetTopic.c_str());
    mqttClient.subscribe(pirHoldSetTopic.c_str());
    mqttClient.subscribe(soundWindowSetTopic.c_str());
    mqttClient.subscribe(soundHoldSetTopic.c_str());
    mqttClient.subscribe(soundThresholdSetTopic.c_str());
    for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
      String beaconNameSetTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_name/set";
      String beaconUuidSetTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_uuid/set";
      String beaconMajorSetTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_major/set";
      String beaconMinorSetTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_minor/set";
      mqttClient.subscribe(beaconNameSetTopic.c_str());
      mqttClient.subscribe(beaconUuidSetTopic.c_str());
      mqttClient.subscribe(beaconMajorSetTopic.c_str());
      mqttClient.subscribe(beaconMinorSetTopic.c_str());
    }
    blinkLED(2, 150);
  } else {
    Serial.printf("failed (rc=%d) — will retry in 5s\n", mqttClient.state());
    delay(5000);
  }
}

void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr;
  payloadStr.reserve(length);

  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  payloadStr.trim();

  if (handlePirConfigCommand(topicStr, payloadStr)) {
    return;
  }
  if (handleSoundConfigCommand(topicStr, payloadStr)) {
    return;
  }
  if (handleBeaconConfigCommand(topicStr, payloadStr)) {
    return;
  }

  Serial.printf("[MQTT] Unhandled command topic: %s\n", topicStr.c_str());
}

bool handlePirConfigCommand(const String& topic, const String& payload) {
  String key;

  if (topic.endsWith("/config/pir_warmup_ms/set")) {
    key = "pir_warmup_ms";
  } else if (topic.endsWith("/config/pir_detect_stable_ms/set")) {
    key = "pir_detect_stable_ms";
  } else if (topic.endsWith("/config/pir_clear_stable_ms/set")) {
    key = "pir_clear_stable_ms";
  } else if (topic.endsWith("/config/pir_hold_ms/set")) {
    key = "pir_hold_ms";
  } else {
    return false;
  }

  RuntimeConfig previousConfig = runtimeConfig;
  if (!setRuntimeConfigValue(key, payload) || !validateRuntimeConfig(runtimeConfig)) {
    runtimeConfig = previousConfig;
    Serial.printf("[MQTT] Rejected PIR config update: %s=%s\n", key.c_str(), payload.c_str());
    publishPirConfigState();
    return true;
  }

  setupPIR();
  if (!saveRuntimeConfigToNvs()) {
    runtimeConfig = previousConfig;
    setupPIR();
    Serial.printf("[MQTT] Failed to persist PIR config update: %s=%s\n", key.c_str(), payload.c_str());
    publishPirConfigState();
    return true;
  }

  Serial.printf("[MQTT] Applied PIR config update: %s=%s\n", key.c_str(), payload.c_str());
  publishPirConfigState();
  return true;
}

bool handleSoundConfigCommand(const String& topic, const String& payload) {
  String key;

  if (topic.endsWith("/config/sound_sample_window_ms/set")) {
    key = "sound_sample_window_ms";
  } else if (topic.endsWith("/config/sound_hold_ms/set")) {
    key = "sound_hold_ms";
  } else if (topic.endsWith("/config/sound_threshold_adc/set")) {
    key = "sound_threshold_adc";
  } else {
    return false;
  }

  RuntimeConfig previousConfig = runtimeConfig;
  if (!setRuntimeConfigValue(key, payload) || !validateRuntimeConfig(runtimeConfig)) {
    runtimeConfig = previousConfig;
    Serial.printf("[MQTT] Rejected sound config update: %s=%s\n", key.c_str(), payload.c_str());
    publishSoundConfigState();
    return true;
  }

  if (!saveRuntimeConfigToNvs()) {
    runtimeConfig = previousConfig;
    Serial.printf("[MQTT] Failed to persist sound config update: %s=%s\n", key.c_str(), payload.c_str());
    publishSoundConfigState();
    return true;
  }

  Serial.printf("[MQTT] Applied sound config update: %s=%s\n", key.c_str(), payload.c_str());
  publishSoundConfigState();
  return true;
}

bool handleBeaconConfigCommand(const String& topic, const String& payload) {
  String key;

  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String nameTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_name/set";
    String uuidTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_uuid/set";
    String majorTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_major/set";
    String minorTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_minor/set";

    if (topic == nameTopic) {
      key = String("ble_beacon") + String(i + 1) + "_name";
      break;
    }
    if (topic == uuidTopic) {
      key = String("ble_beacon") + String(i + 1) + "_uuid";
      break;
    }
    if (topic == majorTopic) {
      key = String("ble_beacon") + String(i + 1) + "_major";
      break;
    }
    if (topic == minorTopic) {
      key = String("ble_beacon") + String(i + 1) + "_minor";
      break;
    }
  }

  if (key.length() == 0) {
    return false;
  }

  RuntimeConfig previousConfig = runtimeConfig;
  if (!setRuntimeConfigValue(key, payload) || !validateRuntimeConfig(runtimeConfig)) {
    runtimeConfig = previousConfig;
    Serial.printf("[MQTT] Rejected beacon config update: %s=%s\n", key.c_str(), payload.c_str());
    publishBeaconConfigState();
    publishBeaconState(true);
    return true;
  }

  if (!saveRuntimeConfigToNvs()) {
    runtimeConfig = previousConfig;
    Serial.printf("[MQTT] Failed to persist beacon config update: %s=%s\n", key.c_str(), payload.c_str());
    publishBeaconConfigState();
    publishBeaconState(true);
    return true;
  }

  setupBLE();
  publishBeaconDiscovery();
  publishBeaconConfigDiscovery();
  publishBeaconConfigState();
  publishBeaconState(true);
  Serial.printf("[MQTT] Applied beacon config update: %s=%s\n", key.c_str(), payload.c_str());
  return true;
}

void mqttPublish(const char* topic, const char* payload, bool retained) {
  if (!mqttClient.connected()) return;
  if (!mqttClient.publish(topic, payload, retained)) {
    Serial.printf("[MQTT] Publish failed — topic=%s bytes=%u retained=%s\n",
                  topic, (unsigned)strlen(payload), retained ? "true" : "false");
  }
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

void publishPirConfigDiscovery() {
  struct PirNumberDiscovery {
    const char* objectSuffix;
    const char* name;
    const char* key;
    int minValue;
    int maxValue;
    int step;
    const char* unit;
    const char* icon;
  };

  PirNumberDiscovery numbers[] = {
    {"pir_warmup_ms", "PIR Warmup", "pir_warmup_ms", 0, 300000, 1000, "ms", "mdi:timer-sand"},
    {"pir_detect_stable_ms", "PIR Detect Stable", "pir_detect_stable_ms", 10, 10000, 10, "ms", "mdi:motion-sensor"},
    {"pir_clear_stable_ms", "PIR Clear Stable", "pir_clear_stable_ms", 50, 60000, 50, "ms", "mdi:motion-sensor-off"},
    {"pir_hold_ms", "PIR Hold", "pir_hold_ms", 250, 60000, 250, "ms", "mdi:timer-outline"},
  };

  int numberCount = sizeof(numbers) / sizeof(numbers[0]);
  for (int i = 0; i < numberCount; i++) {
    PirNumberDiscovery& number = numbers[i];
    String configTopic = String("homeassistant/number/") + deviceID + "/" + number.objectSuffix + "/config";
    String stateTopic = topicBase + "/config/" + number.key + "/state";
    String commandTopic = topicBase + "/config/" + number.key + "/set";

    StaticJsonDocument<768> doc;
    doc["name"] = String("Sentinel ") + number.name;
    doc["unique_id"] = deviceID + "_" + number.objectSuffix;
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["min"] = number.minValue;
    doc["max"] = number.maxValue;
    doc["step"] = number.step;
    doc["mode"] = "box";
    doc["availability_topic"] = topicBase + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    if (number.unit) {
      doc["unit_of_measurement"] = number.unit;
    }
    if (number.icon) {
      doc["icon"] = number.icon;
    }

    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(deviceID);
    dev["name"] = deviceName;
    dev["manufacturer"] = "Sentinel DIY";
    dev["model"] = "Sentinel Multi-Sensor v2";
    dev["sw_version"] = "2.0.0";
    dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(configTopic.c_str(), payload, true);
    delay(50);
  }

  Serial.printf("[DISCOVERY] Published %d PIR config entities\n", numberCount);
}

void publishPirConfigState() {
  String warmupTopic = topicBase + "/config/pir_warmup_ms/state";
  String detectTopic = topicBase + "/config/pir_detect_stable_ms/state";
  String clearTopic = topicBase + "/config/pir_clear_stable_ms/state";
  String holdTopic = topicBase + "/config/pir_hold_ms/state";

  mqttPublish(warmupTopic.c_str(), String(runtimeConfig.pirWarmupMs).c_str(), true);
  mqttPublish(detectTopic.c_str(), String(runtimeConfig.pirDetectStableMs).c_str(), true);
  mqttPublish(clearTopic.c_str(), String(runtimeConfig.pirClearStableMs).c_str(), true);
  mqttPublish(holdTopic.c_str(), String(runtimeConfig.pirHoldMs).c_str(), true);
}

void publishSoundConfigDiscovery() {
  struct SoundNumberDiscovery {
    const char* objectSuffix;
    const char* name;
    const char* key;
    int minValue;
    int maxValue;
    int step;
    const char* unit;
    const char* icon;
  };

  SoundNumberDiscovery numbers[] = {
    {"sound_sample_window_ms", "Sound Sample Window", "sound_sample_window_ms", 10, 1000, 10, "ms", "mdi:waveform"},
    {"sound_hold_ms", "Sound Hold", "sound_hold_ms", 100, 60000, 100, "ms", "mdi:timer-outline"},
    {"sound_threshold_adc", "Sound Threshold", "sound_threshold_adc", 10, 4095, 10, "adc", "mdi:microphone-alert"},
  };

  int numberCount = sizeof(numbers) / sizeof(numbers[0]);
  for (int i = 0; i < numberCount; i++) {
    SoundNumberDiscovery& number = numbers[i];
    String configTopic = String("homeassistant/number/") + deviceID + "/" + number.objectSuffix + "/config";
    String stateTopic = topicBase + "/config/" + number.key + "/state";
    String commandTopic = topicBase + "/config/" + number.key + "/set";

    StaticJsonDocument<768> doc;
    doc["name"] = String("Sentinel ") + number.name;
    doc["unique_id"] = deviceID + "_" + number.objectSuffix;
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["min"] = number.minValue;
    doc["max"] = number.maxValue;
    doc["step"] = number.step;
    doc["mode"] = "box";
    doc["availability_topic"] = topicBase + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    if (number.unit) {
      doc["unit_of_measurement"] = number.unit;
    }
    if (number.icon) {
      doc["icon"] = number.icon;
    }

    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(deviceID);
    dev["name"] = deviceName;
    dev["manufacturer"] = "Sentinel DIY";
    dev["model"] = "Sentinel Multi-Sensor v2";
    dev["sw_version"] = "2.0.0";
    dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(configTopic.c_str(), payload, true);
    delay(50);
  }

  Serial.printf("[DISCOVERY] Published %d sound config entities\n", numberCount);
}

void publishSoundConfigState() {
  String windowTopic = topicBase + "/config/sound_sample_window_ms/state";
  String holdTopic = topicBase + "/config/sound_hold_ms/state";
  String thresholdTopic = topicBase + "/config/sound_threshold_adc/state";

  mqttPublish(windowTopic.c_str(), String(runtimeConfig.soundSampleWindowMs).c_str(), true);
  mqttPublish(holdTopic.c_str(), String(runtimeConfig.soundHoldMs).c_str(), true);
  mqttPublish(thresholdTopic.c_str(), String(runtimeConfig.soundThresholdAdc).c_str(), true);
}

void clearLegacyBleTopics() {
  const char* legacyDiscoveryTopics[] = {
    "homeassistant/sensor/%s/ble_device_count/config",
    "homeassistant/sensor/%s/ble_active_devices/config"
  };

  for (size_t i = 0; i < sizeof(legacyDiscoveryTopics) / sizeof(legacyDiscoveryTopics[0]); i++) {
    char topic[128];
    snprintf(topic, sizeof(topic), legacyDiscoveryTopics[i], deviceID.c_str());
    mqttClient.publish(topic, "", true);
    delay(20);
  }

  String legacyBleTopic = topicBase + "/ble";
  String legacyBleListTopic = topicBase + "/ble/list";
  mqttClient.publish(legacyBleTopic.c_str(), "", true);
  mqttClient.publish(legacyBleListTopic.c_str(), "", true);

  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String legacyConfigTopic = String("homeassistant/text/") + deviceID + "/ble_beacon" + String(i + 1) + "_addr/config";
    String legacyStateTopic = topicBase + "/config/ble_beacon" + String(i + 1) + "_addr/state";
    mqttClient.publish(legacyConfigTopic.c_str(), "", true);
    mqttClient.publish(legacyStateTopic.c_str(), "", true);
  }
}

void publishBeaconDiscovery() {
  String stateTopic = topicBase + "/beacons";

  {
    String configTopic = String("homeassistant/sensor/") + deviceID + "/ble_beacons_present/config";

    StaticJsonDocument<768> doc;
    doc["name"] = "Sentinel BLE Beacons Present";
    doc["unique_id"] = deviceID + "_ble_beacons_present";
    doc["state_topic"] = stateTopic;
    doc["value_template"] = "{{ value_json.present_count }}";
    doc["icon"] = "mdi:bluetooth";
    doc["availability_topic"] = topicBase + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";

    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(deviceID);
    dev["name"] = deviceName;
    dev["manufacturer"] = "Sentinel DIY";
    dev["model"] = "Sentinel Multi-Sensor v2";
    dev["sw_version"] = "2.0.0";
    dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(configTopic.c_str(), payload, true);
    delay(50);
  }

  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String configTopic = String("homeassistant/binary_sensor/") + deviceID + "/ble_beacon_" + String(i + 1) + "/config";
    String valueTemplate = String("{{ value_json.beacon") + String(i + 1) + "_present }}";

    StaticJsonDocument<768> doc;
    doc["name"] = String("Sentinel ") + beaconDisplayName(i);
    doc["unique_id"] = deviceID + "_ble_beacon_" + String(i + 1);
    doc["state_topic"] = stateTopic;
    doc["value_template"] = valueTemplate;
    doc["payload_on"] = "home";
    doc["payload_off"] = "away";
    doc["device_class"] = "presence";
    doc["icon"] = "mdi:bluetooth-connect";
    doc["availability_topic"] = topicBase + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";

    JsonObject dev = doc.createNestedObject("device");
    JsonArray ids = dev.createNestedArray("identifiers");
    ids.add(deviceID);
    dev["name"] = deviceName;
    dev["manufacturer"] = "Sentinel DIY";
    dev["model"] = "Sentinel Multi-Sensor v2";
    dev["sw_version"] = "2.0.0";
    dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(configTopic.c_str(), payload, true);
    delay(50);
  }

  Serial.printf("[BLE] Published watchlist discovery for %u beacon slots\n", (unsigned)BLE_MAX_WATCH_BEACONS);
}

void publishBeaconConfigDiscovery() {
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String baseKey = String("ble_beacon") + String(i + 1);

    {
      String configTopic = String("homeassistant/text/") + deviceID + "/" + baseKey + "_name/config";
      String stateTopic = topicBase + "/config/" + baseKey + "_name/state";
      String commandTopic = topicBase + "/config/" + baseKey + "_name/set";

      StaticJsonDocument<768> doc;
      doc["name"] = String("Sentinel Beacon ") + String(i + 1) + " Name";
      doc["unique_id"] = deviceID + "_" + baseKey + "_name";
      doc["state_topic"] = stateTopic;
      doc["command_topic"] = commandTopic;
      doc["icon"] = "mdi:tag-text";
      doc["entity_category"] = "config";
      doc["availability_topic"] = topicBase + "/status";
      doc["payload_available"] = "online";
      doc["payload_not_available"] = "offline";

      JsonObject dev = doc.createNestedObject("device");
      JsonArray ids = dev.createNestedArray("identifiers");
      ids.add(deviceID);
      dev["name"] = deviceName;
      dev["manufacturer"] = "Sentinel DIY";
      dev["model"] = "Sentinel Multi-Sensor v2";
      dev["sw_version"] = "2.0.0";
      dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

      char payload[768];
      serializeJson(doc, payload, sizeof(payload));
      mqttClient.publish(configTopic.c_str(), payload, true);
      delay(50);
    }

    {
      String configTopic = String("homeassistant/text/") + deviceID + "/" + baseKey + "_uuid/config";
      String stateTopic = topicBase + "/config/" + baseKey + "_uuid/state";
      String commandTopic = topicBase + "/config/" + baseKey + "_uuid/set";

      StaticJsonDocument<768> doc;
      doc["name"] = String("Sentinel Beacon ") + String(i + 1) + " UUID";
      doc["unique_id"] = deviceID + "_" + baseKey + "_uuid";
      doc["state_topic"] = stateTopic;
      doc["command_topic"] = commandTopic;
      doc["icon"] = "mdi:bluetooth-settings";
      doc["entity_category"] = "config";
      doc["availability_topic"] = topicBase + "/status";
      doc["payload_available"] = "online";
      doc["payload_not_available"] = "offline";

      JsonObject dev = doc.createNestedObject("device");
      JsonArray ids = dev.createNestedArray("identifiers");
      ids.add(deviceID);
      dev["name"] = deviceName;
      dev["manufacturer"] = "Sentinel DIY";
      dev["model"] = "Sentinel Multi-Sensor v2";
      dev["sw_version"] = "2.0.0";
      dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

      char payload[768];
      serializeJson(doc, payload, sizeof(payload));
      mqttClient.publish(configTopic.c_str(), payload, true);
      delay(50);
    }

    {
      String configTopic = String("homeassistant/number/") + deviceID + "/" + baseKey + "_major/config";
      String stateTopic = topicBase + "/config/" + baseKey + "_major/state";
      String commandTopic = topicBase + "/config/" + baseKey + "_major/set";

      StaticJsonDocument<768> doc;
      doc["name"] = String("Sentinel Beacon ") + String(i + 1) + " Major";
      doc["unique_id"] = deviceID + "_" + baseKey + "_major";
      doc["state_topic"] = stateTopic;
      doc["command_topic"] = commandTopic;
      doc["min"] = 0;
      doc["max"] = 65535;
      doc["step"] = 1;
      doc["mode"] = "box";
      doc["icon"] = "mdi:numeric";
      doc["entity_category"] = "config";
      doc["availability_topic"] = topicBase + "/status";
      doc["payload_available"] = "online";
      doc["payload_not_available"] = "offline";

      JsonObject dev = doc.createNestedObject("device");
      JsonArray ids = dev.createNestedArray("identifiers");
      ids.add(deviceID);
      dev["name"] = deviceName;
      dev["manufacturer"] = "Sentinel DIY";
      dev["model"] = "Sentinel Multi-Sensor v2";
      dev["sw_version"] = "2.0.0";
      dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

      char payload[768];
      serializeJson(doc, payload, sizeof(payload));
      mqttClient.publish(configTopic.c_str(), payload, true);
      delay(50);
    }

    {
      String configTopic = String("homeassistant/number/") + deviceID + "/" + baseKey + "_minor/config";
      String stateTopic = topicBase + "/config/" + baseKey + "_minor/state";
      String commandTopic = topicBase + "/config/" + baseKey + "_minor/set";

      StaticJsonDocument<768> doc;
      doc["name"] = String("Sentinel Beacon ") + String(i + 1) + " Minor";
      doc["unique_id"] = deviceID + "_" + baseKey + "_minor";
      doc["state_topic"] = stateTopic;
      doc["command_topic"] = commandTopic;
      doc["min"] = 0;
      doc["max"] = 65535;
      doc["step"] = 1;
      doc["mode"] = "box";
      doc["icon"] = "mdi:numeric";
      doc["entity_category"] = "config";
      doc["availability_topic"] = topicBase + "/status";
      doc["payload_available"] = "online";
      doc["payload_not_available"] = "offline";

      JsonObject dev = doc.createNestedObject("device");
      JsonArray ids = dev.createNestedArray("identifiers");
      ids.add(deviceID);
      dev["name"] = deviceName;
      dev["manufacturer"] = "Sentinel DIY";
      dev["model"] = "Sentinel Multi-Sensor v2";
      dev["sw_version"] = "2.0.0";
      dev["connections"].to<JsonArray>().add(serialized("[\"mac\",\"" + deviceMAC + "\"]"));

      char payload[768];
      serializeJson(doc, payload, sizeof(payload));
      mqttClient.publish(configTopic.c_str(), payload, true);
      delay(50);
    }
  }

  Serial.printf("[BLE] Published %u beacon config entities\n", (unsigned)(BLE_MAX_WATCH_BEACONS * 4));
}

void publishBeaconConfigState() {
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String baseKey = String("ble_beacon") + String(i + 1);
    String nameTopic = topicBase + "/config/" + baseKey + "_name/state";
    String uuidTopic = topicBase + "/config/" + baseKey + "_uuid/state";
    String majorTopic = topicBase + "/config/" + baseKey + "_major/state";
    String minorTopic = topicBase + "/config/" + baseKey + "_minor/state";

    mqttPublish(nameTopic.c_str(), beaconDisplayName(i).c_str(), true);
    mqttPublish(uuidTopic.c_str(), runtimeConfig.bleBeaconUuids[i].c_str(), true);
    mqttPublish(majorTopic.c_str(), String(runtimeConfig.bleBeaconMajors[i]).c_str(), true);
    mqttPublish(minorTopic.c_str(), String(runtimeConfig.bleBeaconMinors[i]).c_str(), true);
  }
}

void publishBeaconState(bool initialPublish) {
  size_t configuredCount = 0;
  size_t presentCount = 0;
  StaticJsonDocument<768> doc;

  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    String prefix = String("beacon") + String(i + 1);
    bool configured = isBeaconConfigured(i);
    String presentState = configured ? (beaconPresent[i] ? "home" : "away") : "unconfigured";

    doc[prefix + "_name"] = beaconDisplayName(i);
    doc[prefix + "_uuid"] = runtimeConfig.bleBeaconUuids[i];
    doc[prefix + "_major"] = runtimeConfig.bleBeaconMajors[i];
    doc[prefix + "_minor"] = runtimeConfig.bleBeaconMinors[i];
    doc[prefix + "_present"] = presentState;
    doc[prefix + "_rssi"] = beaconPresent[i] ? beaconRssi[i] : 0;

    if (configured) {
      configuredCount++;
      if (beaconPresent[i]) {
        presentCount++;
      }
    }
  }

  doc["configured_count"] = configuredCount;
  doc["present_count"] = presentCount;
  doc["timestamp"] = currentTimestampSeconds();

  char payload[768];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/beacons";
  mqttPublish(topic.c_str(), payload, true);

  if (initialPublish) {
    Serial.printf("[BLE] Initial beacon watchlist publish — %u configured, %u present\n",
                  (unsigned)configuredCount, (unsigned)presentCount);
  } else {
    Serial.printf("[BLE] Beacon watchlist updated — %u configured, %u present\n",
                  (unsigned)configuredCount, (unsigned)presentCount);
  }
}

void publishSoundState(const char* intrusionState, int peakToPeak, int minSample, int maxSample) {
  StaticJsonDocument<128> doc;
  doc["intrusion"] = intrusionState;
  doc["peak_adc"] = peakToPeak;
  doc["peak_to_peak_adc"] = peakToPeak;
  doc["min_adc"] = minSample;
  doc["max_adc"] = maxSample;
  doc["threshold_adc"] = runtimeConfig.soundThresholdAdc;
  doc["timestamp"] = currentTimestampSeconds();

  char payload[128];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/sound";
  mqttPublish(topic.c_str(), payload);
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
  pirWarmupUntil = millis() + runtimeConfig.pirWarmupMs;
  pirHighSince = 0;
  pirLowSince = 0;
  pirHoldUntil = 0;
  pirLastRawState = digitalRead(PIN_PIR);
  motionDetected = false;
  Serial.println(F("[PIR] Motion sensor initialized on GPIO 27"));
}

void checkMotion() {
  unsigned long now = millis();
  int pirState = digitalRead(PIN_PIR);

  if (now < pirWarmupUntil) {
    pirLastRawState = pirState;
    pirHighSince = 0;
    pirLowSince = 0;
    motionDetected = false;
    return;
  }

  if (pirState != pirLastRawState) {
    pirLastRawState = pirState;
    if (pirState == HIGH) {
      pirHighSince = now;
      pirLowSince = 0;
    } else {
      pirLowSince = now;
      pirHighSince = 0;
    }
  }

  if (pirState == HIGH) {
    pirHoldUntil = now + runtimeConfig.pirHoldMs;

    if (!motionDetected &&
        pirHighSince > 0 &&
      (now - pirHighSince) >= runtimeConfig.pirDetectStableMs &&
        (now - lastMotionPub) >= PIR_COOLDOWN) {
      motionDetected = true;
      lastMotionPub = now;

      StaticJsonDocument<64> doc;
      doc["motion"] = "detected";
      doc["timestamp"] = currentTimestampSeconds();

      char payload[64];
      serializeJson(doc, payload, sizeof(payload));

      String topic = topicBase + "/motion";
      mqttPublish(topic.c_str(), payload);

      Serial.println(F("[PIR] Motion DETECTED"));
      blinkLED(1, 50);
    }
    return;
  }

  if (motionDetected &&
      pirLowSince > 0 &&
      now >= pirHoldUntil &&
      (now - pirLowSince) >= runtimeConfig.pirClearStableMs) {
    motionDetected = false;

    StaticJsonDocument<64> doc;
    doc["motion"] = "clear";
    doc["timestamp"] = currentTimestampSeconds();

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
  unsigned long now = millis();

  if (soundIntrusionActive && now >= soundHoldUntil) {
    soundIntrusionActive = false;
    publishSoundState("clear", 0, 0, 0);
    Serial.println(F("[SOUND] Intrusion CLEARED"));
  }

  unsigned long startMillis = millis();
  int minSample = 4095;
  int maxSample = 0;

  while (millis() - startMillis < runtimeConfig.soundSampleWindowMs) {
    int sample = analogRead(PIN_SOUND);
    if (sample < minSample) {
      minSample = sample;
    }
    if (sample > maxSample) {
      maxSample = sample;
    }
  }

  int peakToPeak = maxSample - minSample;
  if (peakToPeak >= (int)runtimeConfig.soundThresholdAdc) {
    soundHoldUntil = now + runtimeConfig.soundHoldMs;
    if (!soundIntrusionActive) {
      soundIntrusionActive = true;
      publishSoundState("detected", peakToPeak, minSample, maxSample);
      Serial.printf("[SOUND] Intrusion detected — p2p ADC: %d (min=%d max=%d threshold=%lu)\n",
                    peakToPeak, minSample, maxSample, runtimeConfig.soundThresholdAdc);
      blinkLED(2, 50);
    }
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
  doc["timestamp"] = currentTimestampSeconds();

  char payload[96];
  serializeJson(doc, payload, sizeof(payload));

  String topic = topicBase + "/door";
  mqttPublish(topic.c_str(), payload, true);  // retained

  Serial.printf("[DOOR] State changed → %s\n", doorStr);
  blinkLED(1, 100);
}

// ============================================================================
//  BLE — Beacon Watchlist Scanning
// ============================================================================

class SentinelBLECallbacks : public BLEAdvertisedDeviceCallbacks {
  public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
      processBleScanResult(advertisedDevice);
    }
};

void setupBLE() {
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    beaconPresent[i] = false;
    beaconRssi[i] = 0;
    beaconLastSeenMs[i] = 0;
  }

  if (!pBLEScan) {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
  }

  if (pBLEScan) {
    pBLEScan->setActiveScan(false);
    pBLEScan->setInterval(160);
    pBLEScan->setWindow(80);
  }

  Serial.println(F("[BLE] Watchlist scanner initialized"));
}

void processBleScanResult(BLEAdvertisedDevice advertisedDevice) {
  String uuid;
  uint16_t major = 0;
  uint16_t minor = 0;

  if (!parseIBeaconAdvertisement(advertisedDevice, uuid, major, minor)) {
    return;
  }

  logIBeaconSighting(uuid, major, minor, advertisedDevice.getRSSI());
  processBeaconAdvertisement(uuid, major, minor, advertisedDevice.getRSSI());
}

bool parseIBeaconAdvertisement(BLEAdvertisedDevice advertisedDevice, String& uuid, uint16_t& major, uint16_t& minor) {
  std::string manufacturerData = advertisedDevice.getManufacturerData();
  if (manufacturerData.length() < 25) {
    return false;
  }

  const uint8_t* data = (const uint8_t*)manufacturerData.data();
  if (data[0] != 0x4C || data[1] != 0x00 || data[2] != 0x02 || data[3] != 0x15) {
    return false;
  }

  char uuidBuffer[37];
  snprintf(uuidBuffer, sizeof(uuidBuffer),
           "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
           data[4], data[5], data[6], data[7],
           data[8], data[9],
           data[10], data[11],
           data[12], data[13],
           data[14], data[15], data[16], data[17], data[18], data[19]);
  uuid = String(uuidBuffer);
  major = ((uint16_t)data[20] << 8) | data[21];
  minor = ((uint16_t)data[22] << 8) | data[23];
  return true;
}

void logIBeaconSighting(const String& uuid, uint16_t major, uint16_t minor, int rssi) {
  if (!bleEnrollmentLoggingEnabled) {
    return;
  }

  String sightingId = uuid + "/" + String(major) + "/" + String(minor);
  unsigned long nowMs = millis();
  size_t replacementIndex = 0;
  unsigned long oldestSeenMs = bleEnrollmentSeenMs[0];

  for (size_t i = 0; i < BLE_ENROLLMENT_CACHE_SIZE; i++) {
    if (bleEnrollmentSeenIds[i] == sightingId) {
      if ((nowMs - bleEnrollmentSeenMs[i]) < BLE_ENROLLMENT_LOG_COOLDOWN_MS) {
        return;
      }
      bleEnrollmentSeenMs[i] = nowMs;
      Serial.printf("[BLE][ENROLL] UUID:%s Major:%u Minor:%u RSSI:%d\n",
                    uuid.c_str(), (unsigned)major, (unsigned)minor, rssi);
      return;
    }

    if (bleEnrollmentSeenIds[i].length() == 0) {
      bleEnrollmentSeenIds[i] = sightingId;
      bleEnrollmentSeenMs[i] = nowMs;
      Serial.printf("[BLE][ENROLL] UUID:%s Major:%u Minor:%u RSSI:%d\n",
                    uuid.c_str(), (unsigned)major, (unsigned)minor, rssi);
      return;
    }

    if (bleEnrollmentSeenMs[i] < oldestSeenMs) {
      oldestSeenMs = bleEnrollmentSeenMs[i];
      replacementIndex = i;
    }
  }

  bleEnrollmentSeenIds[replacementIndex] = sightingId;
  bleEnrollmentSeenMs[replacementIndex] = nowMs;
  Serial.printf("[BLE][ENROLL] UUID:%s Major:%u Minor:%u RSSI:%d\n",
                uuid.c_str(), (unsigned)major, (unsigned)minor, rssi);
}

void processBeaconAdvertisement(const String& uuid, uint16_t major, uint16_t minor, int rssi) {
  unsigned long nowMs = millis();

  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    if (!isBeaconConfigured(i)) {
      continue;
    }

    if (runtimeConfig.bleBeaconUuids[i].equalsIgnoreCase(uuid) &&
        runtimeConfig.bleBeaconMajors[i] == major &&
        runtimeConfig.bleBeaconMinors[i] == minor) {
      bool wasPresent = beaconPresent[i];
      beaconPresent[i] = true;
      beaconRssi[i] = rssi;
      beaconLastSeenMs[i] = nowMs;

      if (!wasPresent) {
        Serial.printf("[BLE] Beacon %s arrived (%s %u/%u) RSSI:%d\n",
                      beaconDisplayName(i).c_str(), uuid.c_str(), (unsigned)major, (unsigned)minor, rssi);
      }
      return;
    }
  }
}

void expireBeaconPresence(unsigned long nowMs) {
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    if (!isBeaconConfigured(i)) {
      beaconPresent[i] = false;
      beaconRssi[i] = 0;
      beaconLastSeenMs[i] = 0;
      continue;
    }

    if (beaconPresent[i] && (nowMs - beaconLastSeenMs[i]) > BLE_PRESENCE_TIMEOUT_MS) {
      beaconPresent[i] = false;
      beaconRssi[i] = 0;
      Serial.printf("[BLE] Beacon %s left (%s %lu/%lu)\n",
                    beaconDisplayName(i).c_str(), runtimeConfig.bleBeaconUuids[i].c_str(),
                    runtimeConfig.bleBeaconMajors[i], runtimeConfig.bleBeaconMinors[i]);
    }
  }
}

void runBLEScan() {
  if (!pBLEScan) return;

  bool anyConfigured = false;
  for (size_t i = 0; i < BLE_MAX_WATCH_BEACONS; i++) {
    if (isBeaconConfigured(i)) {
      anyConfigured = true;
      break;
    }
  }

  if (!anyConfigured) {
    return;
  }

  Serial.println(F("[BLE] Starting beacon watchlist scan..."));

  SentinelBLECallbacks callbacks;
  pBLEScan->setAdvertisedDeviceCallbacks(&callbacks, false);
  BLEScanResults results = pBLEScan->start(BLE_SCAN_DURATION_MS / 1000, false);
  (void)results;
  pBLEScan->clearResults();

  expireBeaconPresence(millis());
  publishBeaconState(false);
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
