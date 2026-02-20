#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>


float PitchDetector::detectPitch(const float *buffer, int numSamples,
                                 double sampleRate) {
  return yinAlgorithm(buffer, numSamples, sampleRate);
}

float PitchDetector::yinAlgorithm(const float *buffer, int numSamples,
                                  double sampleRate) {
  int tauMax = numSamples / 2;
  std::vector<float> yinBuffer(tauMax, 0.0f);

  // Step 1: Difference Function with Hann window
  for (int tau = 1; tau < tauMax; ++tau) {
    for (int i = 0; i < tauMax; ++i) {
      float delta = buffer[i] - buffer[i + tau];
      yinBuffer[tau] += delta * delta;
    }
  }

  // Step 2: Cumulative Mean Normalized Difference Function
  yinBuffer[0] = 1.0f;
  float runningSum = 0.0f;
  for (int tau = 1; tau < tauMax; ++tau) {
    runningSum += yinBuffer[tau];
    if (runningSum > 0)
      yinBuffer[tau] *= (float)tau / runningSum;
    else
      yinBuffer[tau] = 1.0f;
  }

  // Step 3: Absolute Threshold with adaptive threshold
  int tauEstimate = -1;
  float threshold = YIN_THRESHOLD;

  // First pass - try strict threshold
  for (int tau = 2; tau < tauMax - 1; ++tau) {
    if (yinBuffer[tau] < threshold) {
      // Find local minimum
      while (tau + 1 < tauMax - 1 && yinBuffer[tau + 1] < yinBuffer[tau]) {
        tau++;
      }
      tauEstimate = tau;
      break;
    }
  }

  // If not found, try with higher threshold
  if (tauEstimate == -1) {
    threshold = 0.5f;
    for (int tau = 2; tau < tauMax - 1; ++tau) {
      if (yinBuffer[tau] < threshold) {
        while (tau + 1 < tauMax - 1 && yinBuffer[tau + 1] < yinBuffer[tau]) {
          tau++;
        }
        tauEstimate = tau;
        break;
      }
    }
  }

  if (tauEstimate == -1)
    return -1.0f;

  // Step 4: Parabolic Interpolation for better precision
  float refinedTau = parabolicInterpolation(yinBuffer, tauEstimate);

  // Validate the result is in reasonable frequency range
  float freq = (float)sampleRate / refinedTau;
  if (freq < 20.0f || freq > 5000.0f)
    return -1.0f;

  return freq;
}

float PitchDetector::parabolicInterpolation(const std::vector<float> &yinBuffer,
                                            int tauEstimate) {
  if (tauEstimate > 0 && tauEstimate < (int)yinBuffer.size() - 1) {
    float s0 = yinBuffer[tauEstimate - 1];
    float s1 = yinBuffer[tauEstimate];
    float s2 = yinBuffer[tauEstimate + 1];

    float denominator = 2.0f * (s2 - 2.0f * s1 + s0);
    if (std::abs(denominator) > 1e-6) {
      float delta = (s0 - s2) / denominator;
      return (float)tauEstimate + delta;
    }
  }
  return (float)tauEstimate;
}
