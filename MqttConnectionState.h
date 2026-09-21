#pragma once

#include <stdint.h>

enum class MqttConnectionEvent : uint8_t {
  kNone,
  kConnected,
  kDisconnected,
};

class MqttConnectionState {
 public:
  static constexpr uint32_t kReconnectIntervalMs = 5000;

  bool shouldAttempt(uint32_t nowMs, bool wifiConnected) const {
    return wifiConnected && (!attempted_ || nowMs - lastAttemptMs_ >= kReconnectIntervalMs);
  }
  void recordAttempt(uint32_t nowMs) { lastAttemptMs_ = nowMs; attempted_ = true; }
  void reset() { attempted_ = false; }
  constexpr MqttConnectionEvent observeConnection(bool connected) {
    if (!connectionObserved_) {
      connectionObserved_ = true;
      lastConnected_ = connected;
      return connected ? MqttConnectionEvent::kConnected
                       : MqttConnectionEvent::kNone;
    }
    if (connected == lastConnected_) {
      return MqttConnectionEvent::kNone;
    }
    lastConnected_ = connected;
    return connected ? MqttConnectionEvent::kConnected
                     : MqttConnectionEvent::kDisconnected;
  }

 private:
  uint32_t lastAttemptMs_ = 0;
  bool attempted_ = false;
  bool connectionObserved_ = false;
  bool lastConnected_ = false;
};
