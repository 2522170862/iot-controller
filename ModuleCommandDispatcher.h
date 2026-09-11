#pragma once

#include "MqttMessageProtocol.h"

class RelayController;
class RgbLedMatrix;
class ServoMotor;
class StepperMotor;

struct CommandExecutionResult {
  bool ok;
  const char* stateOrError;
};

class ModuleCommandDispatcher {
 public:
  ModuleCommandDispatcher(RelayController* relay, RgbLedMatrix* rgb,
                          ServoMotor* servo, StepperMotor* stepper);
  CommandExecutionResult dispatch(const MqttCommand& command);

 private:
  RelayController* relay_;
  RgbLedMatrix* rgb_;
  ServoMotor* servo_;
  StepperMotor* stepper_;
};
