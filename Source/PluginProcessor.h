#pragma once
#include "AudioFileLoader.h"
#include "MidiBuilder.h"
#include "PitchDetector.h"
#include "ScaleQuantizer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class Sample2MidiAudioProcessor : public juce::AudioProcessor {
public:
  Sample2MidiAudioProcessor();
  ~Sample2MidiAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override { return "Sample2MIDI"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return true; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int index) override {}
  const juce::String getProgramName(int index) override { return {}; }
  void changeProgramName(int index, const juce::String &newName) override {}

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  // -----------------------------------------------------------------------
  // Audio loading / analysis
  // -----------------------------------------------------------------------

  void loadAndAnalyze(const juce::File &file,
                      std::function<void(int noteCount)> onComplete = nullptr);

  const std::vector<MidiNote> &getDetectedNotes() const {
    return detectedNotes;
  }
  double getCurrentSampleRate() const { return currentSampleRate; }
  juce::AudioFormatManager &getFormatManager() { return formatManager; }

  // -----------------------------------------------------------------------
  // Playback (preview the loaded audio)
  // -----------------------------------------------------------------------

  void startPlayback();
  void stopPlayback();
  bool isPlaybackActive() const;

  // -----------------------------------------------------------------------
  // MIDI export
  // -----------------------------------------------------------------------

  /** Save the detected notes to a .mid file chosen by the user. */
  void exportMidiToFile();

private:
  std::vector<MidiNote> analyzeBuffer(const juce::AudioBuffer<float> &buffer,
                                      double sampleRate);

  juce::AudioFormatManager formatManager;
  std::vector<MidiNote> detectedNotes;
  double currentSampleRate = 44100.0;

  PitchDetector pitchDetector;
  MidiBuilder midiBuilder;
  ScaleQuantizer scaleQuantizer;
  AudioFileLoader audioFileLoader;

  // Playback
  std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
  juce::AudioTransportSource transportSource;
  juce::MixerAudioSource mixer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sample2MidiAudioProcessor)
};
