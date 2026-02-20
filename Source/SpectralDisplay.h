#pragma once
#include <complex>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>


class SpectralDisplay : public juce::Component {
public:
  SpectralDisplay();

  void setAudioData(const float *data, int numSamples, double sampleRate);
  void paint(juce::Graphics &g) override;

  // Get detected chord at current position
  std::string getDetectedChord() const { return currentChord; }
  void setPosition(double pos) {
    currentPosition = pos;
    repaint();
  }

private:
  std::vector<float> spectralData;
  std::vector<float> magnitudes;
  std::string currentChord;
  double currentPosition = 0.0;
  double sampleRate = 44100.0;

  void performFFT();
  void detectChord();
  std::string freqToNoteName(float freq) const;
  int freqToMIDI(float freq) const;
};
