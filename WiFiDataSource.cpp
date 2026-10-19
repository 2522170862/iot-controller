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
  const String ip = WiFi.localIP().toString();
  const bool isConnected = status == WL_CONNECTED &&
                           WiFi.SSID() == currentCredentials_.ssid &&
                           ip != "0.0.0.0";

  if (isConnected) {
    if (!connected_) {
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

  // Arduino-ESP32 can retain the previous attempt's failed status after begin().
  // Only a matching network + IP or this attempt's deadline ends validation.
  // Do not restart at the normal 10 s reconnect interval during this window.
  if (attemptState_ == WifiAttemptState::kConnecting) {
    if (nowMs - lastAttemptMs_ < kAttemptTimeoutMs) {
      return;
    }
    attemptState_ = WifiAttemptState::kFailed;
    lastAttemptMs_ = nowMs;
    Serial.println("Wi-Fi connection attempt timed out");
    return;
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
  connected_ = false;
  setLocalIp("0.0.0.0");
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
