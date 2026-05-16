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
#define INTERVAL_BLE        120000  // BLE scan every 2 minutes
#define BLE_SCAN_DURATION_MS 10000  // each BLE scan runs for 10s
#define BLE_DEVICE_TIMEOUT_MS 150000 // remove device if unseen for about 2.5 minutes
#define BLE_MAX_TRACKED_DEVICES 24   // cap tracked BLE devices to protect RAM
#define SOUND_SAMPLE_WINDOW 100     // sound sampling window (ms)
#define SOUND_THRESHOLD     1500    // Calibrated for MAX9814
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

const uint16_t CONFIG_VERSION = 1;
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
unsigned long pirWarmupUntil = 0;
unsigned long pirHighSince   = 0;
unsigned long pirLowSince    = 0;
unsigned long pirHoldUntil   = 0;
int          pirLastRawState = LOW;

struct BleTrackedDevice {
  bool active = false;
  String address;
  int rssi = 0;
  unsigned long firstSeenMs = 0;
  unsigned long lastSeenMs = 0;
  bool seenThisScan = false;
};

BleTrackedDevice bleDevices[BLE_MAX_TRACKED_DEVICES];
size_t bleActiveCount = 0;
size_t bleNewCount = 0;
size_t bleExpiredCount = 0;

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
void mqttMessageCallback(char* topic, byte* payload, unsigned int length);
bool handlePirConfigCommand(const String& topic, const String& payload);

void readAndPublishEnvironment();
void publishDoorState(bool state);
void checkMotion();
void checkSound();
void checkDoor();
void runBLEScan();
void resetBleScanMarks();
int  findBleDeviceSlot(const String& address);
int  allocateBleDeviceSlot();
void processBleScanResult(BLEAdvertisedDevice advertisedDevice);
void expireBleDevices(unsigned long nowMs);
String buildBleActiveDeviceListJson();
String buildBleActiveAddressCsv();
String buildBleActiveSummary();
void publishBlePresence(bool initialPublish = false);
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
    publishAutoDiscovery();
    publishPirConfigDiscovery();
    publishPirConfigState();
    publishBlePresence(true);
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
      cfg.pirHoldMs < 250 || cfg.pirHoldMs > 60000) {
    return false;
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
  } else {
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
  reconnectWiFi();
  reconnectMQTT();
  Serial.println(F("[CONFIG] Applied runtime config to active services"));
}

void processSerialCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

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
    mqttClient.subscribe(pirWarmupSetTopic.c_str());
    mqttClient.subscribe(pirDetectSetTopic.c_str());
    mqttClient.subscribe(pirClearSetTopic.c_str());
    mqttClient.subscribe(pirHoldSetTopic.c_str());
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
      stateTopicBLE.c_str(), "{{ value_json.active_count }}", nullptr, nullptr, "mdi:bluetooth", nullptr, nullptr},
        {"sensor", "ble_active_devices", "BLE Active Devices",
      stateTopicBLE.c_str(), "{{ value_json.active_device_summary }}", nullptr, nullptr, "mdi:bluetooth-connect", nullptr, nullptr},

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
    doc["timestamp"]  = currentTimestampSeconds();

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
  doc["timestamp"] = currentTimestampSeconds();

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
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
      processBleScanResult(advertisedDevice);
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

void resetBleScanMarks() {
  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    bleDevices[i].seenThisScan = false;
  }
  bleNewCount = 0;
  bleExpiredCount = 0;
}

int findBleDeviceSlot(const String& address) {
  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (bleDevices[i].active && bleDevices[i].address.equalsIgnoreCase(address)) {
      return (int)i;
    }
  }
  return -1;
}

int allocateBleDeviceSlot() {
  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (!bleDevices[i].active) {
      return (int)i;
    }
  }
  return -1;
}

void processBleScanResult(BLEAdvertisedDevice advertisedDevice) {
  String address = String(advertisedDevice.getAddress().toString().c_str());
  int rssi = advertisedDevice.getRSSI();
  unsigned long nowMs = millis();

  int slot = findBleDeviceSlot(address);
  bool isNewDevice = false;

  if (slot < 0) {
    slot = allocateBleDeviceSlot();
    if (slot < 0) {
      return; // table full; drop extra devices rather than fragment memory
    }
    bleDevices[slot].active = true;
    bleDevices[slot].address = address;
    bleDevices[slot].firstSeenMs = nowMs;
    isNewDevice = true;
    bleNewCount++;
  }

  bleDevices[slot].rssi = rssi;
  bleDevices[slot].lastSeenMs = nowMs;
  bleDevices[slot].seenThisScan = true;

  if (isNewDevice) {
    Serial.printf("[BLE] Present: %s RSSI:%d\n", address.c_str(), rssi);
  }
}

void expireBleDevices(unsigned long nowMs) {
  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (!bleDevices[i].active) {
      continue;
    }

    if ((nowMs - bleDevices[i].lastSeenMs) > BLE_DEVICE_TIMEOUT_MS) {
      Serial.printf("[BLE] Left: %s\n", bleDevices[i].address.c_str());
      bleDevices[i].active = false;
      bleDevices[i].address = "";
      bleDevices[i].rssi = 0;
      bleDevices[i].firstSeenMs = 0;
      bleDevices[i].lastSeenMs = 0;
      bleDevices[i].seenThisScan = false;
      bleExpiredCount++;
    }
  }
}

String buildBleActiveDeviceListJson() {
  String list = "[";
  bool first = true;

  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (!bleDevices[i].active) {
      continue;
    }

    if (!first) {
      list += ",";
    }
    first = false;

    list += "{\"address\":\"" + bleDevices[i].address +
            "\",\"rssi\":" + String(bleDevices[i].rssi) +
            ",\"first_seen_ms\":" + String(bleDevices[i].firstSeenMs) +
            ",\"last_seen_ms\":" + String(bleDevices[i].lastSeenMs) + "}";
  }

  list += "]";
  return list;
}

String buildBleActiveAddressCsv() {
  String addresses;
  bool first = true;

  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (!bleDevices[i].active) {
      continue;
    }

    if (!first) {
      addresses += ", ";
    }
    first = false;
    addresses += bleDevices[i].address;
  }

  return addresses;
}

String buildBleActiveSummary() {
  String summary;
  size_t shown = 0;

  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (!bleDevices[i].active) {
      continue;
    }

    String candidate = summary;
    if (shown > 0) {
      candidate += ", ";
    }
    candidate += bleDevices[i].address;

    if (candidate.length() > 180) {
      break;
    }

    summary = candidate;
    shown++;
  }

  if (bleActiveCount == 0) {
    return "none";
  }

  if (shown < bleActiveCount) {
    summary += " +" + String(bleActiveCount - shown) + " more";
  }

  return summary;
}

void publishBlePresence(bool initialPublish) {
  bleActiveCount = 0;
  for (size_t i = 0; i < BLE_MAX_TRACKED_DEVICES; i++) {
    if (bleDevices[i].active) {
      bleActiveCount++;
    }
  }

  String activeList = buildBleActiveDeviceListJson();
  String activeCsv = buildBleActiveAddressCsv();
  String activeSummary = buildBleActiveSummary();
  String payload = String("{\"active_count\":") + String(bleActiveCount) +
                   ",\"active_devices\":" + activeList +
                   ",\"active_device_addresses\":\"" + activeCsv + "\"" +
                   ",\"active_device_summary\":\"" + activeSummary + "\"" +
                   ",\"new_devices\":" + String(bleNewCount) +
                   ",\"expired_devices\":" + String(bleExpiredCount) +
                   ",\"scan_duration_sec\":" + String(BLE_SCAN_DURATION_MS / 1000) +
                   ",\"timestamp\":" + String(currentTimestampSeconds()) + "}";

  String topic = topicBase + "/ble";
  mqttPublish(topic.c_str(), payload.c_str(), true);

  String listTopic = topicBase + "/ble/list";
  String listPayload = String("{\"active_count\":") + String(bleActiveCount) +
                       ",\"devices\":" + activeList +
                       ",\"timestamp\":" + String(currentTimestampSeconds()) + "}";
  mqttPublish(listTopic.c_str(), listPayload.c_str(), true);

  if (initialPublish) {
    Serial.printf("[BLE] Initial presence publish — %u devices active\n", (unsigned)bleActiveCount);
  } else {
    Serial.printf("[BLE] Presence updated — %u active, %u new, %u expired\n",
                  (unsigned)bleActiveCount, (unsigned)bleNewCount, (unsigned)bleExpiredCount);
  }
}

void runBLEScan() {
  if (!pBLEScan) return;

  Serial.println(F("[BLE] Starting scan..."));

  SentinelBLECallbacks callbacks;
  resetBleScanMarks();
  pBLEScan->setAdvertisedDeviceCallbacks(&callbacks, false);
  BLEScanResults results = pBLEScan->start(BLE_SCAN_DURATION_MS / 1000, false);  // 10-second scan
  pBLEScan->clearResults();

  expireBleDevices(millis());

  publishBlePresence(false);
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
