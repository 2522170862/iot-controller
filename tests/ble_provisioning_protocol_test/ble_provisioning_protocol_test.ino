#include <assert.h>
#include <string.h>

#include "../../BleProvisioningProtocol.cpp"

namespace {
void assertValidSplitRequest() {
  BleProvisioningFrameAssembler assembler;
  BleWifiRequest request = {};
  const char* first =
      "{\"id\":\"a1b2c3d4\",\"cmd\":\"configure_wifi\",\"ssid\":\"Lab";
  const char* second = "-WiFi\",\"password\":\"12345678\"}\n";

  assert(assembler.append(reinterpret_cast<const uint8_t*>(first),
                          strlen(first), &request) ==
         BleProtocolResult::kNeedMore);
  assert(assembler.append(reinterpret_cast<const uint8_t*>(second),
                          strlen(second), &request) == BleProtocolResult::kOk);
  assert(strcmp(request.id, "a1b2c3d4") == 0);
  assert(strcmp(request.ssid, "Lab-WiFi") == 0);
  assert(strcmp(request.password, "12345678") == 0);
}

void assertTwoFramesAcrossWrites() {
  BleProvisioningFrameAssembler assembler;
  BleWifiRequest request = {};
  const char* frames =
      "{\"id\":\"11111111\",\"cmd\":\"configure_wifi\",\"ssid\":\"A\",\"password\":\"\"}\n"
      "{\"id\":\"22222222\",\"cmd\":\"configure_wifi\",\"ssid\":\"B\",\"password\":\"p\"}\n";

  const char* separator = strchr(frames, '\n');
  const size_t firstAndPartOfSecond =
      static_cast<size_t>(separator - frames) + 8;
  assert(assembler.append(reinterpret_cast<const uint8_t*>(frames),
                          firstAndPartOfSecond, &request) ==
         BleProtocolResult::kOk);
  assert(strcmp(request.id, "11111111") == 0);
  assert(assembler.append(
             reinterpret_cast<const uint8_t*>(frames + firstAndPartOfSecond),
             strlen(frames) - firstAndPartOfSecond, &request) ==
         BleProtocolResult::kOk);
  assert(strcmp(request.id, "22222222") == 0);
}

void assertValidationFailures() {
  struct Case {
    const char* json;
    BleProtocolResult expected;
  } cases[] = {
      {"not-json\n", BleProtocolResult::kInvalidJson},
      {"{\"id\":\"123\",\"cmd\":\"configure_wifi\",\"ssid\":\"A\",\"password\":\"\"}\n",
       BleProtocolResult::kInvalidRequest},
      {"{\"id\":\"12345678\",\"cmd\":\"wrong\",\"ssid\":\"A\",\"password\":\"\"}\n",
       BleProtocolResult::kInvalidRequest},
      {"{\"id\":\"12345678\",\"cmd\":\"configure_wifi\",\"ssid\":\"\",\"password\":\"\"}\n",
       BleProtocolResult::kInvalidSsid},
      {"{\"id\":\"12345678\",\"cmd\":\"configure_wifi\",\"ssid\":\"123456789012345678901234567890123\",\"password\":\"\"}\n",
       BleProtocolResult::kInvalidSsid},
      {"{\"id\":\"12345678\",\"cmd\":\"configure_wifi\",\"ssid\":\"A\",\"password\":\"1234567890123456789012345678901234567890123456789012345678901234\"}\n",
       BleProtocolResult::kInvalidPassword},
  };

  for (const Case& testCase : cases) {
    BleProvisioningFrameAssembler assembler;
    BleWifiRequest request = {};
    assert(assembler.append(
               reinterpret_cast<const uint8_t*>(testCase.json),
               strlen(testCase.json), &request) == testCase.expected);
  }
}

void assertOversizedFrameResets() {
  BleProvisioningFrameAssembler assembler;
  BleWifiRequest request = {};
  uint8_t oversized[257];
  memset(oversized, 'x', sizeof(oversized));
  assert(assembler.append(oversized, sizeof(oversized), &request) ==
         BleProtocolResult::kMessageTooLong);

  const char* valid =
      "{\"id\":\"abcdef01\",\"cmd\":\"configure_wifi\",\"ssid\":\"A\",\"password\":\"\"}\n";
  assert(assembler.append(reinterpret_cast<const uint8_t*>(valid),
                          strlen(valid), &request) == BleProtocolResult::kOk);
  assert(strcmp(request.id, "abcdef01") == 0);
}

void assertEventSerialization() {
  char json[192] = {};
  assert(BleProvisioningProtocol::makeEvent(
      "a1b2c3d4", "wifi_connected", nullptr, "192.168.1.5", json,
      sizeof(json)));
  assert(strcmp(json,
                "{\"id\":\"a1b2c3d4\",\"event\":\"wifi_connected\","
                "\"ip\":\"192.168.1.5\"}\n") == 0);
}

void assertRequestQueueCopiesInFifoOrder() {
  BleProvisioningRequestQueue queue;
  BleWifiRequest first = {};
  snprintf(first.id, sizeof(first.id), "%s", "11111111");
  snprintf(first.ssid, sizeof(first.ssid), "%s", "First");
  BleWifiRequest second = {};
  snprintf(second.id, sizeof(second.id), "%s", "22222222");
  snprintf(second.ssid, sizeof(second.ssid), "%s", "Second");

  assert(queue.push(first));
  assert(queue.push(second));
  snprintf(first.ssid, sizeof(first.ssid), "%s", "Changed");

  BleWifiRequest output = {};
  assert(queue.pop(&output));
  assert(strcmp(output.id, "11111111") == 0);
  assert(strcmp(output.ssid, "First") == 0);
  assert(queue.pop(&output));
  assert(strcmp(output.id, "22222222") == 0);
  assert(!queue.pop(&output));
}

void assertRequestQueueRejectsOverflow() {
  BleProvisioningRequestQueue queue;
  BleWifiRequest request = {};
  for (uint8_t index = 0; index < BleProvisioningRequestQueue::kCapacity;
       ++index) {
    request.id[0] = static_cast<char>('0' + index);
    assert(queue.push(request));
  }
  assert(!queue.push(request));
}
}  // namespace

void setup() {
  assertValidSplitRequest();
  assertTwoFramesAcrossWrites();
  assertValidationFailures();
  assertOversizedFrameResets();
  assertEventSerialization();
  assertRequestQueueCopiesInFifoOrder();
  assertRequestQueueRejectsOverflow();
}

void loop() {}
