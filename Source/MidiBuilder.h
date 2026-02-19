#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct RawNoteEvent {
    int midiNote;
    double timestamp;
    float velocity;
};

struct CleanNote {
    int midiNote;
    double startTime;
    double endTime;
    float velocity;
};

class MidiBuilder {
public:
    MidiBuilder() = default;
    
    std::vector<CleanNote> assembleNotes(const std::vector<RawNoteEvent>& rawEvents, double sampleRate);
    void exportMidi(const std::vector<CleanNote>& notes, const juce::File& file);
    void performDragDrop(const std::vector<CleanNote>& notes);

private:
    static constexpr double MIN_NOTE_DURATION = 0.06; // 60ms
    static constexpr double NOTE_CONTINUATION_GAP = 0.02; // 20ms
};
