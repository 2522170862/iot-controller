#include <assert.h>

#include "../../MicrophoneInputDataSource.cpp"

namespace {
uint16_t fakeSample = 2000;
uint16_t readFakeSample(uint8_t) { return fakeSample; }

void sampleWindow(MicrophoneInputDataSource& microphone, uint32_t* nowUs,
                  bool sound) {
  for (uint8_t i = 0; i < 32; ++i) {
    fakeSample = sound && (i & 1) ? 2620 : 2000;
    *nowUs += 250;
    microphone.poll(*nowUs);
  }
}
}  // namespace

void setup() {
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(0) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(5) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(20) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(620) == 50);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(1220) == 100);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(200, 200) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(800, 200) == 50);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(4095) == 100);

  MicrophoneInputDataSource microphone(readFakeSample);
  EnvironmentData environment = {};
  microphone.begin();
  uint32_t nowUs = micros() + 1000000;
  for (uint16_t i = 0; i < 300; ++i) {
    sampleWindow(microphone, &nowUs, false);
    microphone.readInto(&environment);
    assert(environment.microphonePercent == 0.0f);
  }
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
