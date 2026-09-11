#pragma once

#include <Arduino.h>

class RelayController {
 public:
  using PinModeFunction = void (*)(uint8_t pin, uint8_t mode);
  using DigitalWriteFunction = void (*)(uint8_t pin, uint8_t level);

  RelayController(uint8_t relay1Pin, uint8_t relay2Pin,
                  PinModeFunction pinModeFunction,
                  DigitalWriteFunction digitalWriteFunction);

  void begin();
  void setChannel(uint8_t channel, bool on);
  void allOff();
  bool isOn(uint8_t channel) const;

  // MQTT callback usage:
  // relayController.handleMqttCommand(topic, payload, payloadLength);
  bool handleMqttCommand(const char* topic, const uint8_t* payload,
                         size_t payloadLength);

 private:
  bool payloadEquals(const uint8_t* payload, size_t payloadLength,
                     const char* expected) const;

  uint8_t relay1Pin_;
  uint8_t relay2Pin_;
  PinModeFunction pinModeFunction_;
  DigitalWriteFunction digitalWriteFunction_;
  bool relay1On_ = false;
  bool relay2On_ = false;
};
