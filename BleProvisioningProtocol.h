#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MqttConfiguration.h"

namespace BleProvisioningUuids {
constexpr char kService[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kRx[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kTx[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
}  // namespace BleProvisioningUuids

constexpr size_t kBleProvisioningMaxChunkBytes = 20;
constexpr size_t kBleProvisioningMaxMessageBytes = 1024;

enum class BleProtocolResult : uint8_t {
  kOk,
  kNeedMore,
  kInvalidJson,
  kInvalidRequest,
  kInvalidSsid,
  kInvalidPassword,
  kInvalidMqtt,
  kMessageTooLong,
  kDuplicateChunk,
  kInvalidChunk,
};

enum class BleProvisioningKind : uint8_t { kWifi, kMqtt };

struct BleMqttSettings {
  MqttConfiguration value;
  bool hasSubTopic = false;
  bool hasPubTopic = false;
  bool hasSubQos = false;
};

struct BleProvisioningRequest {
  char id[9] = {};
  char ssid[33] = {};
  char password[64] = {};
  BleProvisioningKind kind = BleProvisioningKind::kWifi;
  BleMqttSettings mqtt;
};

// Keep existing Wi-Fi-only test sketches and integrations source-compatible.
using BleWifiRequest = BleProvisioningRequest;

class BleProvisioningRequestQueue {
 public:
  static constexpr uint8_t kCapacity = 4;

  bool push(const BleWifiRequest& request);
  bool pop(BleWifiRequest* output);
  uint8_t size() const;
  void clear();

 private:
  BleWifiRequest entries_[kCapacity] = {};
  uint8_t head_ = 0;
  uint8_t size_ = 0;
};

class BleProvisioningProtocol {
 public:
  static BleProtocolResult parseRequest(const char* json,
                                        BleWifiRequest* output);
  static bool makeEvent(const char* id, const char* event,
                        const char* reason, const char* ip, char* output,
                        size_t capacity);
  static bool makeChunkAck(const char* id, uint16_t sequence,
                           char* output, size_t capacity);

 private:
  static bool isRequestId(const char* value);
};

class BleProvisioningFrameAssembler {
 public:
  BleProtocolResult append(const uint8_t* data, size_t length,
                           BleWifiRequest* output);
  void reset();

 private:
  char buffer_[kBleProvisioningMaxMessageBytes + 1] = {};
  size_t length_ = 0;
};

struct BleChunkReceipt {
  char id[9] = {};
  uint16_t sequence = 0;
};

// Wire format: @ + eight hex ID digits + three hex sequence digits + ':' +
// up to seven raw JSON bytes. The entire physical write is at most 20 bytes.
class BleAcknowledgedReceiver {
 public:
  BleProtocolResult append(const uint8_t* packet, size_t length,
                           BleChunkReceipt* receipt, BleWifiRequest* request);
  void reset();

 private:
  BleProvisioningFrameAssembler assembler_;
  char id_[9] = {};
  uint16_t nextSequence_ = 0;
  size_t receivedBytes_ = 0;
  uint8_t lastPacket_[kBleProvisioningMaxChunkBytes] = {};
  size_t lastLength_ = 0;
  bool completed_ = false;
};
