# MQTT 通信与模块控制 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 ESP32-S3 实现带队列、hash 关联、遥测和远程模块控制的 MQTT 通信。

**Architecture:** `WiFiDataSource` 只管理 Wi-Fi。`MqttService` 管理 PubSubClient、连接状态、固定收发队列及 hash；`ModuleCommandDispatcher` 验证命令后调用既有外设类。主循环保持非阻塞，并负责遥测时机。

**Tech Stack:** Arduino IDE `.ino`、ESP32 Arduino Core、WiFi、PubSubClient 2.8、ArduinoJson 7、Adafruit_NeoPixel。

**Spec:** `docs/superpowers/specs/2026-09-11-mqtt-integration-design.md`

## Global Constraints

- 使用 `esp32:esp32:esp32s3` 编译，保留 Arduino `.ino` 工程结构。
- Topic 固定：下行 `LoTS2C/`，上行 `LoTC2S/`，QoS 0；仅一台设备，不加设备 ID。
- `MqttCredentials.h` 使用用户提供的配置，受 Git 版本控制并提交。
- 设备上行包均含 8 位十六进制 hash；入站命令提供时原样回执，未提供则生成。
- MQTT 回调仅复制数据：入站队列 8 条、出站队列 16 条、JSON 最大 384 字节。
- 环境每 20 秒上报；摇杆只通过 `input/get` 查询；编码器变化上报；RFID 读卡即时上报。
- 控制 relay、rgb、servo、stepper；不实现直流电机。

## File Structure

- `MqttCredentials.h`：MQTT 主机、端口、账号、密码、Topic 和 QoS。
- `MqttMessageQueue.h/.cpp`：无动态分配 FIFO 和回执优先策略。
- `MqttMessageProtocol.h/.cpp`：命令 JSON、hash、遥测和回执 JSON。
- `MqttService.h/.cpp`：PubSubClient、Wi-Fi 门控、订阅和队列收发。
- `ModuleCommandDispatcher.h/.cpp`：模块命令校验及执行。
- `RgbLedMatrix.*`、`StepperMotor.*`：补齐 MQTT 所需控制 API。
- `iot-controller.ino`：调度与上报。
- `docs/MQTT指令格式说明.md`：外部对接说明。

### Task 1: 固定长度 MQTT 队列与配置

**Files:** Create `MqttCredentials.h`, `MqttMessageQueue.h`, `MqttMessageQueue.cpp`, `tests/mqtt_message_queue_test/mqtt_message_queue_test.ino`.

**Produces:**

```cpp
struct MqttMessage { char topic[40]; char payload[385]; bool isReply; };
class MqttMessageQueue {
 public:
  bool push(const MqttMessage& message);
  bool pushReply(const MqttMessage& message);
  bool pop(MqttMessage* message);
  size_t size() const;
};
```

- [ ] **Step 1: Write the failing FIFO/priority test**

```cpp
MqttMessageQueue queue;
for (uint8_t index = 1; index <= 16; ++index) {
  MqttMessage telemetry{"LoTC2S/", "", false};
  snprintf(telemetry.payload, sizeof(telemetry.payload), "telemetry-%u", index);
  assert(queue.push(telemetry));
}
assert(queue.pushReply({"LoTC2S/", "reply", true}));
MqttMessage item{};
assert(queue.pop(&item) && strcmp(item.payload, "telemetry-2") == 0);
assert(queue.pop(&item) && strcmp(item.payload, "reply") == 0);
```

- [ ] **Step 2: Run the test before implementation**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_queue_test`

Expected: failure because `MqttMessageQueue` is absent.

- [ ] **Step 3: Implement ring-buffer behavior**

Use a 16-slot ring buffer. Ordinary `push` rejects when full. `pushReply` first removes the oldest non-reply telemetry; only rejects when all queued entries are replies.

- [ ] **Step 4: Re-run the test**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_queue_test`

Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add MqttCredentials.h MqttMessageQueue.h MqttMessageQueue.cpp tests/mqtt_message_queue_test
git commit -m "feat: add mqtt message queue"
```

### Task 2: 命令 JSON、hash、回执与遥测协议

**Files:** Create `MqttMessageProtocol.h`, `MqttMessageProtocol.cpp`, `tests/mqtt_message_protocol_test/mqtt_message_protocol_test.ino`.

**Produces:**

```cpp
enum class MqttCommandKind { kRelay, kRgb, kServo, kStepperMove, kStepperStop, kInputGet, kInvalid };
struct MqttCommand { MqttCommandKind kind; char hash[9]; uint8_t channel; int32_t value1; int32_t value2; int32_t value3; int32_t value4; };
MqttParseResult parseCommand(const char* json, MqttCommand* output);
void generateHash(uint32_t sequence, uint32_t nowMs, char output[9]);
bool buildReply(const MqttCommand&, bool ok, const char* value, MqttMessage* output);
bool buildInputReply(const MqttCommand&, const EnvironmentData&, MqttMessage* output);
bool buildEnvironmentTelemetry(const EnvironmentData&, char hash[9], MqttMessage* output);
```

- [ ] **Step 1: Write failing protocol tests**

```cpp
MqttCommand command{};
assert(parseCommand("{\"hash\":\"8f29c104\",\"module\":\"relay\",\"channel\":1,\"action\":\"on\"}", &command) == MqttParseResult::kOk);
assert(command.kind == MqttCommandKind::kRelay && command.channel == 1 && command.value1 == 1);
assert(parseCommand("{\"module\":\"servo\",\"action\":\"set\",\"params\":{\"angle\":181}}", &command) == MqttParseResult::kInvalidParams);
```

- [ ] **Step 2: Run the test before implementation**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_protocol_test`

Expected: failure because the protocol API is absent.

- [ ] **Step 3: Implement strict parsing and builders**

Use ArduinoJson. Validate relay channel 1/2; RGB r/g/b 0–255 plus brightness 0–25; servo angle 0–180; stepper turns `>0`, direction `cw/ccw`, RPM 1–15. Require an 8-hex hash when supplied, otherwise make an FNV-1a hash from sequence and milliseconds.

- [ ] **Step 4: Re-run protocol test and commit**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_protocol_test`

Expected: exit 0.

```bash
git add MqttMessageProtocol.h MqttMessageProtocol.cpp tests/mqtt_message_protocol_test
git commit -m "feat: add mqtt command protocol"
```

### Task 3: Wi-Fi 门控的 MQTT 服务

**Files:** Create `MqttService.h`, `MqttService.cpp`, `tests/mqtt_service_state_test/mqtt_service_state_test.ino`; modify `WiFiDataSource.cpp`.

**Produces:**

```cpp
class MqttService {
 public:
  void begin();
  void poll(uint32_t nowMs, bool wifiConnected);
  bool takeIncoming(MqttMessage* message);
  bool enqueue(const MqttMessage& message);
  bool enqueueReply(const MqttMessage& message);
  bool connected() const;
};
```

- [ ] **Step 1: Write failing reconnect-state test**

```cpp
MqttConnectionState state;
assert(!state.shouldAttempt(0, false));
assert(state.shouldAttempt(0, true));
state.recordAttempt(0);
assert(!state.shouldAttempt(4999, true));
assert(state.shouldAttempt(5000, true));
```

- [ ] **Step 2: Run before implementation**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_service_state_test`

Expected: failure because `MqttConnectionState` is absent.

- [ ] **Step 3: Implement transport**

Use `WiFiClient` and PubSubClient with `setBufferSize(512)`. When Wi-Fi is absent, disconnect MQTT and skip attempts. When Wi-Fi exists, retry every 5000 ms using `MqttCredentials`; after connect subscribe to `LoTS2C/`. Callback copies topic/payload into the 8-entry incoming queue. `poll` calls `client.loop()` and publishes one outgoing queue item. Display the configured server host through `WiFiDataSource::readNetwork()`.

- [ ] **Step 4: Verify and commit**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_service_state_test`

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 .`

Expected: both exit 0.

```bash
git add MqttCredentials.h MqttService.h MqttService.cpp WiFiDataSource.cpp tests/mqtt_service_state_test
git commit -m "feat: add queued mqtt transport"
```

### Task 4: 外设命令分发

**Files:** Create `ModuleCommandDispatcher.h`, `ModuleCommandDispatcher.cpp`, `tests/module_command_dispatcher_test/module_command_dispatcher_test.ino`; modify `RgbLedMatrix.h/.cpp`, `StepperMotor.h/.cpp`.

**Produces:**

```cpp
struct CommandExecutionResult { bool ok; const char* stateOrError; };
CommandExecutionResult dispatch(const MqttCommand& command, const EnvironmentData& snapshot);
void RgbLedMatrix::fillColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);
uint32_t StepperMotor::stepIntervalUsForRpm(uint8_t rpm);
```

- [ ] **Step 1: Write failing dispatcher test**

```cpp
assert(dispatcher.dispatch(relayOn, data).ok);
assert(relay.isOn(1));
assert(!dispatcher.dispatch(servoAt181, data).ok);
assert(strcmp(dispatcher.dispatch(inputGet, data).stateOrError, "input") == 0);
```

- [ ] **Step 2: Run before implementation**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\module_command_dispatcher_test`

Expected: failure because dispatcher is absent.

- [ ] **Step 3: Implement documented controls**

`relay/on|off` calls `RelayController`; `rgb/set` fills all 64 pixels and `clear` clears; `servo/set` calls `setAngle`; `stepper/move` rejects a busy motor, converts signed turns to half-steps and RPM to interval; `stepper/stop` calls `stop(true)`; `input/get` creates a reply from the current snapshot.

- [ ] **Step 4: Verify and commit**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\module_command_dispatcher_test`

Expected: exit 0.

```bash
git add RgbLedMatrix.h RgbLedMatrix.cpp StepperMotor.h StepperMotor.cpp ModuleCommandDispatcher.h ModuleCommandDispatcher.cpp tests/module_command_dispatcher_test
git commit -m "feat: dispatch mqtt module commands"
```

### Task 5: 主循环遥测与最终协议文档

**Files:** Modify `iot-controller.ino`, `README.md`; create `MqttTelemetrySchedule.h`, `tests/mqtt_telemetry_schedule_test/mqtt_telemetry_schedule_test.ino`, `docs/MQTT指令格式说明.md`.

**Produces:** 20 秒环境遥测、编码器变化遥测、即时 RFID 遥测，以及命令执行回执。

- [ ] **Step 1: Write failing schedule test**

```cpp
assert(!MqttTelemetrySchedule::environmentDue(19999, 0));
assert(MqttTelemetrySchedule::environmentDue(20000, 0));
assert(MqttTelemetrySchedule::encoderChanged(4, false, 3, false));
assert(!MqttTelemetrySchedule::encoderChanged(4, false, 4, false));
```

- [ ] **Step 2: Run before implementation**

Run: `arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_telemetry_schedule_test`

Expected: failure because the schedule helper is absent.

- [ ] **Step 3: Integrate without blocking**

Call `mqttService.poll(nowMs, wifiConnected)` every loop. Drain incoming FIFO in order, dispatch one command at a time, and enqueue a matching reply. Enqueue environment telemetry at 20000 ms; compare encoder position/press state for changes; enqueue RFID after a new UID. Do not enqueue joystick unless `input/get` was received. Remove the existing automatic 20-degree stepper demo constants and loop block.

- [ ] **Step 4: Write and verify the guide**

Document exact Topics, QoS, command JSON for relay/RGB/servo/stepper/input, reply JSON, 20-second environment telemetry, encoder and RFID telemetry, hash behavior and queue overflow behavior. Every example must use accepted parser values.

Run all:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_queue_test
arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_message_protocol_test
arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_service_state_test
arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\module_command_dispatcher_test
arduino-cli compile --fqbn esp32:esp32:esp32s3 tests\mqtt_telemetry_schedule_test
arduino-cli compile --fqbn esp32:esp32:esp32s3 .
git diff --check
```

Expected: every command exits 0.

- [ ] **Step 5: Commit**

```bash
git add iot-controller.ino MqttTelemetrySchedule.h README.md docs/MQTT指令格式说明.md tests/mqtt_telemetry_schedule_test
git commit -m "feat: integrate mqtt module control"
```
