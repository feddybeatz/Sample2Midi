#include "SpectralDisplay.h"
#include <algorithm>
#include <cmath>


SpectralDisplay::SpectralDisplay() {
  // Initialize with empty data
  magnitudes.resize(128, 0.0f);
}

void SpectralDisplay::setAudioData(const float *data, int numSamples,
                                   double rate) {
  sampleRate = rate;

  // Simple DFT-based spectral analysis (for demonstration)
  // For better performance, use FFT library
  const int numBins = 128;
  magnitudes.resize(numBins, 0.0f);

  if (numSamples < 1024)
    return;

  // Use windowed segments for analysis
  const int windowSize = 4096;
  if (numSamples < windowSize)
    return;

  // Analyze multiple windows and average
  int numWindows = numSamples / windowSize;
  std::vector<float> avgMagnitudes(numBins, 0.0f);

  for (int w = 0; w < numWindows; ++w) {
    const float *window = data + w * windowSize;

    // Simple DFT for each bin
    for (int bin = 0; bin < numBins; ++bin) {
      float freq = (float)bin * sampleRate / windowSize;
      if (freq < 20.0f || freq > 5000.0f)
        continue;

      float real = 0, imag = 0;
      for (int i = 0; i < windowSize; i += 4) { // Skip for performance
        float phase = 2.0f * 3.14159f * freq * i / sampleRate;
        real += window[i] * std::cos(phase);
        imag += window[i] * std::sin(phase);
      }
      avgMagnitudes[bin] += std::sqrt(real * real + imag * imag);
    }
  }

  // Average and normalize
  for (int bin = 0; bin < numBins; ++bin) {
    magnitudes[bin] = avgMagnitudes[bin] / (float)numWindows;
  }

  detectChord();
  repaint();
}

void SpectralDisplay::paint(juce::Graphics &g) {
  auto area = getLocalBounds();

  // Background
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRoundedRectangle(area.toFloat(), 8.0f);

  // Draw frequency bars
  auto barArea = area.reduced(10, 20);
  int numBars = (int)magnitudes.size();
  float barWidth = (float)barArea.getWidth() / numBars;

  // Find max magnitude for normalization
  float maxMag = 0.001f;
  for (float m : magnitudes) {
    if (m > maxMag)
      maxMag = m;
  }

  for (int i = 0; i < numBars; ++i) {
    float height = (magnitudes[i] / maxMag) * barArea.getHeight();
    height = std::min(height, (float)barArea.getHeight());

    // Color based on frequency (bass = cyan, treble = magenta)
    float hue = (float)i / numBars;
    g.setColour(juce::Colour::fromHSV(hue * 0.7f, 0.8f, 0.8f, 1.0f));

    g.fillRect(barArea.getX() + i * barWidth, barArea.getBottom() - height,
               barWidth - 1, height);
  }

  // Draw detected chord
  if (!currentChord.empty()) {
    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawText(currentChord, area.reduced(10, 2), juce::Justification::topLeft);
  }

  // Draw frequency labels
  g.setColour(juce::Colour(0xff666666));
  g.setFont(juce::Font(juce::FontOptions(9.0f)));

  const char *freqLabels[] =
  {\"50\", \"100\", \"200\", \"500\", \"1k\", \"2k\", \"5k\"};
    int labelPositions[] = {1, 2, 4, 10, 20, 40, 100};
  for (int i = 0; i < 7; ++i) {
    float x = barArea.getX() +
              (float)labelPositions[i] / numBars * barArea.getWidth();
    g.drawText(freqLabels[i], (int)x, barArea.getBottom() + 2, 30, 10,
               juce::Justification::centredLeft);
  }
}

void SpectralDisplay::detectChord() {
  // Find prominent frequencies
  std::vector<std::pair<int, float>> peaks;

  for (int i = 1; i < (int)magnitudes.size() - 1; ++i) {
    if (magnitudes[i] > magnitudes[i - 1] &&
        magnitudes[i] > magnitudes[i + 1] && magnitudes[i] > 0.1f) {
      float freq = (float)i * sampleRate / 4096.0f;
      peaks.push_back({i, freq});
    }
  }

  // Sort by magnitude
  std::sort(peaks.begin(), peaks.end(), [this](const auto &a, const auto &b) {
    return magnitudes[a.first] > magnitudes[b.first];
  });

  // Take top 3 peaks
  if (peaks.size() < 3) {
    if (peaks.empty()) {
      currentChord = \"No chord detected\";
    } else {
      currentChord = freqToNoteName(peaks[0].second);
    }
    return;
  }

  // Get MIDI notes for top peaks
  std::vector<int> midiNotes;
  for (int i = 0; i < 3 && i < (int)peaks.size(); ++i) {
    midiNotes.push_back(freqToMIDI(peaks[i].second));
  }

  // Determine chord from intervals
  if (midiNotes.size() >= 2) {
    int root = midiNotes[0] % 12;

    // Calculate intervals from root
    std::vector<int> intervals;
    for (size_t i = 1; i < midiNotes.size(); ++i) {
      intervals.push_back((midiNotes[i] - root + 12) % 12);
    }

    // Common chord intervals
    // Major: 0, 4, 7
    // Minor: 0, 3, 7
    // Diminished: 0, 3, 6
    // Augmented: 0, 4, 8
    // 7th: 0, 4, 7, 10
    // Maj7: 0, 4, 7, 11

    bool has4 =
        std::find(intervals.begin(), intervals.end(), 4) != intervals.end();
    bool has3 =
        std::find(intervals.begin(), intervals.end(), 3) != intervals.end();
    bool has7 =
        std::find(intervals.begin(), intervals.end(), 7) != intervals.end();

    const char *noteNames[] =
    {\"C\", \"C#\", \"D\", \"D#\", \"E\", \"F\", \"F#\", \"G\", \"G#\", \"A\", \"A#\", \"B\"};

      if (has4 && has7){currentChord =
                            std::string(noteNames[root]) + \" Major\";
      } else if (has3 && has7){currentChord =
                                   std::string(noteNames[root]) + \" Minor\";
      } else if (has3 && std::find(intervals.begin(), intervals.end(), 6) !=
                             intervals.end()){
          currentChord = std::string(noteNames[root]) + \" Dim\";
      } else if (has4 && std::find(intervals.begin(), intervals.end(), 8) !=
                             intervals.end()){
          currentChord = std::string(noteNames[root]) + \" Aug\";
      } else if (has4 && has7 &&
                 std::find(intervals.begin(), intervals.end(), 10) !=
                     intervals.end()){currentChord =
                                          std::string(noteNames[root]) + \"7\";
      } else if (has4 && has7 &&
                 std::find(intervals.begin(), intervals.end(), 11) !=
                     intervals.end()){
          currentChord = std::string(noteNames[root]) + \" Maj7\";
      } else if (has3 && has7 &&
                 std::find(intervals.begin(), intervals.end(), 10) !=
                     intervals.end()){
          currentChord = std::string(noteNames[root]) + \" m7\";
      } else {currentChord = freqToNoteName(peaks[0].second);
  }
}
}

std::string SpectralDisplay::freqToNoteName(float freq) const {
  if (freq <= 0)
    return \"--\";

           int midi = freqToMIDI(freq);
  const char *noteNames[] =
  {\"C\", \"C#\", \"D\", \"D#\", \"E\", \"F\", \"F#\", \"G\", \"G#\", \"A\", \"A#\", \"B\"};

    int octave = midi / 12 - 1;
  int note = midi % 12;

  return std::string(noteNames[note]) + std::to_string(octave);
}

int SpectralDisplay::freqToMIDI(float freq) const {
  if (freq <= 0)
    return -1;
  return (int)std::round(69.0 + 12.0 * std::log2(freq / 440.0));
}
