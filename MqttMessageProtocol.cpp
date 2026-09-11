#include "MqttMessageProtocol.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
bool readInt(JsonVariantConst value, int32_t minimum, int32_t maximum,
             int32_t* output) {
  if (!value.is<int>() || output == nullptr) return false;
  const int32_t parsed = value.as<int>();
  if (parsed < minimum || parsed > maximum) return false;
  *output = parsed;
  return true;
}
}  // namespace

MqttParseResult MqttMessageProtocol::parseCommand(const char* json,
                                                   MqttCommand* output) {
  if (json == nullptr || output == nullptr) return MqttParseResult::kInvalidJson;
  *output = {};
  JsonDocument document;
  if (deserializeJson(document, json)) return MqttParseResult::kInvalidJson;
  const char* hash = document["hash"] | "";
  if (hash[0] != '\0' && !isHash(hash)) return MqttParseResult::kInvalidHash;
  snprintf(output->hash, sizeof(output->hash), "%s", hash);
  const char* module = document["module"] | "";
  const char* action = document["action"] | "";

  if (strcmp(module, "relay") == 0) {
    int32_t channel = 0;
    if (!readInt(document["channel"], 1, 2, &channel)) return MqttParseResult::kInvalidParams;
    output->kind = MqttCommandKind::kRelay;
    output->channel = static_cast<uint8_t>(channel);
    if (strcmp(action, "on") == 0) output->value1 = 1;
    else if (strcmp(action, "off") == 0) output->value1 = 0;
    else return MqttParseResult::kInvalidAction;
    return MqttParseResult::kOk;
  }
  if (strcmp(module, "rgb") == 0) {
    if (strcmp(action, "clear") == 0) { output->kind = MqttCommandKind::kRgbClear; return MqttParseResult::kOk; }
    if (strcmp(action, "set") != 0) return MqttParseResult::kInvalidAction;
    JsonObjectConst params = document["params"].as<JsonObjectConst>();
    if (params.isNull() || !readInt(params["r"], 0, 255, &output->value1) ||
        !readInt(params["g"], 0, 255, &output->value2) ||
        !readInt(params["b"], 0, 255, &output->value3) ||
        !readInt(params["brightness"], 0, 25, &output->value4)) return MqttParseResult::kInvalidParams;
    output->kind = MqttCommandKind::kRgbSet;
    return MqttParseResult::kOk;
  }
  if (strcmp(module, "servo") == 0) {
    if (strcmp(action, "set") != 0) return MqttParseResult::kInvalidAction;
    JsonObjectConst params = document["params"].as<JsonObjectConst>();
    if (params.isNull() || !readInt(params["angle"], 0, 180, &output->value1)) return MqttParseResult::kInvalidParams;
    output->kind = MqttCommandKind::kServo;
    return MqttParseResult::kOk;
  }
  if (strcmp(module, "stepper") == 0) {
    if (strcmp(action, "stop") == 0) { output->kind = MqttCommandKind::kStepperStop; return MqttParseResult::kOk; }
    if (strcmp(action, "move") != 0) return MqttParseResult::kInvalidAction;
    JsonObjectConst params = document["params"].as<JsonObjectConst>();
    const float turns = params["turns"] | 0.0f;
    const int speed = params["speed"] | 0;
    const char* direction = params["direction"] | "";
    if (params.isNull() || turns <= 0.0f || turns > 100.0f || speed < 1 || speed > 15 ||
        (strcmp(direction, "cw") != 0 && strcmp(direction, "ccw") != 0)) return MqttParseResult::kInvalidParams;
    output->kind = MqttCommandKind::kStepperMove;
    output->value1 = static_cast<int32_t>(lroundf(turns * 1000.0f));
    output->value2 = strcmp(direction, "cw") == 0 ? 1 : -1;
    output->value3 = speed;
    return MqttParseResult::kOk;
  }
  if (strcmp(module, "dc_motor") == 0) {
    if (strcmp(action, "stop") == 0) {
      output->kind = MqttCommandKind::kDcMotorStop;
      return MqttParseResult::kOk;
    }
    JsonObjectConst params = document["params"].as<JsonObjectConst>();
    if (params.isNull() ||
        !readInt(params["speed"], 0, 100, &output->value1)) {
      return MqttParseResult::kInvalidParams;
    }
    if (strcmp(action, "forward") == 0) {
      output->kind = MqttCommandKind::kDcMotorForward;
    } else if (strcmp(action, "reverse") == 0) {
      output->kind = MqttCommandKind::kDcMotorReverse;
    } else {
      return MqttParseResult::kInvalidAction;
    }
    return MqttParseResult::kOk;
  }
  if (strcmp(module, "input") == 0) {
    if (strcmp(action, "get") != 0) return MqttParseResult::kInvalidAction;
    output->kind = MqttCommandKind::kInputGet;
    return MqttParseResult::kOk;
  }
  return MqttParseResult::kInvalidModule;
}

void MqttMessageProtocol::generateHash(uint32_t sequence, uint32_t nowMs,
                                       char output[9]) {
  uint32_t hash = 2166136261UL;
  const uint32_t values[] = {sequence, nowMs};
  for (uint32_t value : values) {
    for (uint8_t byte = 0; byte < 4; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xFF;
      hash *= 16777619UL;
    }
  }
  snprintf(output, 9, "%08lx", static_cast<unsigned long>(hash));
}

bool MqttMessageProtocol::isHash(const char* value) {
  if (strlen(value) != 8) return false;
  for (uint8_t index = 0; index < 8; ++index) {
    const char character = value[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f') ||
          (character >= 'A' && character <= 'F'))) return false;
  }
  return true;
}
