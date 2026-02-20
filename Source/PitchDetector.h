#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

struct Note {
  int midiNote;
  float startTime;
  float endTime;
  float velocity;
};

class PitchDetector {
public:
  PitchDetector() = default;

  void prepare(double sampleRate) { this->sampleRate = sampleRate; }

  // Returns MIDI note number
  float detectPitch(const float *buffer, int numSamples, double sampleRate);

  // Alternative interface that returns notes directly
  std::vector<Note> analyze(const juce::AudioBuffer<float> &buffer);

private:
  double sampleRate = 44100.0;

  // YIN pitch detection
  float yinPitch(const float *buffer, int bufferSize, double rate);
};
