#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "MidiBuilder.h"

class Sample2MidiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::FileDragAndDropTarget,
                                        public juce::DragAndDropContainer
{
public:
    Sample2MidiAudioProcessorEditor (Sample2MidiAudioProcessor&);
    ~Sample2MidiAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    Sample2MidiAudioProcessor& audioProcessor;
    
    juce::AudioThumbnailCache thumbnailCache { 5 };
    WaveformDisplay waveformDisplay;
    
    juce::TextButton loadButton { "LOAD AUDIO" };
    juce::ComboBox scaleDropdown;
    juce::Slider quantizeSlider;
    juce::ComboBox rangeDropdown;
    juce::ToggleButton pitchBendToggle { "PITCH BEND" };
    juce::ToggleButton chordModeToggle { "CHORD MODE" };
    
    juce::TextButton exportButton { "DRAG MIDI" };
    MidiBuilder midiBuilder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sample2MidiAudioProcessorEditor)
};
