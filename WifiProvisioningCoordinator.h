#pragma once

#include <stdint.h>

#include "BleProvisioningProtocol.h"
#include "WiFiDataSource.h"
#include "WifiCredentialStore.h"

enum class WifiProvisioningEventKind : uint8_t {
  kConnecting,
  kConnected,
  kFailed,
};

struct WifiProvisioningEvent {
  WifiProvisioningEventKind kind = WifiProvisioningEventKind::kFailed;
  char id[9] = {};
  char reason[24] = {};
  char ip[16] = {};
};

class WifiProvisioningCoordinator {
 public:
  static constexpr uint32_t kValidationTimeoutMs = 15000;

  WifiProvisioningCoordinator(IWifiConnection* wifi,
                              IWifiCredentialStore* store);

  bool start(const BleWifiRequest& request, uint32_t nowMs);
  void poll(uint32_t nowMs);
  bool active() const;
  bool takeEvent(WifiProvisioningEvent* output);

 private:
  static constexpr uint8_t kEventCapacity = 2;

  IWifiConnection* wifi_;
  IWifiCredentialStore* store_;
  bool active_ = false;
  uint32_t startedMs_ = 0;
  char requestId_[9] = {};
  WifiCredentialsValue previousCredentials_ = {};
  WifiCredentialsValue candidateCredentials_ = {};
  WifiProvisioningEvent events_[kEventCapacity] = {};
  uint8_t eventHead_ = 0;
  uint8_t eventSize_ = 0;

  void finishFailure(const char* reason, uint32_t nowMs);
  void pushEvent(WifiProvisioningEventKind kind, const char* reason = nullptr,
                 const char* ip = nullptr);
};
