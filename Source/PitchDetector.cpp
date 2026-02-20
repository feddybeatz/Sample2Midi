#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <vector>

float PitchDetector::detectPitch(const float *buffer, int bufferSize,
                                 double rate) {
  // Simple YIN-like pitch detection for scale detection
  const float threshold = 0.1f;
  const int minPeriod = (int)(rate / 1000.0f); // Max freq 1000Hz
  const int maxPeriod = (int)(rate / 50.0f);   // Min freq 50Hz

  if (bufferSize < maxPeriod * 2)
    return -1.0f;

  std::vector<float> yinBuffer(maxPeriod + 1, 0.0f);

  // Step 1: Difference function
  for (int tau = 0; tau <= maxPeriod; tau++) {
    yinBuffer[tau] = 0.0f;
    for (int i = 0; i < bufferSize - maxPeriod; i++) {
      float delta = buffer[i] - buffer[i + tau];
      yinBuffer[tau] += delta * delta;
    }
  }

  // Step 2: Cumulative mean normalized difference
  yinBuffer[0] = 1.0f;
  float runningSum = 0.0f;
  for (int tau = 1; tau <= maxPeriod; tau++) {
    runningSum += yinBuffer[tau];
    yinBuffer[tau] = yinBuffer[tau] * tau / runningSum;
  }

  // Step 3: Absolute threshold
  int tauEstimate = -1;
  for (int tau = minPeriod; tau <= maxPeriod; tau++) {
    if (yinBuffer[tau] < threshold) {
      while (tau + 1 <= maxPeriod && yinBuffer[tau + 1] < yinBuffer[tau]) {
        tau++;
      }
      tauEstimate = tau;
      break;
    }
  }

  if (tauEstimate == -1) {
    float minVal = yinBuffer[minPeriod];
    tauEstimate = minPeriod;
    for (int tau = minPeriod + 1; tau <= maxPeriod; tau++) {
      if (yinBuffer[tau] < minVal) {
        minVal = yinBuffer[tau];
        tauEstimate = tau;
      }
    }
  }

  if (tauEstimate <= 0 || tauEstimate >= maxPeriod)
    return -1.0f;

  // Step 4: Parabolic interpolation
  float s0 = yinBuffer[tauEstimate - 1];
  float s1 = yinBuffer[tauEstimate];
  float s2 = yinBuffer[tauEstimate + 1];
  float betterTau = tauEstimate + (s2 - s0) / (2.0f * (2.0f * s1 - s2 - s0));

  float frequency = rate / betterTau;

  // Convert to MIDI note
  if (frequency > 20.0f && frequency < 5000.0f) {
    return 69.0f + 12.0f * std::log2(frequency / 440.0f);
  }

  return -1.0f;
}

std::vector<Notes::Event>
PitchDetector::analyze(const juce::AudioBuffer<float> &buffer,
                       double sampleRate) {
  // Prepare audio: convert to mono and resample to 22050 Hz (required by
  // BasicPitch)
  auto preparedAudio = prepareAudio(buffer, sampleRate);

  if (preparedAudio.empty()) {
    return {};
  }

  // Reset BasicPitch for new transcription
  basicPitch.reset();

  // Run transcription
  basicPitch.transcribeToMIDI(preparedAudio.data(), (int)preparedAudio.size());

  // Update MIDI with current parameters
  basicPitch.updateMIDI();

  // Return the note events
  return basicPitch.getNoteEvents();
}

std::vector<PitchDetector::Note>
PitchDetector::analyzeSimple(const juce::AudioBuffer<float> &buffer,
                             double sampleRate) {
  std::vector<Note> notes;

  // Get the neural network note events
  auto events = analyze(buffer, sampleRate);

  // Convert Notes::Event to simpler Note struct
  for (const auto &event : events) {
    Note note;
    note.midiNote = event.pitch;
    note.startTime = (float)event.startTime;
    note.endTime = (float)event.endTime;
    note.velocity = (float)std::min(1.0, event.amplitude * 127.0f / 127.0f);

    // Only include valid MIDI notes
    if (note.midiNote >= 21 && note.midiNote <= 108) {
      notes.push_back(note);
    }
  }

  return notes;
}

std::vector<float>
PitchDetector::prepareAudio(const juce::AudioBuffer<float> &buffer,
                            double sourceSampleRate) {
  std::vector<float> result;

  // Mix to mono
  int numSamples = buffer.getNumSamples();
  std::vector<float> monoBuffer(numSamples);

  if (buffer.getNumChannels() > 1) {
    // Average all channels
    for (int i = 0; i < numSamples; i++) {
      float sum = 0.0f;
      for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
        sum += buffer.getReadPointer(ch)[i];
      }
      monoBuffer[i] = sum / buffer.getNumChannels();
    }
  } else {
    // Single channel - just copy
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples,
              monoBuffer.begin());
  }

  // Resample to 22050 Hz if needed (BasicPitch requires 22050 Hz)
  const int targetSampleRate = 22050;

  if (std::abs(sourceSampleRate - targetSampleRate) < 1.0) {
    // No resampling needed
    return monoBuffer;
  }

  // Simple linear interpolation resampling
  double ratio = sourceSampleRate / (double)targetSampleRate;
  int targetLength = (int)(numSamples / ratio);
  result.resize(targetLength);

  for (int i = 0; i < targetLength; i++) {
    double sourcePos = i * ratio;
    int idx1 = (int)sourcePos;
    int idx2 = std::min(idx1 + 1, numSamples - 1);
    double frac = sourcePos - idx1;

    result[i] =
        (float)(monoBuffer[idx1] * (1.0 - frac) + monoBuffer[idx2] * frac);
  }

  return result;
}
