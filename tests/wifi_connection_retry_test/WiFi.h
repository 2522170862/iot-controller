#pragma once
#include "Arduino.h"
#include <vector>
enum wl_status_t { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_CONNECTED,
                   WL_CONNECT_FAILED, WL_DISCONNECTED };
constexpr int WIFI_STA = 1;
constexpr int WIFI_OFF = 0;
struct TestSta {
  bool begin();
  bool connect(const char* ssid, const char* password);
};
struct TestIp {
  String value;
  String toString() const { return value; }
};
struct TestWifi {
  wl_status_t rawStatus = WL_DISCONNECTED;
  String actualSsid;
  String ip = "0.0.0.0";
  int beginCount = 0;
  bool connecting = false;
  bool driverStarted = false;
  bool autoReconnect = true;
  bool stopResult = true;
  bool startResult = true;
  bool connectResult = true;
  int rejectedConfigurations = 0;
  String configuredSsid;
  String configuredPassword;
  std::vector<int> modes;
  TestSta STA;
  void persistent(bool) {}
  bool mode(int value) {
    modes.push_back(value);
    if (value == WIFI_OFF) {
      if (!stopResult) return false;
      connecting = false;
      driverStarted = false;
      actualSsid.clear();
      ip = "0.0.0.0";
    } else driverStarted = true;
    return true;
  }
  void setAutoReconnect(bool value) { autoReconnect = value; }
  // ESP32 core 3.3.3 keeps the old status until asynchronous Wi-Fi events.
  void disconnect(bool, bool) {}
  wl_status_t begin(const char* ssid, const char* password) {
    if (!driverStarted) driverStarted = startResult;
    return STA.connect(ssid, password) ? rawStatus : WL_CONNECT_FAILED;
  }
  wl_status_t status() const { return rawStatus; }
  String SSID() const { return actualSsid; }
  TestIp localIP() const { return {ip}; }
};
inline TestWifi WiFi;
inline bool TestSta::begin() {
  if (!WiFi.startResult) return false;
  WiFi.driverStarted = true;
  return true;
}
inline bool TestSta::connect(const char* ssid, const char* password) {
  ++WiFi.beginCount;
  // Mirrors ESP32's 'sta is connecting, cannot set config' failure boundary.
  if (WiFi.connecting || !WiFi.driverStarted || !WiFi.connectResult) {
    ++WiFi.rejectedConfigurations;
    return false;
  }
  WiFi.configuredSsid = ssid;
  WiFi.configuredPassword = password;
  WiFi.connecting = true;
  return true;
}
