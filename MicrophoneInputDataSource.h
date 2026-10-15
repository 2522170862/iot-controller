#pragma once

#include <Arduino.h>

#include "DashboardTypes.h"
#include "PeripheralPins.h"

class MicrophoneInputDataSource {
 public:
  using AnalogReadFunction = uint16_t (*)(uint8_t pin);

  static constexpr uint8_t kMicrophonePin = PeripheralPins::kMicrophone;

  explicit MicrophoneInputDataSource(
      AnalogReadFunction analogReadFunction = analogRead);

  void begin();
  void poll(uint32_t nowUs);
  void readInto(EnvironmentData* data) const;

  static uint8_t levelFromPeakToPeak(uint16_t peakToPeak,
                                   uint16_t noiseFloor = 20);

 private:
  static constexpr uint32_t kSampleIntervalUs = 250;
  static constexpr uint8_t kSamplesPerWindow = 32;
  static constexpr uint32_t kPeakHoldUs = 500000;

  AnalogReadFunction analogReadFunction_;
  uint16_t minimumSample_ = 4095;
  uint16_t maximumSample_ = 0;
  uint8_t sampleCount_ = 0;
  uint8_t levelPercent_ = 0;
  uint32_t lastSampleUs_ = 0;
  uint32_t peakHoldUntilUs_ = 0;
  bool calibrated_ = false;
  uint32_t calibrationStartedUs_ = 0;
  uint32_t calibrationSum_ = 0;
  uint32_t calibrationWindows_ = 0;
  uint16_t noiseFloor_ = 20;
  static constexpr uint32_t kSettlingUs = 1000000;
  static constexpr uint32_t kCalibrationUs = 2000000;
};
