#pragma once

#include <stdint.h>

enum class MqttCommandKind {
  kRelay,
  kRgbSet,
  kRgbClear,
  kServo,
  kStepperMove,
  kStepperStop,
  kInputGet,
  kInvalid,
};

enum class MqttParseResult { kOk, kInvalidJson, kInvalidHash, kInvalidModule, kInvalidAction, kInvalidParams };

struct MqttCommand {
  MqttCommandKind kind = MqttCommandKind::kInvalid;
  char hash[9] = {};
  uint8_t channel = 0;
  int32_t value1 = 0;
  int32_t value2 = 0;
  int32_t value3 = 0;
  int32_t value4 = 0;
};

class MqttMessageProtocol {
 public:
  static MqttParseResult parseCommand(const char* json, MqttCommand* output);
  static void generateHash(uint32_t sequence, uint32_t nowMs, char output[9]);

 private:
  static bool isHash(const char* value);
};
