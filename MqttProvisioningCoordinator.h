#pragma once

#include "BleProvisioningProtocol.h"
#include "MqttConfiguration.h"

enum class MqttProvisioningEventKind : uint8_t { kConnecting, kConnected, kFailed };
struct MqttProvisioningEvent {
  MqttProvisioningEventKind kind = MqttProvisioningEventKind::kFailed;
  char id[9] = {};
  char reason[24] = {};
};

class MqttProvisioningCoordinator {
 public:
  static constexpr uint32_t kValidationTimeoutMs = 15000;
  MqttProvisioningCoordinator(IMqttConfigurationConnection* connection, IMqttConfigurationStore* store);
  bool start(const BleProvisioningRequest& request, uint32_t nowMs);
  void poll(uint32_t nowMs);
  bool active() const { return active_; }
  bool takeEvent(MqttProvisioningEvent* output);

 private:
  IMqttConfigurationConnection* connection_;
  IMqttConfigurationStore* store_;
  bool active_ = false;
  uint32_t startedMs_ = 0;
  char id_[9] = {};
  MqttConfiguration previous_;
  MqttProvisioningEvent events_[2] = {};
  uint8_t eventCount_ = 0;
  void pushEvent(MqttProvisioningEventKind kind, const char* reason = nullptr);
  void fail(const char* reason, uint32_t nowMs);
};
