#pragma once

#include "BasicPitch.h"
#include "Notes.h"
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>


class PitchDetector {
public:
  PitchDetector() {
    // Initialize BasicPitch with default parameters
    // Note sensitivity: 0.7 (higher = more notes)
    // Split sensitivity: 0.5 (higher = more note splitting)
    // Min note duration: 60ms
    basicPitch.setParameters(0.7f, 0.5f, 60.0f);
  }

  void prepare(double sampleRate) { this->sampleRate = sampleRate; }

  // Returns vector of Notes::Event from BasicPitch (2-arg version)
  std::vector<Notes::Event> analyze(const juce::AudioBuffer<float> &buffer,
                                    double sampleRate);

  // Compatibility: single-arg version uses stored sampleRate
  std::vector<Notes::Event> analyze(const juce::AudioBuffer<float> &buffer) {
    return analyze(buffer, sampleRate);
  }

  // Compatibility: detectPitch for scale detection (returns -1 if no pitch)
  float detectPitch(const float *buffer, int numSamples, double sampleRate);

  // Convert Notes::Event to simpler Note struct for compatibility
  struct Note {
    int midiNote;
    float startTime;
    float endTime;
    float velocity;
  };

  std::vector<Note> analyzeSimple(const juce::AudioBuffer<float> &buffer,
                                  double sampleRate);

private:
  double sampleRate = 44100.0;
  BasicPitch basicPitch;

  // Convert audio buffer to mono and resample to 22050 Hz
  std::vector<float> prepareAudio(const juce::AudioBuffer<float> &buffer,
                                  double sourceSampleRate);
};
