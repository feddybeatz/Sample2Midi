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
  addAndMakeVisible(statusDot);
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

      audioProcessor.loadAndAnalyze(
          file, [this](int noteCount) { updateStatus(noteCount); },
          [this]() {
            // Update spectral display when load completes
            auto buffer = audioProcessor.getAudioBuffer();
            if (buffer && buffer->getNumSamples() > 0) {
              spectralDisplay.setAudioData(
                  buffer->getReadPointer(0), buffer->getNumSamples(),
                  audioProcessor.getCurrentSampleRate());
            }
          });
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

        if (detectedKey.isEmpty()) {
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
  pitchBendToggle.setColour(juce::TextButton::textColourOffId,
                            Colors::textGray);
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
  chordModeToggle.setColour(juce::TextButton::textColourOffId,
                            Colors::textGray);
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
      // Start playback from current playhead position
      double playPos = waveformDisplay.getPlayheadPosition();
      audioProcessor.startPlayback(playPos);
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
  exportButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::black);
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
                    audioProcessor.getCurrentSampleRate(), tempFile,
                    audioProcessor.detectedBPM.load());

      if (tempFile.existsAsFile()) {
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            {tempFile.getFullPathName()}, false);
      }
    }
  };
  addAndMakeVisible(dragZone);

  // Make editor resizable with minimum size
  setResizable(true, true);
  setResizeLimits(800, 500, 1600, 1000); // min 800x500, max 1600x1000

  setSize(1100, 700);
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

  // ---- Top Bar (70px) ----
  auto topArea = bounds.removeFromTop(70);
  g.setColour(Colors::borderSubtle);
  g.drawHorizontalLine(topArea.getBottom() - 1, 0.0f,
                       (float)topArea.getWidth());

  auto topPadded = topArea.reduced(24, 0);

  // Title "Sample2MIDI" - 30px bold, letterSpacing -0.02em
  g.setColour(Colors::textWhite);
  auto titleFont = juce::Font(juce::FontOptions(30.0f, juce::Font::bold));
  titleFont.setExtraKerningFactor(-0.02f);
  g.setFont(titleFont);
  auto titleArea = topPadded.removeFromLeft(250);
  g.drawText(juce::String("Sample2MIDI"), titleArea,
             juce::Justification::centredLeft);

  // Subtitle "AUDIO TO MIDI CONVERSION ENGINE" - 11px uppercase gray
  g.setColour(Colors::textDarkGray);
  g.setFont(juce::Font(juce::FontOptions(11.0f)));
  auto subArea = topPadded.removeFromLeft(280);
  g.drawText(juce::String("AUDIO TO MIDI CONVERSION ENGINE"), subArea,
             juce::Justification::centredLeft);

  // Brand badge: gradient #00E5FF to #00B8D4, black text, 8px radius
  auto badgeArea = topPadded.removeFromRight(140).reduced(0, 16);
  auto badgeBounds = badgeArea.toFloat();

  // Gradient badge background
  juce::ColourGradient gradient(Colors::accentCyan, badgeBounds.getTopLeft(),
                                Colors::accentCyan.withAlpha(0.7f),
                                badgeBounds.getBottomLeft(), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(badgeBounds, 8.0f);

  // Badge text
  g.setColour(juce::Colours::black);
  g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
  g.drawText(juce::String("Feddy Beatz"), badgeArea,
             juce::Justification::centred);

  // ---- Waveform Display (340px height, 24px margin) ----
  auto waveformArea = bounds.reduced(24, 0);
  auto wfHeight = 340;
  auto waveformBounds = waveformArea.removeFromTop(wfHeight);

  // Background gradient #1A1A24 to #0F0F14
  juce::ColourGradient wfGradient(
      juce::Colour(0xFF1A1A24), waveformBounds.toFloat().getTopLeft(),
      juce::Colour(0xFF0F0F14), waveformBounds.toFloat().getBottomLeft(),
      false);
  g.setGradientFill(wfGradient);
  g.fillRoundedRectangle(waveformBounds.toFloat(), 8.0f);

  // Pulsing dashed center line when empty
  if (!hasSample) {
    auto centerY = waveformBounds.getCentreY();
    g.setColour(Colors::textDarkGray.withAlpha(
        0.5f + 0.3f * std::sin(juce::Time::currentTimeMillis() * 0.005f)));
    float dash[] = {8.0f, 8.0f};
    g.drawDashedLine(
        juce::Line<float>(waveformBounds.getX() + 16, (float)centerY,
                          waveformBounds.getRight() - 16, (float)centerY),
        dash, 2, 1.0f);
  }

  // ---- Control Bar background (gradient panel) ----
  auto controlBgArea = bounds.reduced(16, 0);
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

  // Calculate responsive sizes based on current editor size
  int editorWidth = bounds.getWidth();
  int editorHeight = bounds.getHeight();

  // Top bar: 60px fixed height
  bounds.removeFromTop(60);

  // ---- Drag zone (bottom, only when sample loaded) ----
  if (hasSample) {
    auto dragArea = bounds.removeFromBottom(50).reduced(16, 8);
    dragZone.setBounds(dragArea);
  }

  // ---- Control bar (bottom 25% of remaining space, min 100px) ----
  int controlHeight = juce::jmax(100, bounds.getHeight() / 4);
  auto controlsArea = bounds.removeFromBottom(controlHeight).reduced(16, 0);
  auto inner = controlsArea.reduced(16, 8);

  auto row1 = inner.removeFromTop(inner.getHeight() / 2);
  auto row2 = inner;

  // -- Row 1: use proportional widths --
  // Scale dropdown: 12% of width
  scaleDropdown.setBounds(
      row1.removeFromLeft(juce::jmax(80, editorWidth / 10)).reduced(0, 6));
  autoDetectButton.setBounds(row1.removeFromLeft(36).reduced(2, 6));
  row1.removeFromLeft(juce::jmax(8, editorWidth / 60));

  // Quantize slider: 20% of width
  auto quantizeArea = row1.removeFromLeft(juce::jmax(120, editorWidth / 5));
  quantizeLabel.setBounds(quantizeArea.removeFromRight(50).reduced(0, 6));
  quantizeSlider.setBounds(quantizeArea.reduced(0, 8));
  row1.removeFromLeft(juce::jmax(8, editorWidth / 60));

  // Range dropdown: 12%
  rangeDropdown.setBounds(
      row1.removeFromLeft(juce::jmax(80, editorWidth / 10)).reduced(0, 6));
  row1.removeFromLeft(juce::jmax(8, editorWidth / 60));

  // Toggles: 8% each
  pitchBendToggle.setBounds(
      row1.removeFromLeft(juce::jmax(60, editorWidth / 15)).reduced(0, 6));
  row1.removeFromLeft(juce::jmax(4, editorWidth / 100));

  chordModeToggle.setBounds(
      row1.removeFromLeft(juce::jmax(70, editorWidth / 12)).reduced(0, 6));

  // -- Row 2: transport controls --
  playButton.setBounds(row2.removeFromLeft(40).reduced(0, 4));
  row2.removeFromLeft(8);
  stopButton.setBounds(row2.removeFromLeft(40).reduced(0, 4));

  // Export button: right aligned, 15% width
  exportButton.setBounds(
      row2.removeFromRight(juce::jmax(100, editorWidth / 7)).reduced(0, 4));

  // ---- Status bar ----
  auto statusArea = bounds.removeFromBottom(36).reduced(24, 0);
  statusLabel.setBounds(
      statusArea.removeFromLeft(juce::jmin(400, editorWidth / 3)));
  if (!hasSample)
    loadButton.setBounds(statusArea.removeFromRight(100));
  else
    loadButton.setBounds(juce::Rectangle<int>());

  // ---- Waveform & Spectral displays ----
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
  }

  // ---- Zoom buttons (top right of waveform) ----
  auto zoomW = 30;
  auto zoomH = 24;
  auto zoomY = waveformArea.getY() + 8;
  zoomOutButton.setBounds(
      zoomOutButton.getBounds().isEmpty()
          ? juce::Rectangle<int>(waveformArea.getRight() - zoomW * 2 - 8, zoomY,
                                 zoomW, zoomH)
          : zoomOutButton.getBounds());
  zoomInButton.setBounds(
      zoomInButton.getBounds().isEmpty()
          ? juce::Rectangle<int>(waveformArea.getRight() - zoomW - 4, zoomY,
                                 zoomW, zoomH)
          : zoomInButton.getBounds());
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

  audioProcessor.loadAndAnalyze(
      file, [this](int noteCount) { updateStatus(noteCount); },
      [this]() {
        // Update spectral display when load completes
        auto buffer = audioProcessor.getAudioBuffer();
        if (buffer && buffer->getNumSamples() > 0) {
          spectralDisplay.setAudioData(buffer->getReadPointer(0),
                                       buffer->getNumSamples(),
                                       audioProcessor.getCurrentSampleRate());
        }
      });
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
