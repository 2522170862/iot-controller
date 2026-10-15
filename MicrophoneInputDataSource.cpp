#include "MicrophoneInputDataSource.h"

namespace {
// Relative MAX9814 signal amplitude, not calibrated sound pressure (dB).
constexpr uint16_t kFullScaleAboveNoise = 1200;
}

MicrophoneInputDataSource::MicrophoneInputDataSource(
    AnalogReadFunction analogReadFunction)
    : analogReadFunction_(analogReadFunction) {}

void MicrophoneInputDataSource::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(kMicrophonePin, ADC_11db);
  minimumSample_ = 4095;
  maximumSample_ = 0;
  sampleCount_ = 0;
  levelPercent_ = 0;
  peakHoldUntilUs_ = 0;
  calibrated_ = false;
  calibrationSum_ = 0;
  calibrationWindows_ = 0;
  noiseFloor_ = 20;
  calibrationStartedUs_ = micros();
  lastSampleUs_ = calibrationStartedUs_;
  Serial.println("MAX9814: settling 1 s, then quiet calibration 2 s");
}

void MicrophoneInputDataSource::poll(uint32_t nowUs) {
  const uint32_t startupElapsedUs = nowUs - calibrationStartedUs_;
  if (!calibrated_ && startupElapsedUs < kSettlingUs) return;
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

  const uint16_t peakToPeak = maximumSample_ - minimumSample_;
  minimumSample_ = 4095;
  maximumSample_ = 0;
  sampleCount_ = 0;

  if (!calibrated_) {
    calibrationSum_ += peakToPeak;
    ++calibrationWindows_;
    if (startupElapsedUs >= kSettlingUs + kCalibrationUs &&
        calibrationWindows_ >= 32) {
      const uint32_t average = calibrationSum_ / calibrationWindows_;
      // Margin suppresses ordinary ADC noise and quiet-room fluctuations.
      uint32_t floor = average + average / 4 + 12;
      if (floor < 20) floor = 20;
      if (floor > 4095) floor = 4095;
      noiseFloor_ = static_cast<uint16_t>(floor);
      calibrated_ = true;
      Serial.printf("MAX9814: calibrated quiet P-P=%lu, noise floor=%u ADC counts\n",
                    static_cast<unsigned long>(average), noiseFloor_);
      if (noiseFloor_ >= 1000) {
        Serial.println("MAX9814: high quiet noise; check OUT, ground and power, then reboot in silence");
      }
    }
    levelPercent_ = 0;
    return;
  }
  const uint8_t measured = levelFromPeakToPeak(peakToPeak, noiseFloor_);
  // Hold short peaks long enough for the 500 ms dashboard refresh to see them.
  if (measured >= levelPercent_ ||
      static_cast<int32_t>(nowUs - peakHoldUntilUs_) >= 0) {
    levelPercent_ = measured;
    peakHoldUntilUs_ = nowUs + kPeakHoldUs;
  }
}

void MicrophoneInputDataSource::readInto(EnvironmentData* data) const {
  if (data != nullptr) {
    data->microphonePercent = static_cast<float>(levelPercent_);
  }
}

uint8_t MicrophoneInputDataSource::levelFromPeakToPeak(uint16_t peakToPeak,
                                                     uint16_t noiseFloor) {
  if (peakToPeak <= noiseFloor) {
    return 0;
  }
  const uint32_t amplitude = peakToPeak - noiseFloor;
  if (amplitude >= kFullScaleAboveNoise) {
    return 100;
  }

  return static_cast<uint8_t>(
      (amplitude * 100U + kFullScaleAboveNoise / 2U) /
      kFullScaleAboveNoise);
}
