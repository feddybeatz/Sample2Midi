#include "PluginEditor.h"
#include "AudioFileLoader.h"
#include "MidiBuilder.h"
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Sample2MidiAudioProcessorEditor::Sample2MidiAudioProcessorEditor(
    Sample2MidiAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p),
      waveformDisplay(p.getFormatManager(), thumbnailCache) {

  setLookAndFeel(&customLookAndFeel);

  // ---- Waveform ----
  addAndMakeVisible(waveformDisplay);

  // ---- Zoom buttons ----
  zoomInButton.setButtonText(juce::String("+"));
  zoomOutButton.setButtonText(juce::String("-"));
  zoomInButton.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  zoomInButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
  zoomOutButton.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  zoomOutButton.setColour(juce::TextButton::textColourOffId,
                          juce::Colours::white);
  addAndMakeVisible(zoomInButton);
  addAndMakeVisible(zoomOutButton);

  // Zoom button callbacks
  zoomInButton.onClick = [this] {
    double currentZoom = waveformDisplay.getZoom();
    waveformDisplay.setZoom(currentZoom * 1.5);
  };
  zoomOutButton.onClick = [this] {
    double currentZoom = waveformDisplay.getZoom();
    waveformDisplay.setZoom(currentZoom / 1.5);
  };

  // ---- Status bar ----
  statusLabel.setColour(juce::Label::textColourId, Colors::textGray);
  statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
  statusLabel.setText(juce::String("No sample loaded"),
                      juce::dontSendNotification);
  addAndMakeVisible(statusLabel);

  loadButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colours::transparentBlack);
  loadButton.setColour(juce::TextButton::textColourOffId, Colors::accentCyan);
  loadButton.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colours::transparentBlack);
  addAndMakeVisible(loadButton);

  loadButton.onClick = [this] {
    AudioFileLoader::browseForFile([this](const juce::File &file) {
      statusLabel.setText(juce::String("Analyzing..."),
                          juce::dontSendNotification);
      statusLabel.setColour(juce::Label::textColourId, Colors::textGray);
      waveformDisplay.setFile(file);
      hasSample = true;
      dragZone.setVisible(true);
      resized();

      // Update spectral display with audio data
      auto buffer = audioProcessor.getAudioBuffer();
      if (buffer && buffer->getNumSamples() > 0) {
        spectralDisplay.setAudioData(buffer->getReadPointer(0),
                                     buffer->getNumSamples(),
                                     buffer->getSampleRate());
      }

      audioProcessor.loadAndAnalyze(
          file, [this](int noteCount) { updateStatus(noteCount); });
    });
  };

  // ---- Row 1: Scale ----
  {
    juce::StringArray scales = {"C Major", "D Major", "E Major",  "F Major",
                                "G Major", "A Major", "B Major",  "C Minor",
                                "D Minor", "E Minor", "F Minor",  "G Minor",
                                "A Minor", "B Minor", "Chromatic"};
    scaleDropdown.addItemList(scales, 1);
    scaleDropdown.setSelectedId(1);
    addAndMakeVisible(scaleDropdown);
  }

  // Auto-detect button (sparkle icon via Unicode)
  autoDetectButton.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  autoDetectButton.setColour(juce::TextButton::textColourOffId,
                             Colors::accentCyan);
  autoDetectButton.setButtonText(juce::String::fromUTF8("\xe2\x9c\xa8")); // ✨
  addAndMakeVisible(autoDetectButton);

  // Auto-detect button callback - detect scale from audio (background thread)
  autoDetectButton.onClick = [this] {
    autoDetectButton.setEnabled(false);
    autoDetectButton.setButtonText("...");

    juce::Component::SafePointer<Sample2MidiAudioProcessorEditor> safeThis(
        this);

    juce::Thread::launch([safeThis] {
      if (safeThis == nullptr)
        return;

      auto detectedKey = safeThis->audioProcessor.detectScaleFromAudio();

      juce::MessageManager::callAsync([safeThis, detectedKey] {
        if (safeThis == nullptr)
          return;

        if (detectedKey.empty()) {
          safeThis->statusLabel.setText(
              "Could not detect key — load a sample first",
              juce::dontSendNotification);
        } else {
          safeThis->scaleDropdown.setText(detectedKey, juce::sendNotification);
          safeThis->statusLabel.setText("Key detected: " + detectedKey,
                                        juce::dontSendNotification);
        }

        safeThis->autoDetectButton.setEnabled(true);
        safeThis->autoDetectButton.setButtonText(
            juce::String::fromUTF8("\xe2\x9c\xa8"));
      });
    });
    });
};

// ---- Row 1: Quantize ----
quantizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
quantizeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
quantizeSlider.setRange(0, 100, 1);
quantizeSlider.setValue(75);
quantizeSlider.setColour(juce::Slider::thumbColourId, Colors::accentCyan);
addAndMakeVisible(quantizeSlider);

quantizeLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
quantizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
quantizeLabel.setJustificationType(juce::Justification::centredRight);
quantizeLabel.setText(juce::String("75%"), juce::dontSendNotification);
addAndMakeVisible(quantizeLabel);

quantizeSlider.onValueChange = [this] {
  quantizeLabel.setText(juce::String((int)quantizeSlider.getValue()) +
                            juce::String("%"),
                        juce::dontSendNotification);
};

// ---- Row 1: Range ----
rangeDropdown.addItem("Full Range", 1);
rangeDropdown.addItem("C2-C5", 2);
rangeDropdown.addItem("C3-C6", 3);
rangeDropdown.addItem("C4-C7", 4);
rangeDropdown.setSelectedId(1);
addAndMakeVisible(rangeDropdown);

// ---- Row 1: Pitch Bend toggle ----
pitchBendToggle.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
pitchBendToggle.setColour(juce::TextButton::textColourOffId, Colors::textGray);
pitchBendToggle.setColour(juce::TextButton::buttonOnColourId,
                          Colors::accentCyan);
pitchBendToggle.setColour(juce::TextButton::textColourOnId,
                          juce::Colours::black);
pitchBendToggle.setClickingTogglesState(true);
pitchBendToggle.onClick = [this] {
  pitchBendToggle.setButtonText(pitchBendToggle.getToggleState()
                                    ? juce::String("ON")
                                    : juce::String("OFF"));
};
addAndMakeVisible(pitchBendToggle);

// ---- Row 1: Chord Mode toggle ----
chordModeToggle.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
chordModeToggle.setColour(juce::TextButton::textColourOffId, Colors::textGray);
chordModeToggle.setColour(juce::TextButton::buttonOnColourId,
                          Colors::accentCyan);
chordModeToggle.setColour(juce::TextButton::textColourOnId,
                          juce::Colours::black);
chordModeToggle.setClickingTogglesState(true);
chordModeToggle.onClick = [this] {
  bool isChordMode = chordModeToggle.getToggleState();
  chordModeToggle.setButtonText(isChordMode ? juce::String("ON")
                                            : juce::String("OFF"));

  // Enable/disable spectral display
  spectralDisplay.setVisible(isChordMode);
  resized();
};
addAndMakeVisible(chordModeToggle);

// ---- Spectral Display (chord view) ----
addAndMakeVisible(spectralDisplay);
spectralDisplay.setVisible(false);

// ---- Row 2: Play button ----
// Unicode play triangle ▶
playButton.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
playButton.setColour(juce::TextButton::textColourOffId, Colors::accentCyan);
playButton.setColour(juce::TextButton::buttonOnColourId, Colors::accentCyan);
playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
playButton.setClickingTogglesState(true);
playButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6")); // ▶
playButton.onClick = [this] {
  if (playButton.getToggleState()) {
    audioProcessor.startPlayback();
    isPlaying = true;
    startTimer(30); // Update playhead every 30ms
  } else {
    audioProcessor.stopPlayback();
    isPlaying = false;
    stopTimer();
  }
  repaint();
};
addAndMakeVisible(playButton);

// ---- Row 2: Stop button ----
// Unicode stop square ■
stopButton.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
stopButton.setColour(juce::TextButton::textColourOffId, Colors::textGray);
stopButton.setButtonText(juce::String::fromUTF8("\xe2\x96\xa0")); // ■
stopButton.onClick = [this] {
  audioProcessor.stopPlayback();
  isPlaying = false;
  stopTimer();
  playButton.setToggleState(false, juce::dontSendNotification);
  repaint();
};
addAndMakeVisible(stopButton);

// ---- Row 2: Export MIDI button ----
// Opens a Save dialog and writes the .mid file
exportButton.setColour(juce::TextButton::buttonColourId, Colors::accentCyan);
exportButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
exportButton.onClick = [this] { audioProcessor.exportMidiToFile(); };
addAndMakeVisible(exportButton);

// ---- Drag zone ----
// Allows dragging the exported MIDI file directly into FL Studio
dragZone.setVisible(false);
dragZone.onStartDrag = [this] {
  if (!audioProcessor.getDetectedNotes().empty()) {
    // Write temp file then start external drag
    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("Sample2MIDI_Export.mid");
    if (tempFile.existsAsFile())
      tempFile.deleteFile();

    MidiBuilder mb;
    mb.exportMidi(audioProcessor.getDetectedNotes(),
                  audioProcessor.getCurrentSampleRate(), tempFile);

    if (tempFile.existsAsFile()) {
      juce::DragAndDropContainer::performExternalDragDropOfFiles(
          {tempFile.getFullPathName()}, false);
    }
  }
};
addAndMakeVisible(dragZone);

setSize(800, 500);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
Sample2MidiAudioProcessorEditor::~Sample2MidiAudioProcessorEditor() {
  setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
// paint
// ---------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(Colors::background);

  auto bounds = getLocalBounds();

  // ---- Top Bar ----
  auto topArea = bounds.removeFromTop(60);
  g.setColour(Colors::borderSubtle);
  g.drawHorizontalLine(topArea.getBottom() - 1, 0.0f,
                       (float)topArea.getWidth());

  auto topPadded = topArea.reduced(24, 0);

  // Title
  g.setColour(Colors::textWhite);
  g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
  auto titleArea = topPadded.removeFromLeft(160);
  g.drawText(juce::String("Sample2MIDI"), titleArea,
             juce::Justification::centredLeft);

  // Tagline
  g.setColour(Colors::textDarkGray);
  g.setFont(juce::Font(juce::FontOptions(14.0f)));
  auto tagArea = topPadded.removeFromLeft(110);
  g.drawText(juce::String("Audio to MIDI"), tagArea,
             juce::Justification::centredLeft);

  // Badge
  auto badgeArea = topPadded.removeFromRight(120).reduced(0, 14);
  g.setColour(Colors::accentCyan);
  g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
  g.setColour(juce::Colours::black);
  g.setFont(juce::Font(juce::FontOptions(14.0f)));
  g.drawText(juce::String("Feddy Beatz"), badgeArea,
             juce::Justification::centred);

  // ---- Control Bar background ----
  auto controlBgArea =
      getLocalBounds().removeFromBottom(hasSample ? 170 : 120).reduced(16, 0);
  controlBgArea.removeFromBottom(hasSample ? 50 : 0);
  g.setColour(Colors::controlBar);
  g.fillRoundedRectangle(controlBgArea.toFloat(), 8.0f);

  // Row separator line inside control bar
  auto sepY = controlBgArea.getY() + controlBgArea.getHeight() / 2;
  g.setColour(Colors::borderSubtle);
  g.drawHorizontalLine(sepY, (float)controlBgArea.getX() + 16,
                       (float)controlBgArea.getRight() - 16);

  // ---- Labels above controls ----
  g.setFont(juce::Font(juce::FontOptions(12.0f)));
  g.setColour(Colors::textGray);

  // We paint labels in resized() coordinates — use the same layout math
  auto cb = getLocalBounds();
  cb.removeFromTop(60);
  if (hasSample)
    cb.removeFromBottom(50);
  auto ctrlArea = cb.removeFromBottom(120).reduced(16, 0);
  auto innerArea = ctrlArea.reduced(16, 8);
  auto labelRow = innerArea.removeFromTop(innerArea.getHeight() / 2);

  auto paintLabel = [&](juce::Rectangle<int> area, const char *text) {
    g.drawText(juce::String(text), area.withHeight(14).withY(area.getY() - 16),
               juce::Justification::centredLeft);
  };

  paintLabel(labelRow.removeFromLeft(120), "Scale");
  labelRow.removeFromLeft(36 + 16); // auto-detect + gap
  paintLabel(labelRow.removeFromLeft(180), "Quantize");
  labelRow.removeFromLeft(16);
  paintLabel(labelRow.removeFromLeft(110), "Range");
  labelRow.removeFromLeft(16);
  paintLabel(labelRow.removeFromLeft(70), "Pitch Bend");
  labelRow.removeFromLeft(8);
  paintLabel(labelRow.removeFromLeft(80), "Chord Mode");
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(60); // top bar

  // ---- Drag zone (bottom, only when sample loaded) ----
  if (hasSample) {
    auto dragArea = bounds.removeFromBottom(50).reduced(16, 8);
    dragZone.setBounds(dragArea);
  }

  // ---- Control bar (bottom 120px) ----
  auto controlsArea = bounds.removeFromBottom(120).reduced(16, 0);
  auto inner = controlsArea.reduced(16, 8);

  auto row1 = inner.removeFromTop(inner.getHeight() / 2);
  auto row2 = inner;

  // -- Row 1 --
  scaleDropdown.setBounds(row1.removeFromLeft(120).reduced(0, 6));
  autoDetectButton.setBounds(row1.removeFromLeft(36).reduced(2, 6));
  row1.removeFromLeft(16);

  auto quantizeArea = row1.removeFromLeft(180);
  quantizeLabel.setBounds(quantizeArea.removeFromRight(40).reduced(0, 6));
  quantizeSlider.setBounds(quantizeArea.reduced(0, 8));
  row1.removeFromLeft(16);

  rangeDropdown.setBounds(row1.removeFromLeft(110).reduced(0, 6));
  row1.removeFromLeft(16);

  pitchBendToggle.setBounds(row1.removeFromLeft(70).reduced(0, 6));
  row1.removeFromLeft(8);

  chordModeToggle.setBounds(row1.removeFromLeft(80).reduced(0, 6));

  // -- Row 2 --
  playButton.setBounds(row2.removeFromLeft(40).reduced(0, 4));
  row2.removeFromLeft(8);
  stopButton.setBounds(row2.removeFromLeft(40).reduced(0, 4));
  exportButton.setBounds(row2.removeFromRight(130).reduced(0, 4));

  // ---- Status bar ----
  auto statusArea = bounds.removeFromBottom(36).reduced(24, 0);
  statusLabel.setBounds(statusArea.removeFromLeft(400));
  if (!hasSample)
    loadButton.setBounds(statusArea.removeFromRight(100));
  else
    loadButton.setBounds(juce::Rectangle<int>());

  // ---- Waveform ----
  auto waveformArea = bounds.reduced(16, 8);

  // If chord mode is enabled, show spectral display instead
  if (chordModeToggle.getToggleState()) {
    // Split the area: top half waveform, bottom half spectral
    auto spectralArea =
        waveformArea.removeFromBottom(waveformArea.getHeight() / 2);
    spectralDisplay.setBounds(spectralArea);
    waveformDisplay.setBounds(waveformArea);
  } else {
    waveformDisplay.setBounds(waveformArea);
    spectralDisplay.setBounds(waveformArea); // Hidden anyway
  }

  // ---- Zoom buttons (overlay on waveform, top-right) ----
  auto wfBounds = waveformDisplay.getBounds();
  zoomOutButton.setBounds(wfBounds.getRight() - 72, wfBounds.getY() + 8, 32,
                          32);
  zoomInButton.setBounds(wfBounds.getRight() - 36, wfBounds.getY() + 8, 32, 32);
}

// ---------------------------------------------------------------------------
// Icon drawing helpers (kept for potential future use)
// ---------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::drawPlayIcon(juce::Graphics &g,
                                                   juce::Rectangle<float> area,
                                                   bool filled) {
  juce::Path p;
  p.addTriangle(area.getX(), area.getY(), area.getX(), area.getBottom(),
                area.getRight(), area.getCentreY());
  g.setColour(filled ? juce::Colours::black : Colors::accentCyan);
  g.fillPath(p);
}

void Sample2MidiAudioProcessorEditor::drawStopIcon(
    juce::Graphics &g, juce::Rectangle<float> area) {
  g.setColour(Colors::textGray);
  g.fillRoundedRectangle(area.reduced(2.0f), 2.0f);
}

void Sample2MidiAudioProcessorEditor::drawSparklesIcon(
    juce::Graphics &g, juce::Rectangle<float> area) {
  auto c = area.getCentre();
  juce::Path p;
  p.startNewSubPath(c.x, area.getY());
  p.lineTo(c.x + 3, c.y - 3);
  p.lineTo(area.getRight(), c.y);
  p.lineTo(c.x + 3, c.y + 3);
  p.lineTo(c.x, area.getBottom());
  p.lineTo(c.x - 3, c.y + 3);
  p.lineTo(area.getX(), c.y);
  p.lineTo(c.x - 3, c.y - 3);
  p.closeSubPath();
  g.setColour(Colors::accentCyan);
  g.fillPath(p);
}

void Sample2MidiAudioProcessorEditor::drawPlusIcon(
    juce::Graphics &g, juce::Rectangle<float> area) {
  g.setColour(juce::Colours::white);
  g.drawHorizontalLine((int)area.getCentreY(), area.getX(), area.getRight());
  g.drawVerticalLine((int)area.getCentreX(), area.getY(), area.getBottom());
}

void Sample2MidiAudioProcessorEditor::drawMinusIcon(
    juce::Graphics &g, juce::Rectangle<float> area) {
  g.setColour(juce::Colours::white);
  g.drawHorizontalLine((int)area.getCentreY(), area.getX(), area.getRight());
}

void Sample2MidiAudioProcessorEditor::drawGripIcon(
    juce::Graphics &g, juce::Rectangle<float> area) {
  g.setColour(juce::Colours::black);
  auto r = area.reduced(2);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j)
      g.fillEllipse(r.getX() + j * 6.0f, r.getY() + i * 6.0f, 2.0f, 2.0f);
}

// ---------------------------------------------------------------------------
// updateStatus
// ---------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::updateStatus(int noteCount) {
  if (noteCount > 0) {
    statusLabel.setColour(juce::Label::textColourId, Colors::successGreen);
    statusLabel.setText(
        juce::String(noteCount) + " notes | " +
            juce::String((int)audioProcessor.detectedBPM.load()) +
            juce::String(" BPM — drag to export"),
        juce::dontSendNotification);
  } else {
    statusLabel.setColour(juce::Label::textColourId, Colors::textGray);
    statusLabel.setText(
        juce::String("No notes detected - try a different file"),
        juce::dontSendNotification);
  }
}

// ---------------------------------------------------------------------------
// Drag and drop (file drop onto plugin)
// ---------------------------------------------------------------------------
bool Sample2MidiAudioProcessorEditor::isInterestedInFileDrag(
    const juce::StringArray &files) {
  return AudioFileLoader::isSupportedFile(files[0]);
}

void Sample2MidiAudioProcessorEditor::filesDropped(
    const juce::StringArray &files, int x, int y) {
  juce::File file(files[0]);
  statusLabel.setColour(juce::Label::textColourId, Colors::textGray);
  statusLabel.setText(juce::String("Analyzing..."), juce::dontSendNotification);
  waveformDisplay.setFile(file);
  hasSample = true;
  dragZone.setVisible(true);
  resized();

  // Update spectral display with audio data
  auto buffer = audioProcessor.getAudioBuffer();
  if (buffer && buffer->getNumSamples() > 0) {
    spectralDisplay.setAudioData(buffer->getReadPointer(0),
                                 buffer->getNumSamples(),
                                 buffer->getSampleRate());
  }

  audioProcessor.loadAndAnalyze(
      file, [this](int noteCount) { updateStatus(noteCount); });
}

// ---------------------------------------------------------------------------
// Timer for playhead updates
// ----------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::timerCallback() {
  if (isPlaying) {
    // Get current playback position from processor
    double position = audioProcessor.getTransportSourcePosition();
    if (position >= 0) {
      waveformDisplay.setPlayheadPosition(position);
    }
  }
}
