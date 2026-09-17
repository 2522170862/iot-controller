#include <assert.h>

#include "../../MqttTelemetrySchedule.h"

static_assert(!MqttTelemetrySchedule::joystickChanged(1020, 2020, 1000, 2000));
static_assert(MqttTelemetrySchedule::joystickChanged(1021, 2000, 1000, 2000));
static_assert(MqttTelemetrySchedule::joystickChanged(1000, 1979, 1000, 2000));

void setup() {
  assert(!MqttTelemetrySchedule::environmentDue(19999, 0));
  assert(MqttTelemetrySchedule::environmentDue(20000, 0));
  assert(MqttTelemetrySchedule::encoderChanged(4, false, 3, false));
  assert(MqttTelemetrySchedule::encoderChanged(4, true, 4, false));
  assert(!MqttTelemetrySchedule::encoderChanged(4, false, 4, false));
}

void loop() {}
