#include "BleProvisioningProtocol.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

bool BleProvisioningRequestQueue::push(const BleWifiRequest& request) {
  if (size_ >= kCapacity) {
    return false;
  }
  const uint8_t tail = (head_ + size_) % kCapacity;
  entries_[tail] = request;
  ++size_;
  return true;
}

bool BleProvisioningRequestQueue::pop(BleWifiRequest* output) {
  if (output == nullptr || size_ == 0) {
    return false;
  }
  *output = entries_[head_];
  head_ = (head_ + 1) % kCapacity;
  --size_;
  return true;
}

uint8_t BleProvisioningRequestQueue::size() const { return size_; }

void BleProvisioningRequestQueue::clear() {
  head_ = 0;
  size_ = 0;
}

BleProtocolResult BleProvisioningProtocol::parseRequest(
    const char* json, BleWifiRequest* output) {
  if (json == nullptr || output == nullptr) {
    return BleProtocolResult::kInvalidJson;
  }

  JsonDocument document;
  if (deserializeJson(document, json)) {
    return BleProtocolResult::kInvalidJson;
  }

  const char* id = document["id"] | "";
  const char* command = document["cmd"] | "";
  if (!isRequestId(id) || strcmp(command, "configure_wifi") != 0 ||
      !document["ssid"].is<const char*>() ||
      !document["password"].is<const char*>()) {
    return BleProtocolResult::kInvalidRequest;
  }

  const char* ssid = document["ssid"] | "";
  const char* password = document["password"] | "";
  const size_t ssidLength = strlen(ssid);
  const size_t passwordLength = strlen(password);
  if (ssidLength == 0 || ssidLength > 32) {
    return BleProtocolResult::kInvalidSsid;
  }
  if (passwordLength > 63) {
    return BleProtocolResult::kInvalidPassword;
  }

  *output = {};
  snprintf(output->id, sizeof(output->id), "%s", id);
  snprintf(output->ssid, sizeof(output->ssid), "%s", ssid);
  snprintf(output->password, sizeof(output->password), "%s", password);
  return BleProtocolResult::kOk;
}

bool BleProvisioningProtocol::makeEvent(
    const char* id, const char* event, const char* reason, const char* ip,
    char* output, size_t capacity) {
  if (!isRequestId(id) || event == nullptr || event[0] == '\0' ||
      output == nullptr || capacity < 3) {
    return false;
  }

  JsonDocument document;
  document["id"] = id;
  document["event"] = event;
  if (reason != nullptr && reason[0] != '\0') {
    document["reason"] = reason;
  }
  if (ip != nullptr && ip[0] != '\0') {
    document["ip"] = ip;
  }

  const size_t jsonLength = measureJson(document);
  if (jsonLength + 2 > capacity) {
    output[0] = '\0';
    return false;
  }
  serializeJson(document, output, capacity);
  output[jsonLength] = '\n';
  output[jsonLength + 1] = '\0';
  return true;
}

bool BleProvisioningProtocol::isRequestId(const char* value) {
  if (value == nullptr || strlen(value) != 8) {
    return false;
  }
  for (size_t index = 0; index < 8; ++index) {
    const char character = value[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f') ||
          (character >= 'A' && character <= 'F'))) {
      return false;
    }
  }
  return true;
}

BleProtocolResult BleProvisioningFrameAssembler::append(
    const uint8_t* data, size_t length, BleWifiRequest* output) {
  if (data == nullptr || output == nullptr) {
    return BleProtocolResult::kInvalidRequest;
  }

  for (size_t index = 0; index < length; ++index) {
    const char character = static_cast<char>(data[index]);
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      buffer_[length_] = '\0';
      const BleProtocolResult result =
          BleProvisioningProtocol::parseRequest(buffer_, output);
      length_ = 0;
      buffer_[0] = '\0';

      // A BLE write normally carries one 20-byte chunk. Preserve a partial
      // start of the next frame if a caller supplied bytes after the newline.
      for (++index; index < length; ++index) {
        const char remainder = static_cast<char>(data[index]);
        if (remainder == '\r') {
          continue;
        }
        if (remainder == '\n') {
          reset();
          break;
        }
        if (length_ >= kBleProvisioningMaxMessageBytes) {
          reset();
          break;
        }
        buffer_[length_++] = remainder;
      }
      buffer_[length_] = '\0';
      return result;
    }
    if (length_ >= kBleProvisioningMaxMessageBytes) {
      reset();
      return BleProtocolResult::kMessageTooLong;
    }
    buffer_[length_++] = character;
  }

  buffer_[length_] = '\0';
  return BleProtocolResult::kNeedMore;
}

void BleProvisioningFrameAssembler::reset() {
  length_ = 0;
  buffer_[0] = '\0';
}
