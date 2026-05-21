#pragma once

// Local credentials and broker settings for this device.
// This file is ignored by git via .gitignore.

const char* WIFI_SSID     = "Proverbs-DECO";
const char* WIFI_PASSWORD = "%%Hartmann94##";

// Set to true to use fixed network settings below, false for DHCP.
const bool  WIFI_USE_STATIC_IP = false;
const char* WIFI_STATIC_IP = "192.168.1.50";
const char* WIFI_GATEWAY   = "192.168.1.1";
const char* WIFI_SUBNET    = "255.255.255.0";
const char* WIFI_DNS1      = "1.1.1.1";
const char* WIFI_DNS2      = "8.8.8.8";

// Time sync (set to false to disable NTP and keep uptime-based timestamps).
const bool  TIME_USE_NTP   = true;
const char* TZ_INFO        = "UTC0";
// Local time source (Home Assistant + Crony)
const char* NTP_SERVER_1   = "10.0.1.20";
const char* NTP_SERVER_2   = "";
const char* NTP_SERVER_3   = "";

const char* MQTT_SERVER   = "10.0.1.20";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "mqtt_user";
const char* MQTT_PASS     = "Fre%%Clip$$12Yj";
