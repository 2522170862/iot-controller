#include "DashboardEventPolicy.h"

namespace {
constexpr EnvironmentData sampleData() {
  EnvironmentData data = {};
  data.joystickX = 2048;
  data.joystickY = 2048;
  return data;
}

constexpr bool eventPolicyWorks() {
  DashboardEventPolicy policy;
  EnvironmentData data = sampleData();

  if (policy.detect(data, false, 0) != DashboardEventType::kNone) return false;

  data.joystickX = 2100;
  if (policy.detect(data, false, 100) != DashboardEventType::kNone) return false;
  data.joystickX = 2500;
  if (policy.detect(data, false, 200) != DashboardEventType::kJoystick) return false;
  if (!policy.active(3199) || policy.active(3200)) return false;

  data.encoderPosition = 1;
  if (policy.detect(data, false, 4000) != DashboardEventType::kEncoder) return false;

  data.microphonePercent = 65;
  if (policy.detect(data, false, 5000) != DashboardEventType::kMicrophone) return false;
  data.microphonePercent = 70;
  if (policy.detect(data, false, 5100) != DashboardEventType::kNone) return false;

  if (policy.detect(data, true, 6000) != DashboardEventType::kRfid) return false;
  return policy.type(6000) == DashboardEventType::kRfid;
}
}  // namespace

static_assert(eventPolicyWorks());

int main() { return 0; }
