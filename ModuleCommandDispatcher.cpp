#include "ModuleCommandDispatcher.h"

#include "RelayController.h"
#include "RgbLedMatrix.h"
#include "ServoMotor.h"
#include "StepperMotor.h"

ModuleCommandDispatcher::ModuleCommandDispatcher(RelayController* relay,
                                                   RgbLedMatrix* rgb,
                                                   ServoMotor* servo,
                                                   StepperMotor* stepper)
    : relay_(relay), rgb_(rgb), servo_(servo), stepper_(stepper) {}

CommandExecutionResult ModuleCommandDispatcher::dispatch(const MqttCommand& command) {
  switch (command.kind) {
    case MqttCommandKind::kRelay:
      if (relay_ == nullptr) return {false, "unavailable"};
      relay_->setChannel(command.channel, command.value1 != 0);
      return {true, command.value1 != 0 ? "on" : "off"};
    case MqttCommandKind::kRgbSet:
      if (rgb_ == nullptr) return {false, "unavailable"};
      rgb_->fillColor(command.value1, command.value2, command.value3, command.value4);
      return {true, "set"};
    case MqttCommandKind::kRgbClear:
      if (rgb_ == nullptr) return {false, "unavailable"};
      rgb_->clear();
      return {true, "clear"};
    case MqttCommandKind::kServo:
      if (servo_ == nullptr || !servo_->setAngle(command.value1)) return {false, "unavailable"};
      return {true, "set"};
    case MqttCommandKind::kStepperMove:
      if (stepper_ == nullptr) return {false, "unavailable"};
      if (stepper_->isBusy()) return {false, "busy"};
      stepper_->moveSteps(static_cast<int32_t>(command.value1 * StepperMotor::kHalfStepsPerRevolution / 1000) * command.value2,
                          StepperMotor::stepIntervalUsForRpm(command.value3));
      return {true, "moving"};
    case MqttCommandKind::kStepperStop:
      if (stepper_ == nullptr) return {false, "unavailable"};
      stepper_->stop(true);
      return {true, "stopped"};
    case MqttCommandKind::kInputGet:
      return {true, "input"};
    default:
      return {false, "invalid_command"};
  }
}
