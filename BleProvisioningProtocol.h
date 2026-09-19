#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BleProvisioningUuids {
constexpr char kService[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kRx[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char kTx[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
}  // namespace BleProvisioningUuids

constexpr size_t kBleProvisioningMaxChunkBytes = 20;
constexpr size_t kBleProvisioningMaxMessageBytes = 256;

enum class BleProtocolResult : uint8_t {
  kOk,
  kNeedMore,
  kInvalidJson,
  kInvalidRequest,
  kInvalidSsid,
  kInvalidPassword,
  kMessageTooLong,
};

struct BleWifiRequest {
  char id[9] = {};
  char ssid[33] = {};
  char password[64] = {};
};

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
