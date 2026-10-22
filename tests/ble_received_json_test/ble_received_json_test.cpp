// Exercise the firmware-only serial output with a host serial boundary.
#define ARDUINO 10819
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../BleProvisioningProtocol.cpp"
#include "../../MqttConfiguration.cpp"

void require(bool condition, const char* description) {
  if (!condition) { fprintf(stderr, "FAIL: %s\n", description); exit(1); }
}

int main() {
  const char* wifi = "{\"id\":\"a1b2c3d4\",\"cmd\":\"configure_wifi\",\"ssid\":\"Lab\",\"password\":\"dummy&pass\"}\n";
  BleProvisioningFrameAssembler assembler;
  BleWifiRequest request;
  for (size_t offset = 0; offset < strlen(wifi); offset += 20) {
    const size_t remaining = strlen(wifi) - offset;
    const size_t length = remaining < 20 ? remaining : 20;
    const bool last = offset + length == strlen(wifi);
    const auto result = assembler.append(reinterpret_cast<const uint8_t*>(wifi + offset), length, &request);
    require(result == (last ? BleProtocolResult::kOk : BleProtocolResult::kNeedMore), "legacy parsing result is unchanged");
    if (!last) require(Serial.output.empty(), "partial JSON is not printed as a complete request");
  }
  require(Serial.output == std::string("[BLE JSON RX] ") + wifi, "complete Wi-Fi JSON is printed verbatim once");
  require(strcmp(request.password, "dummy&pass") == 0, "serial logging does not modify parsed credentials");

  // Current APP sends the same MQTT JSON either in one write or plain 20-byte
  // fallback fragments. Exercise the actual firmware assembler, not a JSON mock.
  const char* plainMqtt = "{\"id\":\"1234abcd\",\"cmd\":\"configure_mqtt\",\"server\":\"broker.example.com\",\"port\":1883,\"username\":\"user\",\"password\":\"dummy&pass\"}\n";
  for (size_t chunkBytes : {size_t(20), size_t(244)}) {
    assembler.reset();
    Serial.output.clear();
    for (size_t offset = 0; offset < strlen(plainMqtt); offset += chunkBytes) {
      const size_t remaining = strlen(plainMqtt) - offset;
      const size_t length = remaining < chunkBytes ? remaining : chunkBytes;
      const auto result = assembler.append(reinterpret_cast<const uint8_t*>(plainMqtt + offset), length, &request);
      require(result == (offset + length == strlen(plainMqtt) ? BleProtocolResult::kOk : BleProtocolResult::kNeedMore),
              "plain MQTT JSON works with both one write and fallback fragments");
    }
    require(request.kind == BleProvisioningKind::kMqtt && strcmp(request.mqtt.value.server, "broker.example.com") == 0 &&
            strcmp(request.mqtt.value.password, "dummy&pass") == 0, "firmware receives plain MQTT fields unchanged");
    require(Serial.output == std::string("[BLE JSON RX] ") + plainMqtt, "plain MQTT JSON is printed once");
  }

  Serial.output.clear();
  const char* mqtt = "{ \"id\":\"1234abcd\",\"cmd\":\"configure_mqtt\",\"server\":\"mqtt.example.com\",\"port\":1883,\"username\":\"user\",\"password\":\"dummy\\\"&pass\" }\n";
  BleAcknowledgedReceiver receiver;
  BleChunkReceipt receipt;
  for (size_t offset = 0, sequence = 0; offset < strlen(mqtt); offset += 7, ++sequence) {
    const size_t remaining = strlen(mqtt) - offset;
    const size_t length = remaining < 7 ? remaining : 7;
    char packet[21] = {};
    snprintf(packet, sizeof(packet), "@1234abcd%03x:", static_cast<unsigned>(sequence));
    memcpy(packet + 13, mqtt + offset, length);
    const bool last = offset + length == strlen(mqtt);
    const auto result = receiver.append(reinterpret_cast<const uint8_t*>(packet), 13 + length, &receipt, &request);
    require(result == (last ? BleProtocolResult::kOk : BleProtocolResult::kNeedMore), "acknowledged parsing result is unchanged");
    if (!last) require(Serial.output.empty(), "acknowledged partial JSON is not logged");
    require(receiver.append(reinterpret_cast<const uint8_t*>(packet), 13 + length, &receipt, &request) == BleProtocolResult::kDuplicateChunk,
            "duplicate handling is unchanged");
  }
  require(Serial.output == std::string("[BLE JSON RX] ") + mqtt, "MQTT raw spacing and escaping are preserved, duplicate packets are not logged twice");
  require(strcmp(request.mqtt.value.password, "dummy\"&pass") == 0, "MQTT credentials remain unchanged");

  Serial.output.clear();
  const char* invalid = "{broken-json}\n";
  require(assembler.append(reinterpret_cast<const uint8_t*>(invalid), strlen(invalid), &request) == BleProtocolResult::kInvalidJson,
          "malformed input is still rejected");
  require(Serial.output == std::string("[BLE JSON RX] ") + invalid, "complete malformed JSON is also visible for diagnosis");
  puts("PASS: received BLE JSON printed verbatim once, parsing and duplicate handling unchanged");
}
