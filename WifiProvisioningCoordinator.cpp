#include "WifiProvisioningCoordinator.h"

#include <stdio.h>
#include <string.h>

WifiProvisioningCoordinator::WifiProvisioningCoordinator(
    IWifiConnection* wifi, IWifiCredentialStore* store)
    : wifi_(wifi), store_(store) {}

bool WifiProvisioningCoordinator::start(const BleWifiRequest& request,
                                        uint32_t nowMs) {
  if (active_ || wifi_ == nullptr || store_ == nullptr) {
    return false;
  }

  previousCredentials_ = wifi_->currentCredentials();
  candidateCredentials_ = {};
  snprintf(candidateCredentials_.ssid, sizeof(candidateCredentials_.ssid),
           "%s", request.ssid);
  snprintf(candidateCredentials_.password,
           sizeof(candidateCredentials_.password), "%s", request.password);
  snprintf(requestId_, sizeof(requestId_), "%s", request.id);

  active_ = true;
  startedMs_ = nowMs;
  pushEvent(WifiProvisioningEventKind::kConnecting);
  wifi_->connect(candidateCredentials_, nowMs);
  return true;
}

void WifiProvisioningCoordinator::poll(uint32_t nowMs) {
  if (!active_ || wifi_ == nullptr || store_ == nullptr) {
    return;
  }

  const WifiAttemptState state = wifi_->attemptState();
  if (state == WifiAttemptState::kConnected) {
    if (!store_->save(candidateCredentials_)) {
      finishFailure("storage_failed", nowMs);
      return;
    }
    active_ = false;
    pushEvent(WifiProvisioningEventKind::kConnected, nullptr,
              wifi_->localIp());
    return;
  }
  if (state == WifiAttemptState::kFailed) {
    finishFailure("connection_failed", nowMs);
    return;
  }
  if (nowMs - startedMs_ >= kValidationTimeoutMs) {
    finishFailure("timeout", nowMs);
  }
}

bool WifiProvisioningCoordinator::active() const { return active_; }

bool WifiProvisioningCoordinator::takeEvent(WifiProvisioningEvent* output) {
  if (output == nullptr || eventSize_ == 0) {
    return false;
  }
  *output = events_[eventHead_];
  eventHead_ = (eventHead_ + 1) % kEventCapacity;
  --eventSize_;
  return true;
}

void WifiProvisioningCoordinator::finishFailure(const char* reason,
                                                uint32_t nowMs) {
  active_ = false;
  wifi_->connect(previousCredentials_, nowMs);
  pushEvent(WifiProvisioningEventKind::kFailed, reason);
}

void WifiProvisioningCoordinator::pushEvent(WifiProvisioningEventKind kind,
                                            const char* reason,
                                            const char* ip) {
  if (eventSize_ >= kEventCapacity) {
    eventHead_ = (eventHead_ + 1) % kEventCapacity;
    --eventSize_;
  }
  const uint8_t tail = (eventHead_ + eventSize_) % kEventCapacity;
  events_[tail] = {};
  events_[tail].kind = kind;
  snprintf(events_[tail].id, sizeof(events_[tail].id), "%s", requestId_);
  if (reason != nullptr) {
    snprintf(events_[tail].reason, sizeof(events_[tail].reason), "%s",
             reason);
  }
  if (ip != nullptr) {
    snprintf(events_[tail].ip, sizeof(events_[tail].ip), "%s", ip);
  }
  ++eventSize_;
}
