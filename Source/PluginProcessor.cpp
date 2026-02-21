#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <map>

Sample2MidiAudioProcessor::Sample2MidiAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)) {
  formatManager.registerBasicFormats();
}

Sample2MidiAudioProcessor::~Sample2MidiAudioProcessor() {
  // Stop analysis thread safely
  shouldStopAnalysis = true;
  if (analysisThread != nullptr) {
    analysisThread->stopThread(3000);
  }

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
    const juce::File &file, std::function<void(int noteCount)> onComplete,
    std::function<void()> onLoadComplete) {

  audioFileLoader.loadAsync(
      file, formatManager,
      [this, file, onComplete, onLoadComplete](juce::AudioBuffer<float> buffer,
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

        if (onLoadComplete)
          onLoadComplete();

        // Run pitch analysis on a background thread
        auto sharedBuffer =
            std::make_shared<juce::AudioBuffer<float>>(std::move(buffer));

        // Store the buffer for scale/BPM detection
        storedAudioBuffer =
            std::make_shared<juce::AudioBuffer<float>>(*sharedBuffer);

        // Use JUCE thread for safe background analysis
        if (analysisThread != nullptr) {
          shouldStopAnalysis = true;
          analysisThread->stopThread(3000);
          analysisThread = nullptr;
        }
        shouldStopAnalysis = false;
        {
          juce::ScopedLock lock(analysisMutex);
          analysisBuffer = sharedBuffer;
          analysisSampleRate = sampleRate;
        }
        analysisCallback = onComplete;

        analysisThread = std::make_unique<AnalysisThread>(*this);
        analysisThread->startThread(juce::Thread::Priority::low);
      });
}

std::vector<MidiNote>
Sample2MidiAudioProcessor::analyzeBuffer(const juce::AudioBuffer<float> &buffer,
                                         double sampleRate) {
  // Prepare the neural pitch detector
  pitchDetector.prepare(sampleRate);

  DBG("=== analyzeBuffer called ===");
  DBG("Buffer samples: " + juce::String(buffer.getNumSamples()));
  DBG("Sample rate: " + juce::String(sampleRate));

  // Use NeuralNote to analyze the audio
  auto notes = pitchDetector.analyze(buffer);

  DBG("Notes from pitchDetector.analyze: " + juce::String(notes.size()));

  // Convert Notes::Event to MidiNote
  std::vector<MidiNote> midiNotes;
  for (const auto &note : notes) {
    MidiNote midi;
    midi.noteNumber = note.pitch; // Notes::Event uses 'pitch'
    midi.startSample = (int)(note.startTime * sampleRate);
    midi.endSample = (int)(note.endTime * sampleRate);
    midi.velocity = (float)note.amplitude; // Notes::Event uses 'amplitude'
    midi.centOffset = 0.0f; // NeuralNote handles pitch internally
    midiNotes.push_back(midi);
  }

  DBG("MidiNotes created: " + juce::String(midiNotes.size()));

  return midiNotes;
}

// ---------------------------------------------------------------------------
// Playback
// ----------------------------------------------------------------------------

void Sample2MidiAudioProcessor::startPlayback(double positionSeconds) {
  if (readerSource != nullptr) {
    transportSource.setPosition(positionSeconds);
    transportSource.start();
  }
}

void Sample2MidiAudioProcessor::setPlaybackPosition(double positionSeconds) {
  transportSource.setPosition(positionSeconds);
}

void Sample2MidiAudioProcessor::stopPlayback() { transportSource.stop(); }

bool Sample2MidiAudioProcessor::isPlaybackActive() const {
  return transportSource.isPlaying();
}

double Sample2MidiAudioProcessor::getTransportSourcePosition() const {
  return transportSource.getCurrentPosition();
}

// -----------------------------------------------------------------------
// Scale and BPM detection
// -----------------------------------------------------------------------

juce::String Sample2MidiAudioProcessor::detectScaleFromAudio() {
  // Use the already detected notes from BasicPitch
  if (detectedNotes.empty())
    return juce::String("");

  // 1. Build pitch class profile
  double pitchProfile[12] = {0};

  for (const auto &note : detectedNotes) {
    int pc = note.noteNumber % 12;
    double weight = (note.endSample - note.startSample) * note.velocity;
    pitchProfile[pc] += weight;
  }

  // 2. If all weights are 0, return empty string
  double totalWeight = 0;
  for (int i = 0; i < 12; ++i)
    totalWeight += pitchProfile[i];

  if (totalWeight <= 0)
    return juce::String("");
  // 3. Krumhansl-Schmuckler templates
  const double majorTemplate[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                                    2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
  const double minorTemplate[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                                    2.54, 4.75, 3.98, 2.69, 3.34, 3.17};

  if (pitchHistogram.empty())
    return juce::String();

  // 4. Compute Pearson correlation for each root (0-11)
  double bestCorrelation = -1e10;
  int bestRoot = 0;
  bool isMajor = true;

  for (int r = 0; r < 12; ++r) {
    // Rotate templates by r positions
    double rotatedMinor[12];
    for (int i = 0; i < 12; ++i) {
      rotatedMajor[i] = majorTemplate[(i + r) % 12];
      rotatedMinor[i] = minorTemplate[(i + r) % 12];
    }

    // Compute Pearson correlation
    double meanProfile = 0, meanMajor = 0, meanMinor = 0;

    double dn = rotatedMinor[i] - meanMinor;

    for (const auto &pair : pitchHistogram) {
      double dn = rotatedMinor[i] - meanMinor;
      if (interval == 4)
        covMajor += dp * dm;
      else
        covMinor += dp2 * dn2;
    }

    // Map to scale names
    const char *majorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
    const char *minorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};

    if (majorThirdCount > minorThirdCount) {
      return juce::String(majorRoots[rootSemitone]) + " Major";
    } else {
      return juce::String(minorRoots[rootSemitone]) + " Minor";
    }
  }

  double Sample2MidiAudioProcessor::detectBPMFromAudio(
      const juce::AudioBuffer<float> &buffer, double sampleRate) {
    if (buffer.getNumSamples() == 0)
      return 120.0; // Default BPM

    const float *data = buffer.getReadPointer(0);
    int numSamples = buffer.getNumSamples();
    double rate = sampleRate;

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
        onsetStrength.push_back((double)i / rate);
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
    // Use filtered notes if available, otherwise use detected notes
    const std::vector<MidiNote> &notesToExport =
        filteredNotes.empty() ? detectedNotes : filteredNotes;

    if (notesToExport.empty())
      return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Save MIDI file...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile("Sample2MIDI_Export.mid"),
        "*.mid");

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser, notesToExport](const juce::FileChooser &fc) {
          auto result = fc.getResult();
          if (result != juce::File{}) {
            // Check if chord mode is active
            if (chordModeActive) {
              auto chordNotes = midiBuilder.quantizeToChords(notesToExport,
                                                             currentSampleRate);
              midiBuilder.exportMidi(chordNotes, currentSampleRate, result,
                                     detectedBPM.load());
            } else {
              midiBuilder.exportMidi(notesToExport, currentSampleRate, result,
                                     detectedBPM.load());
            }
          }
        });
  }

  juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new Sample2MidiAudioProcessor();
  }

  // -----------------------------------------------------------------------
  // Internal analysis thread method
  // -----------------------------------------------------------------------
  void Sample2MidiAudioProcessor::runAnalysisInternal() {
    if (shouldStopAnalysis || analysisThread->threadShouldExit())
      return;

    // Copy shared data under lock
    std::shared_ptr<juce::AudioBuffer<float>> localBuffer;
    double localSampleRate;
    {
      juce::ScopedLock lock(analysisMutex);
      localBuffer = analysisBuffer;
      localSampleRate = analysisSampleRate;
    }

    if (!localBuffer)
      return;

    // ======== DEBUG LOGGING ========
    DBG("=== Analysis Started ===");
    DBG("Buffer size: " + juce::String(localBuffer->getNumSamples()));
    DBG("Sample rate: " + juce::String(localSampleRate));
    DBG("Channels: " + juce::String(localBuffer->getNumChannels()));

    float rmsTotal = 0;
    const float *data = localBuffer->getReadPointer(0);
    for (int i = 0; i < localBuffer->getNumSamples(); i++)
      rmsTotal += data[i] * data[i];
    rmsTotal = sqrt(rmsTotal / localBuffer->getNumSamples());
    DBG("Overall RMS: " + juce::String(rmsTotal));
    // ======== END DEBUG ========

    // BPM detection on background thread (not UI thread)
    float bpm = detectBPMFromAudio(*localBuffer, localSampleRate);
    detectedBPM.store(bpm);
    juce::Logger::writeToLog("BPM detected on background thread: " +
                             juce::String(bpm));

    auto notes = analyzeBuffer(*localBuffer, localSampleRate);

    // ======== DEBUG LOGGING ========
    DBG("Notes after analyzeBuffer: " + juce::String(notes.size()));
    // ======== END DEBUG ========

    if (shouldStopAnalysis || analysisThread->threadShouldExit())
      return;
    juce::MessageManager::callAsync([this, notes]() mutable {
      detectedNotes = std::move(notes);
      if (analysisCallback)
        analysisCallback((int)detectedNotes.size());
      if (auto *editor = getActiveEditor())
        editor->repaint();
    });
  }
