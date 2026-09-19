#include "WiFiDataSource.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>

#include "WifiCredentials.h"
#include "MqttCredentials.h"

WiFiDataSource::WiFiDataSource()
    : connected_(false),
      lastAttemptMs_(0),
      localIp_{0},
      currentCredentials_{},
      attemptState_(WifiAttemptState::kIdle) {
  setLocalIp("0.0.0.0");
}

void WiFiDataSource::begin() {
  WifiCredentialsValue fallback = {};
  snprintf(fallback.ssid, sizeof(fallback.ssid), "%s", WifiCredentials::kSsid);
  snprintf(fallback.password, sizeof(fallback.password), "%s",
           WifiCredentials::kPassword);
  begin(fallback);
}

void WiFiDataSource::begin(const WifiCredentialsValue& credentials) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  connect(credentials, millis());
}

void WiFiDataSource::poll(uint32_t nowMs) {
  const wl_status_t status = WiFi.status();
  const bool isConnected = status == WL_CONNECTED;

  if (isConnected) {
    if (!connected_) {
      const String ip = WiFi.localIP().toString();
      setLocalIp(ip.c_str());
      Serial.print("Wi-Fi connected, IP: ");
      Serial.println(localIp_);
    }
    connected_ = true;
    attemptState_ = WifiAttemptState::kConnected;
    return;
  }

  if (connected_) {
    Serial.println("Wi-Fi disconnected");
    setLocalIp("0.0.0.0");
  }
  connected_ = false;

  if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
    attemptState_ = WifiAttemptState::kFailed;
  }

  if (nowMs - lastAttemptMs_ >= kReconnectIntervalMs) {
    startConnection(nowMs);
  }
}

NetworkStatus WiFiDataSource::readNetwork() const {
  return {
      connected_,
      currentCredentials_.ssid,
      localIp_,
      MqttCredentials::kServer,
  };
}

void WiFiDataSource::connect(const WifiCredentialsValue& credentials,
                            uint32_t nowMs) {
  currentCredentials_ = credentials;
  WiFi.disconnect(false, false);
  startConnection(nowMs);
}

WifiCredentialsValue WiFiDataSource::currentCredentials() const {
  return currentCredentials_;
}

WifiAttemptState WiFiDataSource::attemptState() const {
  return attemptState_;
}

const char* WiFiDataSource::localIp() const { return localIp_; }

void WiFiDataSource::startConnection(uint32_t nowMs) {
  lastAttemptMs_ = nowMs;
  attemptState_ = WifiAttemptState::kConnecting;
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(currentCredentials_.ssid);
  WiFi.begin(currentCredentials_.ssid, currentCredentials_.password);
}

void WiFiDataSource::setLocalIp(const char* value) {
  snprintf(localIp_, sizeof(localIp_), "%s", value);
}
