#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

class WaveformDisplay : public juce::Component {
public:
    WaveformDisplay(juce::AudioFormatManager& formatManager, juce::AudioThumbnailCache& cache);
    
    void paint(juce::Graphics& g) override;
    void setFile(const juce::File& file);
    
    double getTotalLength() const { return thumbnail.getTotalLength(); }

private:
    juce::AudioThumbnail thumbnail;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
