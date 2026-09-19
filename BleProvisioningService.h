#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "BleProvisioningProtocol.h"
#include "WifiProvisioningCoordinator.h"

class BLECharacteristic;
class BLEServer;

class BleProvisioningService {
 public:
  void begin();
  void poll(uint32_t nowMs);
  bool connected() const;
  bool takeRequest(BleWifiRequest* output);
  bool enqueueEvent(const WifiProvisioningEvent& event);

 private:
  class ServerCallbacks;
  class RxCallbacks;

  static constexpr uint8_t kOutgoingCapacity = 4;
  static constexpr size_t kOutgoingFrameCapacity = 192;
  static constexpr uint32_t kAdvertisingRestartDelayMs = 500;

  BLEServer* server_ = nullptr;
  BLECharacteristic* txCharacteristic_ = nullptr;
  ServerCallbacks* serverCallbacks_ = nullptr;
  RxCallbacks* rxCallbacks_ = nullptr;
  volatile bool connected_ = false;
  volatile bool advertisingRestartPending_ = false;
  volatile uint32_t disconnectedMs_ = 0;
  BleProvisioningFrameAssembler assembler_;
  BleProvisioningRequestQueue requests_;
  char outgoing_[kOutgoingCapacity][kOutgoingFrameCapacity] = {};
  uint8_t outgoingHead_ = 0;
  uint8_t outgoingSize_ = 0;
  size_t outgoingOffset_ = 0;
  portMUX_TYPE queueMux_ = portMUX_INITIALIZER_UNLOCKED;

  void onConnected();
  void onDisconnected();
  void onWrite(const uint8_t* data, size_t length);
  bool enqueueFrame(const char* frame);
  bool enqueueProtocolError(const char* id, const char* reason);
  void clearOutgoing();
};
