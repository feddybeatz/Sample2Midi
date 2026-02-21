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

  // Clear previous notes when loading new sample
  detectedNotes.clear();

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

        // Store the buffer but don't analyze - user must click "Process"
        auto sharedBuffer =
            std::make_shared<juce::AudioBuffer<float>>(std::move(buffer));

        storedAudioBuffer =
            std::make_shared<juce::AudioBuffer<float>>(*sharedBuffer);

        // Store buffer for later processing
        {
          juce::ScopedLock lock(analysisMutex);
          analysisBuffer = sharedBuffer;
          analysisSampleRate = sampleRate;
        }
      });
}

void Sample2MidiAudioProcessor::processSample() {
  // Run analysis on background thread
  if (analysisThread != nullptr) {
    shouldStopAnalysis = true;
    analysisThread->stopThread(3000);
    analysisThread = nullptr;
  }
  shouldStopAnalysis = false;

  analysisThread = std::make_unique<AnalysisThread>(*this);
  analysisThread->startThread(juce::Thread::Priority::low);
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
// MIDI export
// -----------------------------------------------------------------------

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
                                                  currentSampleRate, result,
                                                  120.0f);
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
  double covMajor = 0, covMinor = 0;
  double varProfile = 0, varMajor = 0, varMinor = 0;

  for (int j = 0; j < 12; ++j) {
    double dp = pitchProfile[j] - meanProfile;
    double dm = rotatedMajor[j] - meanMajor;
    double dn = rotatedMinor[j] - meanMinor;

    covMajor += dp * dm;
    covMinor += dp * dn;
    varProfile += dp * dp;
    varMajor += dm * dm;
    varMinor += dn * dn;
  }

  // Pearson correlation
  double corrMajor = 0, corrMinor = 0;
  if (varProfile > 0 && varMajor > 0)
    corrMajor = covMajor / std::sqrt(varProfile * varMajor);
  if (varProfile > 0 && varMinor > 0)
    corrMinor = covMinor / std::sqrt(varProfile * varMinor);

  if (corrMajor > bestCorrelationMajor) {
    bestCorrelationMajor = corrMajor;
    bestRootMajor = r;
  }
  if (corrMinor > bestCorrelationMinor) {
    bestCorrelationMinor = corrMinor;
    bestRootMinor = r;
  }
}

// Map to scale names
const char *majorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                            "F#", "G",  "G#", "A",  "A#", "B"};
const char *minorRoots[] = {"C",  "C#", "D",  "D#", "E",  "F",
                            "F#", "G",  "G#", "A",  "A#", "B"};

if (bestCorrelationMajor >= bestCorrelationMinor) {
  return juce::String(majorRoots[bestRootMajor]) + " Major";
} else {
  return juce::String(minorRoots[bestRootMinor]) + " Minor";
}
}

// -----------------------------------------------------------------------
// Internal analysis thread method
// -----------------------------------------------------------------------
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
                                                  currentSampleRate, result,
                                                  120.0f);
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
