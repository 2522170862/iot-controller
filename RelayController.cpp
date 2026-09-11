#include "RelayController.h"

#include <cstring>

RelayController::RelayController(uint8_t relay1Pin, uint8_t relay2Pin,
                                 PinModeFunction pinModeFunction,
                                 DigitalWriteFunction digitalWriteFunction)
    : relay1Pin_(relay1Pin),
      relay2Pin_(relay2Pin),
      pinModeFunction_(pinModeFunction),
      digitalWriteFunction_(digitalWriteFunction) {}

void RelayController::begin() {
  // The relay module jumpers are set to Low + Com: LOW energizes a relay.
  digitalWriteFunction_(relay1Pin_, HIGH);
  digitalWriteFunction_(relay2Pin_, HIGH);
  pinModeFunction_(relay1Pin_, OUTPUT);
  pinModeFunction_(relay2Pin_, OUTPUT);
  relay1On_ = false;
  relay2On_ = false;
}

void RelayController::setChannel(uint8_t channel, bool on) {
  if (channel == 1) {
    relay1On_ = on;
    digitalWriteFunction_(relay1Pin_, on ? LOW : HIGH);
  } else if (channel == 2) {
    relay2On_ = on;
    digitalWriteFunction_(relay2Pin_, on ? LOW : HIGH);
  }
}

void RelayController::allOff() {
  setChannel(1, false);
  setChannel(2, false);
}

bool RelayController::isOn(uint8_t channel) const {
  return channel == 1 ? relay1On_ : channel == 2 ? relay2On_ : false;
}

bool RelayController::handleMqttCommand(const char* topic,
                                        const uint8_t* payload,
                                        size_t payloadLength) {
  if (strcmp(topic, "iot-controller/relay/1/set") == 0) {
    if (payloadEquals(payload, payloadLength, "ON")) {
      setChannel(1, true);
      return true;
    }
    if (payloadEquals(payload, payloadLength, "OFF")) {
      setChannel(1, false);
      return true;
    }
  } else if (strcmp(topic, "iot-controller/relay/2/set") == 0) {
    if (payloadEquals(payload, payloadLength, "ON")) {
      setChannel(2, true);
      return true;
    }
    if (payloadEquals(payload, payloadLength, "OFF")) {
      setChannel(2, false);
      return true;
    }
  } else if (strcmp(topic, "iot-controller/relay/all/set") == 0 &&
             payloadEquals(payload, payloadLength, "OFF")) {
    allOff();
    return true;
  }

  return false;
}

bool RelayController::payloadEquals(const uint8_t* payload,
                                    size_t payloadLength,
                                    const char* expected) const {
  const size_t expectedLength = strlen(expected);
  return payloadLength == expectedLength &&
         memcmp(payload, expected, expectedLength) == 0;
}
