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

        // Store the buffer for scale/BPM detection
        storedAudioBuffer =
            std::make_shared<juce::AudioBuffer<float>>(*sharedBuffer);

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

// -----------------------------------------------------------------------
// Scale and BPM detection
// -----------------------------------------------------------------------

std::string Sample2MidiAudioProcessor::detectScaleFromAudio() {
  if (!storedAudioBuffer || storedAudioBuffer->getNumSamples() == 0)
    return "";

  const float *data = storedAudioBuffer->getReadPointer(0);
  int numSamples = storedAudioBuffer->getNumSamples();

  // Collect pitch histogram
  std::map<int, int> pitchHistogram;

  const int windowSize = 4096;
  const int hopSize = 2048;

  for (int i = 0; i < numSamples - windowSize; i += hopSize) {
    float freq =
        pitchDetector.detectPitch(data + i, windowSize, currentSampleRate);
    if (freq > 0) {
      int midi = (int)std::round(69.0 + 12.0 * std::log2(freq / 440.0));
      if (midi >= 0 && midi <= 127) {
        pitchHistogram[midi]++;
      }
    }
  }

  if (pitchHistogram.empty())
    return "";

  // Find the most common pitch (root note)
  int rootNote = 0;
  int maxCount = 0;
  for (const auto &pair : pitchHistogram) {
    if (pair.second > maxCount) {
      maxCount = pair.second;
      rootNote = pair.first;
    }
  }

  // Determine if major or minor based on intervals
  int rootSemitone = rootNote % 12;

  // Count occurrences of major/minor third intervals
  int majorThirdCount = 0; // 4 semitones above root
  int minorThirdCount = 0; // 3 semitones above root

  for (const auto &pair : pitchHistogram) {
    int semitone = pair.first % 12;
    int interval = (semitone - rootSemitone + 12) % 12;
    if (interval == 4)
      majorThirdCount += pair.second;
    if (interval == 3)
      minorThirdCount += pair.second;
  }

  // Map to scale names
  const char *majorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                              "F#", "G",  "G#", "A",  "A#", "B"};
  const char *minorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                              "F#", "G",  "G#", "A",  "A#", "B"};

  if (majorThirdCount > minorThirdCount) {
    return std::string(majorRoots[rootSemitone]) + " Major";
  } else {
    return std::string(minorRoots[rootSemitone]) + " Minor";
  }
}

double Sample2MidiAudioProcessor::detectBPMFromAudio() {
  if (!storedAudioBuffer || storedAudioBuffer->getNumSamples() == 0)
    return 120.0; // Default BPM

  const float *data = storedAudioBuffer->getReadPointer(0);
  int numSamples = storedAudioBuffer->getNumSamples();

  // Simple onset detection using energy difference
  const int blockSize = 1024;
  std::vector<double> onsetStrength;

  for (int i = blockSize; i < numSamples - blockSize; i += blockSize) {
    double energy = 0;
    for (int j = 0; j < blockSize; ++j) {
      energy += data[i + j] * data[i + j];
    }
    energy = std::sqrt(energy / blockSize);

    // Compare with previous block
    double prevEnergy = 0;
    for (int j = 0; j < blockSize; ++j) {
      prevEnergy += data[i - blockSize + j] * data[i - blockSize + j];
    }
    prevEnergy = std::sqrt(prevEnergy / blockSize);

    // Onset if energy increased significantly
    if (energy > prevEnergy * 1.5) {
      onsetStrength.push_back((double)i / currentSampleRate);
    }
  }

  if (onsetStrength.size() < 4)
    return 120.0; // Not enough onsets detected

  // Find the most common interval between onsets
  std::map<double, int> intervalHistogram;

  for (size_t i = 1; i < onsetStrength.size(); ++i) {
    double interval = onsetStrength[i] - onsetStrength[i - 1];
    // Round to nearest common BPM interval
    double bpm = 60.0 / interval;

    // Quantize to common BPM values
    bpm = std::round(bpm / 5.0) * 5.0;
    bpm = std::clamp(bpm, 60.0, 200.0);

    intervalHistogram[bpm]++;
  }

  // Find most common BPM
  double detectedBPM = 120.0;
  int maxCount = 0;
  for (const auto &pair : intervalHistogram) {
    if (pair.second > maxCount) {
      maxCount = pair.second;
      detectedBPM = pair.first;
    }
  }

  return detectedBPM;
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
