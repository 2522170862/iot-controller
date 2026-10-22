#include "BleProvisioningProtocol.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace {
template <size_t N>
bool copyJsonString(JsonVariantConst field, char (&destination)[N]) {
  if (!field.is<const char*>()) return false;
  const JsonString text = field.as<JsonString>();
  if (text.size() >= N || strlen(text.c_str()) != text.size()) return false;
  memcpy(destination, text.c_str(), text.size() + 1);
  return true;
}

BleProtocolResult parseMqtt(JsonDocument& document, BleWifiRequest* output) {
  output->kind = BleProvisioningKind::kMqtt;
  MqttConfiguration& value = output->mqtt.value;
  if (!copyJsonString(document["server"], value.server) ||
      !copyJsonString(document["username"], value.username) ||
      !copyJsonString(document["password"], value.password) ||
      !document["port"].is<uint16_t>() || document["port"].as<uint16_t>() == 0) {
    return BleProtocolResult::kInvalidMqtt;
  }
  value.port = document["port"].as<uint16_t>();
  // Check the complete candidate with harmless defaults for omitted topics;
  // the coordinator later substitutes the actual current topics.
  snprintf(value.subTopic, sizeof(value.subTopic), "LoTS2C/");
  snprintf(value.pubTopic, sizeof(value.pubTopic), "LoTC2S/");
  output->mqtt.hasSubTopic = document["subTopic"].isUnbound() == false;
  output->mqtt.hasPubTopic = document["pubTopic"].isUnbound() == false;
  output->mqtt.hasSubQos = document["subQos"].isUnbound() == false;
  if ((output->mqtt.hasSubTopic && !copyJsonString(document["subTopic"], value.subTopic)) ||
      (output->mqtt.hasPubTopic && !copyJsonString(document["pubTopic"], value.pubTopic)) ||
      (output->mqtt.hasSubQos && (!document["subQos"].is<uint8_t>() || document["subQos"].as<uint8_t>() > 1))) {
    return BleProtocolResult::kInvalidMqtt;
  }
  if (output->mqtt.hasSubQos) value.subQos = document["subQos"].as<uint8_t>();
  return validMqttConfiguration(value) ? BleProtocolResult::kOk : BleProtocolResult::kInvalidMqtt;
}
}  // namespace

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

#ifdef ARDUINO
  Serial.print("[BLE JSON RX] ");
  Serial.println(json);
#endif

  JsonDocument document;
  if (deserializeJson(document, json)) {
    return BleProtocolResult::kInvalidJson;
  }

  const char* id = document["id"] | "";
  const char* command = document["cmd"] | "";
  *output = {};
  if (!isRequestId(id)) return BleProtocolResult::kInvalidRequest;
  snprintf(output->id, sizeof(output->id), "%s", id);
  if (strcmp(command, "configure_mqtt") == 0) return parseMqtt(document, output);
  if (strcmp(command, "configure_wifi") != 0 ||
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

bool BleProvisioningProtocol::makeChunkAck(const char* id, uint16_t sequence,
                                           char* output, size_t capacity) {
  if (!isRequestId(id) || output == nullptr) return false;
  JsonDocument document;
  document["id"] = id;
  document["event"] = "chunk_ack";
  document["seq"] = sequence;
  const size_t length = measureJson(document);
  if (length + 2 > capacity) return false;
  serializeJson(document, output, capacity);
  output[length] = '\n';
  output[length + 1] = '\0';
  return true;
}

BleProtocolResult BleAcknowledgedReceiver::append(
    const uint8_t* packet, size_t length, BleChunkReceipt* receipt,
    BleWifiRequest* request) {
  if (receipt == nullptr || request == nullptr) return BleProtocolResult::kInvalidChunk;
  *receipt = {};
  if (packet == nullptr || length < 14 || length > kBleProvisioningMaxChunkBytes ||
      packet[0] != '@' || packet[12] != ':') return BleProtocolResult::kInvalidChunk;
  uint16_t sequence = 0;
  for (size_t i = 1; i < 12; ++i) {
    const char c = static_cast<char>(packet[i]);
    int digit = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    if (digit < 0) return BleProtocolResult::kInvalidChunk;
    if (i <= 8) receipt->id[i - 1] = c;
    else sequence = static_cast<uint16_t>((sequence << 4) | digit);
  }
  receipt->sequence = sequence;
  if (lastLength_ == length && memcmp(lastPacket_, packet, length) == 0) {
    return BleProtocolResult::kDuplicateChunk;
  }
  if (sequence == 0) {
    reset();
    memcpy(id_, receipt->id, sizeof(id_));
  }
  if (completed_ || sequence != nextSequence_ || strcmp(id_, receipt->id) != 0) {
    return BleProtocolResult::kInvalidChunk;
  }
  const size_t payloadLength = length - 13;
  if (receivedBytes_ + payloadLength > kBleProvisioningMaxMessageBytes) {
    reset();
    return BleProtocolResult::kMessageTooLong;
  }
  for (size_t i = 13; i + 1 < length; ++i) {
    if (packet[i] == '\n') { reset(); return BleProtocolResult::kInvalidChunk; }
  }
  const auto result = assembler_.append(packet + 13, payloadLength, request);
  if (result != BleProtocolResult::kNeedMore && result != BleProtocolResult::kOk) {
    reset();
    return result;
  }
  if (result == BleProtocolResult::kOk && strcmp(request->id, id_) != 0) {
    reset();
    return BleProtocolResult::kInvalidRequest;
  }
  receivedBytes_ += payloadLength;
  memcpy(lastPacket_, packet, length);
  lastLength_ = length;
  ++nextSequence_;
  completed_ = result == BleProtocolResult::kOk;
  return result;
}

void BleAcknowledgedReceiver::reset() {
  assembler_.reset();
  id_[0] = '\0';
  nextSequence_ = 0;
  receivedBytes_ = 0;
  lastLength_ = 0;
  completed_ = false;
}
