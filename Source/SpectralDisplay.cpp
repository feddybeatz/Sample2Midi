#include "SpectralDisplay.h"
#include <algorithm>
#include <cmath>
#include <utility>

SpectralDisplay::SpectralDisplay() {
  magnitudes.resize(128, 0.0f);
  fifo.fill(0);
  scopeData.fill(0);
}

void SpectralDisplay::pushBuffer(const float *data, int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    if (fifoIndex == fftSize) {
      // Copy fifo to fftData
      std::copy(fifo.begin(), fifo.end(), fftData.begin());

      // Apply windowing function
      window.multiplyWithWindowingTable(fftData.data(), fftSize);

      // Perform FFT
      forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

      // Copy magnitude data to scopeData
      for (int j = 0; j < fftSize; j++) {
        scopeData[j] = fftData[j];
      }

      nextFFTBlockReady = true;
      fifoIndex = 0;
    }
    fifo[fifoIndex++] = data[i];
  }
}

void SpectralDisplay::setAudioData(const float *data, int numSamples,
                                   double rate) {
  sampleRate = rate;
  magnitudes.resize(128, 0.0f);
  fifoIndex = 0;
  nextFFTBlockReady = false;
  fifo.fill(0);

  // Push entire buffer through FFT
  pushBuffer(data, numSamples);

  // Copy scopeData to magnitudes for display (use first 128 bins)
  for (int i = 0; i < 128 && i < fftSize; i++) {
    magnitudes[i] = scopeData[i];
  }

  if (nextFFTBlockReady) {
    detectChord();
    nextFFTBlockReady = false;
  }
  repaint();
}

void SpectralDisplay::paint(juce::Graphics &g) {
  auto area = getLocalBounds();

  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRoundedRectangle(area.toFloat(), 8.0f);

  auto barArea = area.reduced(10, 20);
  int numBars = (int)magnitudes.size();
  float barWidth = (float)barArea.getWidth() / numBars;

  float maxMag = 0.001f;
  for (float m : magnitudes) {
    if (m > maxMag)
      maxMag = m;
  }

  // Draw frequency bars in cyan #00E5FF
  for (int i = 0; i < numBars; ++i) {
    float height = (magnitudes[i] / maxMag) * barArea.getHeight();
    height = std::min(height, (float)barArea.getHeight());

    // Use cyan color #00E5FF
    g.setColour(juce::Colour(0xff00e5ff));

    // Add glow effect by drawing twice
    g.setColour(juce::Colour(0x4000e5ff));
    g.fillRect(barArea.getX() + i * barWidth, barArea.getBottom() - height,
               barWidth, height);

    // Main bar
    g.setColour(juce::Colour(0xff00e5ff));
    g.fillRect(barArea.getX() + i * barWidth, barArea.getBottom() - height,
               barWidth - 1, height);
  }

  if (!currentChord.isEmpty()) {
    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawText(currentChord, area.reduced(10, 2), juce::Justification::topLeft);
  }

  g.setColour(juce::Colour(0xff666666));
  g.setFont(juce::Font(juce::FontOptions(9.0f)));

  const char *freqLabels[] = {"50", "100", "200", "500", "1k", "2k", "5k"};
  int labelPositions[] = {1, 2, 4, 10, 20, 40, 100};
  for (int i = 0; i < 7; ++i) {
    float x = barArea.getX() +
              (float)labelPositions[i] / numBars * barArea.getWidth();
    g.drawText(freqLabels[i], (int)x, barArea.getBottom() + 2, 30, 10,
               juce::Justification::centredLeft);
  }
}

void SpectralDisplay::detectChord() {
  std::vector<std::pair<int, float>> peaks;

  // Find peaks in the magnitude spectrum (bins 1-64 for better resolution)
  for (int i = 1; i < 64 && i < (int)magnitudes.size() - 1; ++i) {
    if (magnitudes[i] > magnitudes[i - 1] &&
        magnitudes[i] > magnitudes[i + 1] && magnitudes[i] > 0.05f) {
      // Calculate frequency from bin
      float freq = (float)i * sampleRate / (float)fftSize;
      if (freq > 50.0f && freq < 2000.0f) { // Focus on audible range
        peaks.push_back({i, freq});
      }
    }
  }

  std::sort(peaks.begin(), peaks.end(), [this](const auto &a, const auto &b) {
    return magnitudes[a.first] > magnitudes[b.first];
  });

  if (peaks.empty()) {
    currentChord = juce::String("No chord detected");
    return;
  }

  if (peaks.size() < 3) {
    currentChord = freqToNoteName(peaks[0].second);
    return;
  }

  // Take top 3 peaks for chord detection
  std::vector<int> midiNotes;
  for (int i = 0; i < 3 && i < (int)peaks.size(); ++i) {
    midiNotes.push_back(freqToMIDI(peaks[i].second));
  }

  if (midiNotes.size() >= 2) {
    int root = midiNotes[0] % 12;

    std::vector<int> intervals;
    for (size_t i = 1; i < midiNotes.size(); ++i) {
      intervals.push_back((midiNotes[i] - root + 12) % 12);
    }

    bool has4 =
        std::find(intervals.begin(), intervals.end(), 4) != intervals.end();
    bool has3 =
        std::find(intervals.begin(), intervals.end(), 3) != intervals.end();
    bool has7 =
        std::find(intervals.begin(), intervals.end(), 7) != intervals.end();

    const char *noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                               "F#", "G",  "G#", "A",  "A#", "B"};

    if (has4 && has7) {
      currentChord = juce::String(noteNames[root]) + " Major";
    } else if (has3 && has7) {
      currentChord = juce::String(noteNames[root]) + " Minor";
    } else if (has3 && std::find(intervals.begin(), intervals.end(), 6) !=
                           intervals.end()) {
      currentChord = juce::String(noteNames[root]) + " Dim";
    } else if (has4 && std::find(intervals.begin(), intervals.end(), 8) !=
                           intervals.end()) {
      currentChord = juce::String(noteNames[root]) + " Aug";
    } else if (has4 && has7 &&
               std::find(intervals.begin(), intervals.end(), 10) !=
                   intervals.end()) {
      currentChord = juce::String(noteNames[root]) + "7";
    } else if (has4 && has7 &&
               std::find(intervals.begin(), intervals.end(), 11) !=
                   intervals.end()) {
      currentChord = juce::String(noteNames[root]) + "Maj7";
    } else if (has3 && has7 &&
               std::find(intervals.begin(), intervals.end(), 10) !=
                   intervals.end()) {
      currentChord = juce::String(noteNames[root]) + "m7";
    } else if (has3 && has7 &&
               std::find(intervals.begin(), intervals.end(), 11) !=
                   intervals.end()) {
      currentChord = juce::String(noteNames[root]) + "mMaj7";
    } else {
      // Just show the notes
      currentChord = freqToNoteName(peaks[0].second);
    }
  }
}

juce::String SpectralDisplay::freqToNoteName(float freq) const {
  if (freq <= 0)
    return juce::String("---");

  float midi = 69.0f + 12.0f * std::log2(freq / 440.0f);
  int noteNum = (int)std::round(midi) % 12;

  const char *noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                             "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (int)midi / 12 - 1;

  return juce::String(noteNames[noteNum]) + juce::String(octave);
}

int SpectralDisplay::freqToMIDI(float freq) const {
  if (freq <= 0)
    return 0;
  return (int)std::round(69.0f + 12.0f * std::log2(freq / 440.0f));
}
