#include "MicrophoneInputDataSource.h"

namespace {
constexpr uint16_t kNoiseFloorPeakToPeak = 25;
constexpr uint16_t kLoudPeakToPeak = 1200;
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

  levelPercent_ = levelFromPeakToPeak(maximumSample_ - minimumSample_);
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
      (peakToPeak - kNoiseFloorPeakToPeak) * 100U /
      (kLoudPeakToPeak - kNoiseFloorPeakToPeak));
}
