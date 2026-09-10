#include <assert.h>

#include "../../MicrophoneInputDataSource.cpp"

void setup() {
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(0) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(25) == 0);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(613) == 50);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(1200) == 100);
  assert(MicrophoneInputDataSource::levelFromPeakToPeak(4095) == 100);
}

void loop() {}
