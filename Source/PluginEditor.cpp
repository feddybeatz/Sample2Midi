#include "PluginEditor.h"
#include "AudioFileLoader.h"
#include "PluginProcessor.h"


Sample2MidiAudioProcessorEditor::Sample2MidiAudioProcessorEditor(
    Sample2MidiAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      waveformDisplay(p.getFormatManager(), thumbnailCache) {
  // Styling
  getLookAndFeel().setColour(juce::ResizableWindow::backgroundColourId,
                             juce::Colour(0xff121212));
  getLookAndFeel().setColour(juce::TextButton::buttonColourId,
                             juce::Colour(0xff1f1f1f));

  addAndMakeVisible(waveformDisplay);
  addAndMakeVisible(loadButton);
  addAndMakeVisible(scaleDropdown);
  addAndMakeVisible(quantizeSlider);
  addAndMakeVisible(rangeDropdown);
  addAndMakeVisible(pitchBendToggle);
  addAndMakeVisible(chordModeToggle);
  addAndMakeVisible(exportButton);

  loadButton.onClick = [this] {
    AudioFileLoader::browseForFile([this](const juce::File &file) {
      audioProcessor.loadAndAnalyze(file);
      waveformDisplay.setFile(file);
    });
  };

  exportButton.onClick = [this] {
    if (!audioProcessor.getDetectedNotes().empty()) {
      midiBuilder.performDragDrop(audioProcessor.getDetectedNotes());
    }
  };

  scaleDropdown.addItem("C Major", 1);
  scaleDropdown.addItem("C Minor", 2);
  scaleDropdown.addItem("A Minor", 3);
  scaleDropdown.setSelectedId(1);

  rangeDropdown.addItem("Full Range", 1);
  rangeDropdown.setSelectedId(1);

  setSize(700, 450);
}

Sample2MidiAudioProcessorEditor::~Sample2MidiAudioProcessorEditor() {}

void Sample2MidiAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colours::white);
  g.setFont(20.0f);
  g.drawText("Sample2MIDI", getLocalBounds().removeFromTop(40),
             juce::Justification::centred);
}

void Sample2MidiAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(15);
  area.removeFromTop(30);

  auto bottomStrip = area.removeFromBottom(80);
  waveformDisplay.setBounds(area.removeFromTop(area.getHeight() - 20));

  loadButton.setBounds(bottomStrip.removeFromLeft(100).reduced(5));

  auto controls = bottomStrip.removeFromLeft(300);
  scaleDropdown.setBounds(
      controls.removeFromTop(40).removeFromLeft(150).reduced(2));
  rangeDropdown.setBounds(controls.removeFromLeft(150).reduced(2));

  exportButton.setBounds(bottomStrip.reduced(5));
}

bool Sample2MidiAudioProcessorEditor::isInterestedInFileDrag(
    const juce::StringArray &files) {
  return AudioFileLoader::isSupportedFile(files[0]);
}

void Sample2MidiAudioProcessorEditor::filesDropped(
    const juce::StringArray &files, int x, int y) {
  juce::File file(files[0]);
  audioProcessor.loadAndAnalyze(file);
  waveformDisplay.setFile(file);
}
