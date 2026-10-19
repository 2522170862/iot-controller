#pragma once
#include "Arduino.h"
enum wl_status_t { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_CONNECTED,
                   WL_CONNECT_FAILED, WL_DISCONNECTED };
constexpr int WIFI_STA = 1;
struct TestIp {
  String value;
  String toString() const { return value; }
};
struct TestWifi {
  wl_status_t rawStatus = WL_DISCONNECTED;
  String actualSsid;
  String ip = "0.0.0.0";
  int beginCount = 0;
  void persistent(bool) {}
  void mode(int) {}
  void setAutoReconnect(bool) {}
  // ESP32 core 3.3.3 keeps the old status until asynchronous Wi-Fi events.
  void disconnect(bool, bool) {}
  void begin(const char*, const char*) { ++beginCount; }
  wl_status_t status() const { return rawStatus; }
  String SSID() const { return actualSsid; }
  TestIp localIP() const { return {ip}; }
};
inline TestWifi WiFi;
