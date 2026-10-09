#include "MicrophoneInputDataSource.h"

namespace {
constexpr uint16_t kNoiseFloorPeakToPeak = 5;
// MAX4466 modules have adjustable gain. This range keeps normal speech
// visible without requiring a near-saturated ADC reading.
constexpr uint16_t kLoudPeakToPeak = 160;
}

MicrophoneInputDataSource::MicrophoneInputDataSource(
    AnalogReadFunction analogReadFunction)
    : analogReadFunction_(analogReadFunction) {}

void MicrophoneInputDataSource::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(kMicrophonePin, ADC_11db);
}

void MicrophoneInputDataSource::poll(uint32_t nowUs) {
  if (nowUs - lastSampleUs_ < kSampleIntervalUs) {
    return;
  }

  lastSampleUs_ = nowUs;
  const uint16_t sample = analogReadFunction_(kMicrophonePin);
  if (sample < minimumSample_) {
    minimumSample_ = sample;
  }
  if (sample > maximumSample_) {
    maximumSample_ = sample;
  }

  ++sampleCount_;
  if (sampleCount_ < kSamplesPerWindow) {
    return;
  }

  const uint8_t measured =
      levelFromPeakToPeak(maximumSample_ - minimumSample_);
  // Hold short peaks long enough for the 500 ms dashboard refresh to see them.
  if (measured >= levelPercent_ ||
      static_cast<int32_t>(nowUs - peakHoldUntilUs_) >= 0) {
    levelPercent_ = measured;
    peakHoldUntilUs_ = nowUs + kPeakHoldUs;
  }
  minimumSample_ = 4095;
  maximumSample_ = 0;
  sampleCount_ = 0;
}

void MicrophoneInputDataSource::readInto(EnvironmentData* data) const {
  if (data != nullptr) {
    data->microphonePercent = static_cast<float>(levelPercent_);
  }
}

uint8_t MicrophoneInputDataSource::levelFromPeakToPeak(uint16_t peakToPeak) {
  if (peakToPeak <= kNoiseFloorPeakToPeak) {
    return 0;
  }
  if (peakToPeak >= kLoudPeakToPeak) {
    return 100;
  }

  return static_cast<uint8_t>(
      ((peakToPeak - kNoiseFloorPeakToPeak) * 100U +
       (kLoudPeakToPeak - kNoiseFloorPeakToPeak) / 2U) /
      (kLoudPeakToPeak - kNoiseFloorPeakToPeak));
}
