#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioAnalyzer.h"

class MidiGenerator
{
public:
    static juce::MidiMessageSequence generateSequence(const std::vector<DetectedNote>& notes);
    static void exportToMidiFile(const std::vector<DetectedNote>& notes, const juce::File& file);
};
