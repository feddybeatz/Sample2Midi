#pragma once
#include <vector>

class PitchDetector {
public:
    PitchDetector() = default;
    
    // Returns detected frequency in Hz, or -1 if no pitch found
    float detectPitch(const float* buffer, int numSamples, double sampleRate);

private:
    float yinAlgorithm(const float* buffer, int numSamples, double sampleRate);
    float parabolicInterpolation(const std::vector<float>& yinBuffer, int tauEstimate);

    static constexpr float YIN_THRESHOLD = 0.15f; 
};
