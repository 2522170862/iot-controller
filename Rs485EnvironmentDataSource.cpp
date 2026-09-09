#include "Rs485EnvironmentDataSource.h"

#include <Arduino.h>

#include "ModbusEnvironmentProtocol.h"

<<<<<<< HEAD
namespace {
HardwareSerial rs485Serial(1);
}

void Rs485EnvironmentDataSource::begin() {
=======
namespace
{
  HardwareSerial rs485Serial(2);
}

void Rs485EnvironmentDataSource::begin()
{
>>>>>>> 9186764a07cbc104909227bad36ea2c508f27e75
  rs485Serial.begin(kBaudRate, SERIAL_8N1, kRxPin, kTxPin);
  resetResponse();
}

void Rs485EnvironmentDataSource::poll(uint32_t nowMs)
{
  if (!requestInFlight_)
  {
    if (nowMs - lastRequestMs_ >= kPollIntervalMs)
    {
      startReadRequest(nowMs);
    }
    return;
  }

  while (rs485Serial.available() > 0 &&
         responseLength_ < sizeof(response_))
  {
    response_[responseLength_++] = static_cast<uint8_t>(rs485Serial.read());
  }

  if (responseLength_ == sizeof(response_))
  {
    EnvironmentData received = environment_;
    if (ModbusEnvironmentProtocol::parseReadAllResponse(
            response_, responseLength_, &received))
    {
      received.microphonePercent = 0.0f;
      received.sensorConnected = true;
      environment_ = received;
    }
    else
    {
      environment_.sensorConnected = false;
    }
    requestInFlight_ = false;
    return;
  }

  if (nowMs - requestStartedMs_ >= kResponseTimeoutMs)
  {
    environment_.sensorConnected = false;
    requestInFlight_ = false;
  }
}

const EnvironmentData &Rs485EnvironmentDataSource::readEnvironment() const
{
  return environment_;
}

void Rs485EnvironmentDataSource::startReadRequest(uint32_t nowMs)
{
  while (rs485Serial.available() > 0)
  {
    rs485Serial.read();
  }
  resetResponse();

  uint8_t request[ModbusEnvironmentProtocol::kReadRequestLength];
  ModbusEnvironmentProtocol::buildReadAllRequest(request);

  rs485Serial.write(request, sizeof(request));
  rs485Serial.flush();

  lastRequestMs_ = nowMs;
  requestStartedMs_ = nowMs;
  requestInFlight_ = true;
}

void Rs485EnvironmentDataSource::resetResponse()
{
  responseLength_ = 0;
}
