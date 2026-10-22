#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../BleProvisioningProtocol.cpp"
#include "../../MqttConfiguration.cpp"

int main() {
  BleAcknowledgedReceiver receiver;
  BleChunkReceipt receipt = {};
  BleWifiRequest request = {};
  const char* json = "{\"id\":\"a1b2c3d4\",\"cmd\":\"configure_wifi\",\"ssid\":\"773\",\"password\":\"correct-pass\"}\n";
  char packet[21] = {};
  unsigned seq = 0;
  for (size_t offset = 0; offset < strlen(json); offset += 7, ++seq) {
    const size_t count = strlen(json) - offset < 7 ? strlen(json) - offset : 7;
    snprintf(packet, sizeof(packet), "@a1b2c3d4%03x:", seq);
    memcpy(packet + 13, json + offset, count);
    const auto result = receiver.append(reinterpret_cast<uint8_t*>(packet), 13 + count, &receipt, &request);
    assert(result == (offset + count == strlen(json) ? BleProtocolResult::kOk : BleProtocolResult::kNeedMore));
    assert(strcmp(receipt.id, "a1b2c3d4") == 0 && receipt.sequence == seq);
    // Duplicated chunks, including the final one, cannot append or dispatch twice.
    assert(receiver.append(reinterpret_cast<uint8_t*>(packet), 13 + count, &receipt, &request) == BleProtocolResult::kDuplicateChunk);
  }
  assert(strcmp(request.ssid, "773") == 0);
  assert(strcmp(request.password, "correct-pass") == 0);
  receiver.reset();
  const char* outOfOrder = "@a1b2c3d4001:abc";
  assert(receiver.append(reinterpret_cast<const uint8_t*>(outOfOrder), strlen(outOfOrder), &receipt, &request) == BleProtocolResult::kInvalidChunk);
  const char* partial = "@a1b2c3d4000:{\"id\":";
  assert(receiver.append(reinterpret_cast<const uint8_t*>(partial), strlen(partial), &receipt, &request) == BleProtocolResult::kNeedMore);
  // A new request starts cleanly even if the previous transfer was interrupted.
  const char* fresh = "@11111111000:{\"id\":";
  assert(receiver.append(reinterpret_cast<const uint8_t*>(fresh), strlen(fresh), &receipt, &request) == BleProtocolResult::kNeedMore);
  assert(strcmp(receipt.id, "11111111") == 0);
  char ack[192] = {};
  assert(BleProvisioningProtocol::makeChunkAck("a1b2c3d4", 10, ack, sizeof(ack)));
  assert(strcmp(ack, "{\"id\":\"a1b2c3d4\",\"event\":\"chunk_ack\",\"seq\":10}\n") == 0);
  assert(!BleProvisioningProtocol::makeChunkAck("bad", 0, ack, sizeof(ack)));
  assert(!BleProvisioningProtocol::makeChunkAck("a1b2c3d4", 0, ack, 10));
  receiver.reset();
  // A transfer can never grow the existing 1024-byte JSON buffer without bound.
  size_t total = 0;
  for (unsigned sequence = 0; total <= 1024; ++sequence, total += 7) {
    snprintf(packet, sizeof(packet), "@a1b2c3d4%03x:", sequence);
    memset(packet + 13, 'x', 7);
    const auto result = receiver.append(reinterpret_cast<uint8_t*>(packet), 20, &receipt, &request);
    assert(result == (total + 7 > 1024 ? BleProtocolResult::kMessageTooLong : BleProtocolResult::kNeedMore));
  }
  puts("PASS: acknowledged BLE assembly, ordering, duplicate suppression and fresh retry");
}
