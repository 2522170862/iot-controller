#include <assert.h>
#include <string.h>

#include "../../MqttMessageProtocol.cpp"

void setup() {
  MqttCommand command = {};
  assert(MqttMessageProtocol::parseCommand(
             "{\"hash\":\"8f29c104\",\"module\":\"relay\",\"channel\":1,\"action\":\"on\"}",
             &command) == MqttParseResult::kOk);
  assert(command.kind == MqttCommandKind::kRelay);
  assert(command.channel == 1);
  assert(command.value1 == 1);
  assert(strcmp(command.hash, "8f29c104") == 0);

  assert(MqttMessageProtocol::parseCommand(
             "{\"module\":\"servo\",\"action\":\"set\",\"params\":{\"angle\":181}}",
             &command) == MqttParseResult::kInvalidParams);

  assert(MqttMessageProtocol::parseCommand(
             "{\"module\":\"dc_motor\",\"action\":\"forward\",\"params\":{\"speed\":60}}",
             &command) == MqttParseResult::kOk);
  assert(command.kind == MqttCommandKind::kDcMotorForward);
  assert(command.value1 == 60);

  assert(MqttMessageProtocol::parseCommand(
             "{\"module\":\"dc_motor\",\"action\":\"reverse\",\"params\":{\"speed\":101}}",
             &command) == MqttParseResult::kInvalidParams);

  assert(MqttMessageProtocol::parseCommand(
             "{\"module\":\"dc_motor\",\"action\":\"stop\"}",
             &command) == MqttParseResult::kOk);
  assert(command.kind == MqttCommandKind::kDcMotorStop);

  char hash[9] = {};
  MqttMessageProtocol::generateHash(1, 1000, hash);
  assert(strlen(hash) == 8);
}

void loop() {}
