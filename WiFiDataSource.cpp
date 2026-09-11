#include "WiFiDataSource.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>

#include "WifiCredentials.h"
#include "MqttCredentials.h"

WiFiDataSource::WiFiDataSource()
    : connected_(false), lastAttemptMs_(0), localIp_{0} {
  setLocalIp("0.0.0.0");
}

void WiFiDataSource::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  startConnection(millis());
}

void WiFiDataSource::poll(uint32_t nowMs) {
  const bool isConnected = WiFi.status() == WL_CONNECTED;

  if (isConnected) {
    if (!connected_) {
      const String ip = WiFi.localIP().toString();
      setLocalIp(ip.c_str());
      Serial.print("Wi-Fi connected, IP: ");
      Serial.println(localIp_);
    }
    connected_ = true;
    return;
  }

  if (connected_) {
    Serial.println("Wi-Fi disconnected");
    setLocalIp("0.0.0.0");
  }
  connected_ = false;

  if (nowMs - lastAttemptMs_ >= kReconnectIntervalMs) {
    startConnection(nowMs);
  }
}

NetworkStatus WiFiDataSource::readNetwork() const {
  return {
      connected_,
      WifiCredentials::kSsid,
      localIp_,
      MqttCredentials::kServer,
  };
}

void WiFiDataSource::startConnection(uint32_t nowMs) {
  lastAttemptMs_ = nowMs;
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WifiCredentials::kSsid);
  WiFi.begin(WifiCredentials::kSsid, WifiCredentials::kPassword);
}

void WiFiDataSource::setLocalIp(const char* value) {
  snprintf(localIp_, sizeof(localIp_), "%s", value);
}
