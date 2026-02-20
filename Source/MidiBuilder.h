#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct MidiNote {
  int startSample;
  int endSample;
  int noteNumber;
  float velocity;
  float centOffset = 0.0f; // Pitch bend offset in cents
};

class MidiBuilder {
public:
  MidiBuilder() = default;

  std::vector<MidiNote> buildNotes(const std::vector<int> &framePitches,
                                   const std::vector<float> &frameAmps,
                                   int hopSize, double sampleRate);
  void exportMidi(const std::vector<MidiNote> &notes, double sampleRate,
                  const juce::File &file, float bpm = 120.0f);
  void performDragDrop(const std::vector<MidiNote> &notes, double sampleRate);

private:
  static constexpr double MIN_NOTE_DURATION_MS = 30.0;
};
