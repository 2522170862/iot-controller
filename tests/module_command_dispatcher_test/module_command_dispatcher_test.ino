#include <assert.h>

#include <Adafruit_NeoPixel.h>

#include "../../RelayController.cpp"
#include "../../MqttMessageProtocol.cpp"
#include "../../RgbLedMatrix.cpp"
#include "../../ServoMotor.cpp"
#include "../../StepperMotor.cpp"
#include "../../ModuleCommandDispatcher.cpp"

namespace {
uint8_t levels[64] = {};
void fakePinMode(uint8_t, uint8_t) {}
void fakeDigitalWrite(uint8_t pin, uint8_t level) { levels[pin] = level; }
}

void setup() {
  RelayController relay(4, 5, fakePinMode, fakeDigitalWrite);
  relay.begin();
  ModuleCommandDispatcher dispatcher(&relay, nullptr, nullptr, nullptr);
  MqttCommand command = {};
  command.kind = MqttCommandKind::kRelay;
  command.channel = 1;
  command.value1 = 1;
  const CommandExecutionResult result = dispatcher.dispatch(command);
  assert(result.ok);
  assert(relay.isOn(1));
  assert(levels[4] == LOW);
}

void loop() {}
