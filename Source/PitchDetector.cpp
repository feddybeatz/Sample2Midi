#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>


float PitchDetector::detectPitch(const float *buffer, int numSamples,
                                 double sampleRate) {
  // Step 1: Normalized Square Difference Function (NSDF)
  int n = numSamples;
  std::vector<float> nsdf(n, 0.0f);

  for (int tau = 0; tau < n; tau++) {
    float acf = 0.0f, m = 0.0f;
    for (int i = 0; i < n - tau; i++) {
      acf += buffer[i] * buffer[i + tau];
      m += buffer[i] * buffer[i] + buffer[i + tau] * buffer[i + tau];
    }
    nsdf[tau] = (m == 0.0f) ? 0.0f : 2.0f * acf / m;
  }

  // Step 2: Find key maximum after first zero crossing
  std::vector<int> maxPositions;
  int pos = 0;
  while (pos < n - 1 && nsdf[pos] > 0)
    pos++; // skip to first zero
  while (pos < n - 1 && nsdf[pos] <= 0)
    pos++; // skip negative region

  float curMax = 0.0f;
  int curMaxPos = pos;

  for (int i = pos; i < n - 1; i++) {
    if (nsdf[i] > nsdf[i - 1] && nsdf[i] >= nsdf[i + 1]) {
      if (nsdf[i] > curMax) {
        curMax = nsdf[i];
        curMaxPos = i;
      }
    }
    if (nsdf[i] < 0 && curMax > 0) {
      maxPositions.push_back(curMaxPos);
      curMax = 0.0f;
    }
  }

  if (maxPositions.empty())
    return -1.0f;

  // Step 3: Pick highest peak above threshold * globalMax
  float globalMax = *std::max_element(nsdf.begin(), nsdf.end());
  float threshold = 0.8f;

  int bestTau = -1;
  for (int mp : maxPositions) {
    if (nsdf[mp] >= threshold * globalMax) {
      bestTau = mp;
      break;
    }
  }

  if (bestTau < 1 || bestTau >= n - 1)
    return -1.0f;

  // Step 4: Parabolic interpolation for sub-sample accuracy
  float s0 = nsdf[bestTau - 1];
  float s1 = nsdf[bestTau];
  float s2 = nsdf[bestTau + 1];
  float refinedTau = bestTau + (s2 - s0) / (2.0f * (2.0f * s1 - s2 - s0));

  if (refinedTau <= 0)
    return -1.0f;

  // Step 5: Convert to MIDI note
  float freq = (float)sampleRate / refinedTau;
  if (freq < 50.0f || freq > 2000.0f)
    return -1.0f; // outside musical range

  float midiNote = 69.0f + 12.0f * log2f(freq / 440.0f);
  return midiNote;
}
