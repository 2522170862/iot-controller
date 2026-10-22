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
  connect(credentials, millis());
}

void WiFiDataSource::poll(uint32_t nowMs) {
  const wl_status_t status = WiFi.status();
  const String ip = WiFi.localIP().toString();
  const bool attemptCanSucceed = attemptState_ == WifiAttemptState::kConnecting ||
                                 attemptState_ == WifiAttemptState::kConnected;
  const bool isConnected = attemptCanSucceed && status == WL_CONNECTED &&
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
  connected_ = false;
  setLocalIp("0.0.0.0");
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(currentCredentials_.ssid);

  // In core 3.3.3, disconnect(false, false) returns without cancelling an
  // in-flight association when STA is not connected yet. Stop the Wi-Fi driver
  // before changing credentials; this does not stop the BLE controller.
  // Application-level poll() owns periodic retries, not SDK auto-reconnect.
  WiFi.setAutoReconnect(false);
  if (!WiFi.mode(WIFI_OFF)) {
    attemptState_ = WifiAttemptState::kFailed;
    Serial.println("[Wi-Fi] station stop failed; configuration not changed");
    return;
  }
  if (!WiFi.STA.begin()) {
    attemptState_ = WifiAttemptState::kFailed;
    Serial.println("[Wi-Fi] station start failed");
    return;
  }
  // STA.connect() reports whether config/connect was actually accepted.
  // Legacy WiFi.begin() instead returns a possibly stale connection status.
  if (!WiFi.STA.connect(currentCredentials_.ssid, currentCredentials_.password)) {
    attemptState_ = WifiAttemptState::kFailed;
    Serial.println("[Wi-Fi] driver rejected new connection request");
    return;
  }
  Serial.println("[Wi-Fi] connection request accepted; waiting for IP");
}

void WiFiDataSource::setLocalIp(const char* value) {
  snprintf(localIp_, sizeof(localIp_), "%s", value);
}
