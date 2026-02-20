#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(juce::AudioFormatManager &formatManager,
                                 juce::AudioThumbnailCache &cache)
    : thumbnail(512, formatManager, cache) {}

void WaveformDisplay::paint(juce::Graphics &g) {
  auto area = getLocalBounds();

  // Background
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRoundedRectangle(area.toFloat(), 8.0f);

  if (thumbnail.getTotalLength() > 0.0) {
    // Main Waveform with subtle glow effect simulation (drawing twice)
    auto waveformArea = area.reduced(10, 20);

    // Glow layer
    g.setColour(juce::Colour(0x4000e5ff));
    thumbnail.drawChannels(g, waveformArea, 0.0, thumbnail.getTotalLength(),
                           1.2f);

    // Main layer
    g.setColour(juce::Colour(0xff00e5ff));
    thumbnail.drawChannels(g, waveformArea, 0.0, thumbnail.getTotalLength(),
                           1.0f);

  } else {
    // Grid lines
    g.setColour(juce::Colour(0xff2a2a2a));
    for (int i = 1; i < 8; ++i) {
      float y = (float)area.getHeight() / 8.0f * (float)i;
      g.drawHorizontalLine((int)y, 0, (float)area.getWidth());
    }

    g.setColour(juce::Colour(0xff999999));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(juce::String::fromUTF8("Drag & Drop Audio file here"), area,
               juce::Justification::centred);
  }
}

void WaveformDisplay::setFile(const juce::File &file) {
  thumbnail.setSource(new juce::FileInputSource(file));
  repaint();
}
