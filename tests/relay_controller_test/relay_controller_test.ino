#include <Arduino.h>

#include "../../RelayController.cpp"

namespace {

uint8_t pinModes[64];
uint8_t pinLevels[64];

void fakePinMode(uint8_t pin, uint8_t mode) {
  pinModes[pin] = mode;
}

void fakeDigitalWrite(uint8_t pin, uint8_t level) {
  pinLevels[pin] = level;
}

void require(bool condition, const char* message) {
  if (!condition) {
    Serial.println(message);
    while (true) {
      delay(1000);
    }
  }
}

const uint8_t kOn[] = {'O', 'N'};
const uint8_t kOff[] = {'O', 'F', 'F'};
const uint8_t kInvalid[] = {'B', 'A', 'D'};

}  // namespace

void setup() {
  Serial.begin(115200);
  RelayController relays(4, 5, fakePinMode, fakeDigitalWrite);
  relays.begin();
  require(pinLevels[4] == HIGH && pinLevels[5] == HIGH,
          "FAIL: relays must start off");

  require(relays.handleMqttCommand("iot-controller/relay/1/set", kOn,
                                   sizeof(kOn)),
          "FAIL: channel 1 ON command must be accepted");
  require(relays.isOn(1) && pinLevels[4] == LOW,
          "FAIL: channel 1 ON must energize GPIO4");

  require(relays.handleMqttCommand("iot-controller/relay/2/set", kOn,
                                   sizeof(kOn)),
          "FAIL: channel 2 ON command must be accepted");
  require(relays.isOn(2) && pinLevels[5] == LOW,
          "FAIL: channel 2 ON must energize GPIO5");

  require(relays.handleMqttCommand("iot-controller/relay/all/set", kOff,
                                   sizeof(kOff)),
          "FAIL: all OFF command must be accepted");
  require(!relays.isOn(1) && !relays.isOn(2) && pinLevels[4] == HIGH &&
              pinLevels[5] == HIGH,
          "FAIL: all OFF must release both relays");

  require(!relays.handleMqttCommand("iot-controller/relay/1/set", kInvalid,
                                    sizeof(kInvalid)),
          "FAIL: invalid payload must be rejected");
  require(!relays.isOn(1) && !relays.isOn(2),
          "FAIL: invalid payload must not change relay state");

  Serial.println("PASS: RelayController MQTT command handling");
}

void loop() {}
