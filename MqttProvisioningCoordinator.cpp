#include "MqttProvisioningCoordinator.h"

#include <stdio.h>
#include <string.h>

MqttProvisioningCoordinator::MqttProvisioningCoordinator(
    IMqttConfigurationConnection* connection, IMqttConfigurationStore* store)
    : connection_(connection), store_(store) {}

bool MqttProvisioningCoordinator::start(const BleProvisioningRequest& request, uint32_t nowMs) {
  if (active_ || connection_ == nullptr || store_ == nullptr ||
      request.kind != BleProvisioningKind::kMqtt) return false;
  previous_ = connection_->currentConfiguration();
  MqttConfiguration candidate = request.mqtt.value;
  if (!request.mqtt.hasSubTopic) memcpy(candidate.subTopic, previous_.subTopic, sizeof(candidate.subTopic));
  if (!request.mqtt.hasPubTopic) memcpy(candidate.pubTopic, previous_.pubTopic, sizeof(candidate.pubTopic));
  if (!request.mqtt.hasSubQos) candidate.subQos = previous_.subQos;
  if (!validMqttConfiguration(candidate)) return false;
  snprintf(id_, sizeof(id_), "%s", request.id);
  startedMs_ = nowMs;
  active_ = true;
  pushEvent(MqttProvisioningEventKind::kConnecting);
  connection_->configure(candidate, nowMs);
  return true;
}

void MqttProvisioningCoordinator::poll(uint32_t nowMs) {
  if (!active_) return;
  if (nowMs - startedMs_ >= kValidationTimeoutMs) {
    fail("timeout", nowMs);
    return;
  }
  if (connection_->configurationAttempt() == MqttConfigurationAttempt::kConnected) {
    if (!store_->save(connection_->currentConfiguration())) { fail("storage_failed", nowMs); return; }
    active_ = false;
    pushEvent(MqttProvisioningEventKind::kConnected);
  } else if (connection_->configurationAttempt() == MqttConfigurationAttempt::kFailed) {
    fail(connection_->configurationFailure(), nowMs);
  }
}

void MqttProvisioningCoordinator::fail(const char* reason, uint32_t nowMs) {
  // Copy the reason before configure() resets the connection attempt.
  pushEvent(MqttProvisioningEventKind::kFailed, reason);
  connection_->configure(previous_, nowMs);
  active_ = false;
}

void MqttProvisioningCoordinator::pushEvent(MqttProvisioningEventKind kind, const char* reason) {
  if (eventCount_ == 2) { events_[0] = events_[1]; --eventCount_; }
  auto& event = events_[eventCount_++];
  event = {};
  event.kind = kind;
  snprintf(event.id, sizeof(event.id), "%s", id_);
  if (reason) snprintf(event.reason, sizeof(event.reason), "%s", reason);
}

bool MqttProvisioningCoordinator::takeEvent(MqttProvisioningEvent* output) {
  if (!output || eventCount_ == 0) return false;
  *output = events_[0];
  events_[0] = events_[1];
  --eventCount_;
  return true;
}
