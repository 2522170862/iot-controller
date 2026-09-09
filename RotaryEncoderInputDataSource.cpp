#include "RotaryEncoderInputDataSource.h"

namespace {
constexpr int8_t kTransitionDelta[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};
}

RotaryEncoderInputDataSource::RotaryEncoderInputDataSource(
    DigitalReadFunction digitalReadFunction)
    : digitalReadFunction_(digitalReadFunction) {}

void RotaryEncoderInputDataSource::begin() {
  pinMode(kAPin, INPUT);
  pinMode(kBPin, INPUT);
  pinMode(kButtonPin, INPUT);
  previousState_ = readState();
}

void RotaryEncoderInputDataSource::poll() {
  const uint8_t currentState = readState();
  const uint8_t transition = (previousState_ << 2) | currentState;
  transitionAccumulator_ += kTransitionDelta[transition];
  previousState_ = currentState;

  if (transitionAccumulator_ >= 4) {
    ++position_;
    transitionAccumulator_ = 0;
  } else if (transitionAccumulator_ <= -4) {
    --position_;
    transitionAccumulator_ = 0;
  }
}

void RotaryEncoderInputDataSource::readInto(EnvironmentData* data) const {
  if (data == nullptr) {
    return;
  }

  data->encoderPosition = position_;
  data->encoderPressed = digitalReadFunction_(kButtonPin) == LOW;
}

uint8_t RotaryEncoderInputDataSource::readState() const {
  const uint8_t a = digitalReadFunction_(kAPin) == HIGH ? 0b10 : 0;
  const uint8_t b = digitalReadFunction_(kBPin) == HIGH ? 0b01 : 0;
  return a | b;
}
