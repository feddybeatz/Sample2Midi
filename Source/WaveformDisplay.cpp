#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formatManager, juce::AudioThumbnailCache& cache)
    : thumbnail(512, formatManager, cache) {
}

void WaveformDisplay::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    
    g.setColour(juce::Colour(0xff1f1f1f));
    g.fillRoundedRectangle(area.toFloat(), 5.0f);

    if (thumbnail.getTotalLength() > 0.0) {
        g.setColour(juce::Colours::orange);
        thumbnail.drawChannels(g, area.reduced(2), 0.0, thumbnail.getTotalLength(), 1.0f);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText("Drag & Drop Audio file here", area, juce::Justification::centred);
    }
}

void WaveformDisplay::setFile(const juce::File& file) {
    thumbnail.setSource(new juce::FileInputSource(file));
    repaint();
}
