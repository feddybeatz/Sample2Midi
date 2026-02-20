#include "PitchDetector.h"
#include <cmath>
#include <vector>

float PitchDetector::detectPitch(const float *buffer, int numSamples,
                                 double sampleRate) {
  return yinPitch(buffer, numSamples, sampleRate);
}

float PitchDetector::yinPitch(const float *buffer, int bufferSize,
                              double rate) {
  const float threshold = 0.15f;
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
    // No pitch found, find minimum
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
  float betterTau = tauEstimate;
  float s0 = yinBuffer[tauEstimate - 1];
  float s1 = yinBuffer[tauEstimate];
  float s2 = yinBuffer[tauEstimate + 1];
  betterTau += (s2 - s0) / (2 * (2 * s1 - s2 - s0));

  float frequency = rate / betterTau;

  // Convert to MIDI note
  if (frequency > 20.0f && frequency < 5000.0f) {
    float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    return midiNote;
  }

  return -1.0f;
}

std::vector<Note>
PitchDetector::analyze(const juce::AudioBuffer<float> &buffer) {
  std::vector<Note> notes;

  const float *channelData = buffer.getReadPointer(0);
  int numSamples = buffer.getNumSamples();
  double rate = sampleRate > 0 ? sampleRate : buffer.getSampleRate();

  // Process in windows - increased to 8192 for better low frequency
  const int windowSize = 8192;
  const int hopSize = 2048;

  float prevMidiNote = -1.0f;
  float noteStart = 0.0f;
  float lastVelocity = 0.8f;

  // Confirmation buffer for note smoothing (3 consecutive frames)
  std::vector<int> confirmationBuffer;
  int confirmedNote = -1;

  for (int i = 0; i < numSamples - windowSize; i += hopSize) {
    // 1. SILENCE GATE - Skip detection in silent sections
    float rms = 0;
    for (int j = 0; j < windowSize; j++) {
      rms += channelData[i + j] * channelData[i + j];
    }
    rms = std::sqrt(rms / windowSize);

    float midiNote = -1.0f;

    if (rms >= 0.015f) { // Not silent
      midiNote = yinPitch(channelData + i, windowSize, rate);

      // 2. OCTAVE CORRECTION - Fix wrong octave detection
      if (midiNote > 0 && prevMidiNote > 0) {
        while (midiNote - prevMidiNote > 6)
          midiNote -= 12;
        while (prevMidiNote - midiNote > 6)
          midiNote += 12;
      }

      // 3. NOTE SMOOTHING - Require 3 consecutive frames
      if (midiNote > 0) {
        int noteInt = (int)(midiNote + 0.5f);
        confirmationBuffer.push_back(noteInt);
        if (confirmationBuffer.size() > 3) {
          confirmationBuffer.erase(confirmationBuffer.begin());
        }

        // Check if we have 3 consecutive same notes
        if (confirmationBuffer.size() >= 3) {
          if (confirmationBuffer[0] == confirmationBuffer[1] &&
              confirmationBuffer[1] == confirmationBuffer[2]) {
            confirmedNote = confirmationBuffer[0];
          }
        }
      } else {
        confirmationBuffer.clear();
        confirmedNote = -1;
      }
    } else {
      // Silent frame - clear buffer
      confirmationBuffer.clear();
      confirmedNote = -1;
    }

    if (confirmedNote > 0) {
      float currentNote = (float)confirmedNote;

      // Note onset detection
      if (prevMidiNote < 0 || confirmedNote != (int)(prevMidiNote + 0.5f)) {
        if (prevMidiNote > 0) {
          // End previous note
          Note note;
          note.midiNote = (int)(prevMidiNote + 0.5f);
          note.startTime = noteStart;
          note.endTime = (float)i / rate;

          // 4. VELOCITY FROM AMPLITUDE
          note.velocity = std::sqrt(rms) * 127.0f * 6.0f;
          note.velocity = juce::jlimit(0.1f, 1.0f, note.velocity);

          // 5. MINIMUM NOTE LENGTH - 80ms minimum
          if ((note.endTime - note.startTime) >= 0.08f && note.midiNote >= 21 &&
              note.midiNote <= 108) {
            notes.push_back(note);
          }
        }
        // Start new note
        noteStart = (float)i / rate;
      }
      lastVelocity = std::sqrt(rms) * 127.0f * 6.0f;
      lastVelocity = juce::jlimit(0.1f, 1.0f, lastVelocity);
      prevMidiNote = currentNote;
    } else {
      // End current note if there was one
      if (prevMidiNote > 0) {
        Note note;
        note.midiNote = (int)(prevMidiNote + 0.5f);
        note.startTime = noteStart;
        note.endTime = (float)i / rate;
        note.velocity = lastVelocity;

        // 5. MINIMUM NOTE LENGTH - 80ms minimum
        if ((note.endTime - note.startTime) >= 0.08f && note.midiNote >= 21 &&
            note.midiNote <= 108) {
          notes.push_back(note);
        }
      }
      prevMidiNote = -1.0f;
      confirmationBuffer.clear();
    }
  }

  // Don't forget the last note
  if (prevMidiNote > 0 && numSamples > windowSize) {
    Note note;
    note.midiNote = (int)(prevMidiNote + 0.5f);
    note.startTime = noteStart;
    note.endTime = (float)numSamples / rate;
    note.velocity = lastVelocity;

    // 5. MINIMUM NOTE LENGTH - 80ms minimum
    if ((note.endTime - note.startTime) >= 0.08f && note.midiNote >= 21 &&
        note.midiNote <= 108) {
      notes.push_back(note);
    }
  }

  return notes;
}
