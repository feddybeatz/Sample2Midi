#pragma once
#include <vector>

class PitchDetector {
public:
  PitchDetector() = default;

  // Returns detected MIDI note number, or -1 if no pitch found
  float detectPitch(const float *buffer, int numSamples, double sampleRate);

private:
  static constexpr float MPM_THRESHOLD = 0.8f;
};
