#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

struct DetectedNote
{
    int midiNote;
    double startTime;
    double endTime;
    float velocity;
};

class AudioAnalyzer
{
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    std::vector<DetectedNote> analyze(const juce::AudioBuffer<float>& buffer, double sampleRate);

private:
    float detectPitch(const float* data, int numSamples, double sampleRate);
};
