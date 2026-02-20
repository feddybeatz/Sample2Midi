#pragma once
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
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

  // Push audio samples for FFT analysis
  void pushBuffer(const float *data, int numSamples);

private:
  void performFFT();
  void detectChord();
  juce::String freqToNoteName(float freq) const;
  int freqToMIDI(float freq) const;

  // FFT configuration
  static constexpr int fftOrder = 11;
  static constexpr int fftSize = 1 << fftOrder; // 2048

  juce::dsp::FFT forwardFFT{fftOrder};
  juce::dsp::WindowingFunction<float> window{
      fftSize, juce::dsp::WindowingFunction<float>::hann};

  std::array<float, fftSize * 2> fftData;
  std::array<float, fftSize> scopeData;
  std::array<float, fftSize> fifo;
  int fifoIndex = 0;
  bool nextFFTBlockReady = false;

  std::vector<float> magnitudes;
  juce::String currentChord;
  double currentPosition = 0.0;
  double sampleRate = 44100.0;
};
