#pragma once

#include <stdint.h>

class MqttConnectionState {
 public:
  static constexpr uint32_t kReconnectIntervalMs = 5000;

  bool shouldAttempt(uint32_t nowMs, bool wifiConnected) const {
    return wifiConnected && (!attempted_ || nowMs - lastAttemptMs_ >= kReconnectIntervalMs);
  }
  void recordAttempt(uint32_t nowMs) { lastAttemptMs_ = nowMs; attempted_ = true; }
  void reset() { attempted_ = false; }

 private:
  uint32_t lastAttemptMs_ = 0;
  bool attempted_ = false;
};
