#include <assert.h>

#include "../../MicrophoneInputDataSource.cpp"

namespace {
uint16_t fakeSample = 2000;
uint16_t readFakeSample(uint8_t) { return fakeSample; }

void sampleWindow(MicrophoneInputDataSource& microphone, uint32_t* nowUs,
                  bool sound) {
  for (uint8_t i = 0; i < 32; ++i) {
    fakeSample = sound && (i & 1) ? 2030 : 2000;
    *nowUs += 250;
    microphone.poll(*nowUs);
  }
}
}  // namespace

void setup() {
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(0) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(5) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(30) >= 15);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(160) == 100);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(4095) == 100);

  MicrophoneInputDataSource microphone(readFakeSample);
  EnvironmentData environment = {};
  uint32_t nowUs = 0;
  sampleWindow(microphone, &nowUs, true);
  microphone.readInto(&environment);
  assert(environment.microphonePercent >= 15.0f);

  sampleWindow(microphone, &nowUs, false);
  microphone.readInto(&environment);
  assert(environment.microphonePercent >= 15.0f);

  nowUs += 500000;
  sampleWindow(microphone, &nowUs, false);
  microphone.readInto(&environment);
  assert(environment.microphonePercent == 0.0f);
}

void loop() {}
