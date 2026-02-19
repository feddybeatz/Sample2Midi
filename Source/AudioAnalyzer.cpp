#include "AudioAnalyzer.h"
#include <cmath>

AudioAnalyzer::AudioAnalyzer() {}
AudioAnalyzer::~AudioAnalyzer() {}

std::vector<DetectedNote> AudioAnalyzer::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    std::vector<DetectedNote> notes;
    int windowSize = 2048;
    int hopSize = 512;
    int numSamples = buffer.getNumSamples();
    const float* data = buffer.getReadPointer(0); // Analyze first channel

    int currentNote = -1;
    double noteStartTime = 0;
    float peakVelocity = 0;

    for (int i = 0; i < numSamples - windowSize; i += hopSize)
    {
        float freq = detectPitch(data + i, windowSize, sampleRate);
        if (freq > 0)
        {
            int midi = (int)std::round(12.0 * std::log2(freq / 440.0) + 69.0);
            float vol = buffer.getRMSLevel(0, i, hopSize);

            if (midi != currentNote)
            {
                if (currentNote != -1)
                {
                    notes.push_back({currentNote, noteStartTime, (double)i / sampleRate, peakVelocity});
                }
                currentNote = midi;
                noteStartTime = (double)i / sampleRate;
                peakVelocity = vol;
            }
            else
            {
                peakVelocity = std::max(peakVelocity, vol);
            }
        }
        else if (currentNote != -1)
        {
            notes.push_back({currentNote, noteStartTime, (double)i / sampleRate, peakVelocity});
            currentNote = -1;
        }
    }

    return notes;
}

float AudioAnalyzer::detectPitch(const float* data, int numSamples, double sampleRate)
{
    // Simple Autocorrelation
    int minPeriod = (int)(sampleRate / 1000.0); // 1000Hz max
    int maxPeriod = (int)(sampleRate / 50.0);   // 50Hz min
    
    float maxCorrelation = 0;
    int bestPeriod = -1;

    for (int period = minPeriod; period < maxPeriod; ++period)
    {
        float correlation = 0;
        for (int i = 0; i < numSamples - period; ++i)
        {
            correlation += data[i] * data[i + period];
        }

        if (correlation > maxCorrelation)
        {
            maxCorrelation = correlation;
            bestPeriod = period;
        }
    }

    if (bestPeriod > 0)
        return (float)sampleRate / bestPeriod;

    return 0.0f;
}
