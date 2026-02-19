#include "PitchDetector.h"
#include <cmath>
#include <numeric>
#include <algorithm>

float PitchDetector::detectPitch(const float* buffer, int numSamples, double sampleRate) {
    return yinAlgorithm(buffer, numSamples, sampleRate);
}

float PitchDetector::yinAlgorithm(const float* buffer, int numSamples, double sampleRate) {
    int tauMax = numSamples / 2;
    std::vector<float> yinBuffer(tauMax, 0.0f);

    // Step 1: Difference Function
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

    // Step 3: Absolute Threshold
    int tauEstimate = -1;
    for (int tau = 1; tau < tauMax; ++tau) {
        if (yinBuffer[tau] < YIN_THRESHOLD) {
            tauEstimate = tau;
            // Find local minimum
            while (tau + 1 < tauMax && yinBuffer[tau + 1] < yinBuffer[tau]) {
                tau++;
                tauEstimate = tau;
            }
            break;
        }
    }

    if (tauEstimate == -1) return -1.0f;

    // Step 4: Parabolic Interpolation
    float refinedTau = parabolicInterpolation(yinBuffer, tauEstimate);
    return (float)sampleRate / refinedTau;
}

float PitchDetector::parabolicInterpolation(const std::vector<float>& yinBuffer, int tauEstimate) {
    if (tauEstimate > 0 && tauEstimate < (int)yinBuffer.size() - 1) {
        float s0 = yinBuffer[tauEstimate - 1];
        float s1 = yinBuffer[tauEstimate];
        float s2 = yinBuffer[tauEstimate + 1];
        
        float denominator = 2.0f * (s2 - 2.0f * s1 + s0);
        if (std::abs(denominator) > 1e-6) {
            return (float)tauEstimate + (s0 - s2) / denominator;
        }
    }
    return (float)tauEstimate;
}
