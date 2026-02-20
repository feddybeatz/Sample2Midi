#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include <vector>


class SpectralDisplay : public juce::Component {
public:
  SpectralDisplay();

  void setAudioData(const float *data, int numSamples, double sampleRate);
  void paint(juce::Graphics &g) override;

  juce::String getDetectedChord() const { return currentChord; }
  void setPosition(double pos) {
    currentPosition = pos;
    repaint();
  }

private:
  void performFFT();
  void detectChord();
  juce::String freqToNoteName(float freq) const;
  int freqToMIDI(float freq) const;

  std::vector<float> magnitudes;
  juce::String currentChord;
  double currentPosition = 0.0;
  double sampleRate = 44100.0;
};
