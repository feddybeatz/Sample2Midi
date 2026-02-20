#include "PluginProcessor.h"
#include "PluginEditor.h"

Sample2MidiAudioProcessor::Sample2MidiAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)) {
  formatManager.registerBasicFormats();
}

Sample2MidiAudioProcessor::~Sample2MidiAudioProcessor() {
  transportSource.setSource(nullptr);
}

void Sample2MidiAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  currentSampleRate = sampleRate;
  transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void Sample2MidiAudioProcessor::releaseResources() {
  transportSource.releaseResources();
}

void Sample2MidiAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  buffer.clear();

  // Fill output with audio from the transport source (preview playback)
  if (transportSource.isPlaying()) {
    juce::AudioSourceChannelInfo info(buffer);
    transportSource.getNextAudioBlock(info);
  }
}

bool Sample2MidiAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor *Sample2MidiAudioProcessor::createEditor() {
  return new Sample2MidiAudioProcessorEditor(*this);
}

void Sample2MidiAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {}
void Sample2MidiAudioProcessor::setStateInformation(const void *data,
                                                    int sizeInBytes) {}

// ---------------------------------------------------------------------------
// Background load + analysis
// ---------------------------------------------------------------------------

void Sample2MidiAudioProcessor::loadAndAnalyze(
    const juce::File &file, std::function<void(int noteCount)> onComplete) {

  audioFileLoader.loadAsync(
      file, formatManager,
      [this, file, onComplete](juce::AudioBuffer<float> buffer,
                               double sampleRate) {
        // This lambda runs on the message thread.
        if (buffer.getNumSamples() == 0) {
          if (onComplete)
            onComplete(0);
          return;
        }

        currentSampleRate = sampleRate;

        // Set up transport source for preview playback
        // Re-open the file for the reader source (transport needs its own
        // reader)
        auto *reader = formatManager.createReaderFor(file);
        if (reader != nullptr) {
          transportSource.stop();
          transportSource.setSource(nullptr);
          readerSource =
              std::make_unique<juce::AudioFormatReaderSource>(reader, true);
          transportSource.setSource(readerSource.get(), 0, nullptr, sampleRate);
        }

        // Run pitch analysis on a background thread
        auto sharedBuffer =
            std::make_shared<juce::AudioBuffer<float>>(std::move(buffer));

        std::thread([this, sharedBuffer, sampleRate, onComplete]() {
          auto notes = analyzeBuffer(*sharedBuffer, sampleRate);

          juce::MessageManager::callAsync([this, notes, onComplete]() mutable {
            detectedNotes = std::move(notes);

            if (onComplete)
              onComplete((int)detectedNotes.size());

            if (auto *editor = getActiveEditor())
              editor->repaint();
          });
        }).detach();
      });
}

std::vector<MidiNote>
Sample2MidiAudioProcessor::analyzeBuffer(const juce::AudioBuffer<float> &buffer,
                                         double sampleRate) {
  const int windowSize = 4096;
  const int hopSize = 2048;
  const float *data = buffer.getReadPointer(0);
  const int numSamples = buffer.getNumSamples();

  std::vector<int> framePitches;
  std::vector<float> frameAmps;

  for (int i = 0; i < numSamples - windowSize; i += hopSize) {
    float sum = 0.0f;
    for (int j = 0; j < windowSize; ++j) {
      float s = data[i + j];
      sum += s * s;
    }
    float rms = std::sqrt(sum / windowSize);

    if (rms > 0.001f) {
      float freq = pitchDetector.detectPitch(data + i, windowSize, sampleRate);
      if (freq > 0.0f) {
        int midi = (int)std::round(69.0 + 12.0 * std::log2(freq / 440.0));
        framePitches.push_back(midi);
        frameAmps.push_back(rms);
      } else {
        framePitches.push_back(-1);
        frameAmps.push_back(0.0f);
      }
    } else {
      framePitches.push_back(-1);
      frameAmps.push_back(0.0f);
    }
  }

  return midiBuilder.buildNotes(framePitches, frameAmps, hopSize, sampleRate);
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void Sample2MidiAudioProcessor::startPlayback() {
  if (readerSource != nullptr) {
    transportSource.setPosition(0.0);
    transportSource.start();
  }
}

void Sample2MidiAudioProcessor::stopPlayback() {
  transportSource.stop();
  transportSource.setPosition(0.0);
}

bool Sample2MidiAudioProcessor::isPlaybackActive() const {
  return transportSource.isPlaying();
}

// ---------------------------------------------------------------------------
// MIDI export
// ---------------------------------------------------------------------------

void Sample2MidiAudioProcessor::exportMidiToFile() {
  if (detectedNotes.empty())
    return;

  auto chooser = std::make_shared<juce::FileChooser>(
      "Save MIDI file...",
      juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
          .getChildFile("Sample2MIDI_Export.mid"),
      "*.mid");

  chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                           juce::FileBrowserComponent::canSelectFiles |
                           juce::FileBrowserComponent::warnAboutOverwriting,
                       [this, chooser](const juce::FileChooser &fc) {
                         auto result = fc.getResult();
                         if (result != juce::File{}) {
                           midiBuilder.exportMidi(detectedNotes,
                                                  currentSampleRate, result);
                         }
                       });
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new Sample2MidiAudioProcessor();
}
