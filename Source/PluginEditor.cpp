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

  // Callback to update transport position when dragging playhead
  waveformDisplay.onPlayheadDrag = [this](double newPosition) {
    audioProcessor.setPlaybackPosition(newPosition);
  };

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

            // Update note editor with detected notes
            auto notes = audioProcessor.getDetectedNotes();
            if (!notes.empty()) {
              noteEditor.setNotes(notes, audioProcessor.getCurrentSampleRate());
              filteredNotes = notes;
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

    // Update chord mode for MIDI export
    audioProcessor.chordModeActive = isChordMode;

    resized();
  };
  addAndMakeVisible(chordModeToggle);

  // ---- Note Editor toggle ----
  noteEditorToggle.setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  noteEditorToggle.setColour(juce::TextButton::textColourOffId,
                             Colors::textGray);
  noteEditorToggle.setColour(juce::TextButton::buttonOnColourId,
                             Colors::accentCyan);
  noteEditorToggle.setColour(juce::TextButton::textColourOnId,
                             juce::Colours::black);
  noteEditorToggle.setClickingTogglesState(true);
  noteEditorToggle.onClick = [this] {
    bool isVisible = noteEditorToggle.getToggleState();
    noteEditor.setVisible(isVisible);
    resized();
  };
  addAndMakeVisible(noteEditorToggle);
  noteEditor.setVisible(false);

  // Note editor callback
  noteEditor.onNotesChanged = [this](std::vector<MidiNote> activeNotes) {
    filteredNotes = activeNotes;
    audioProcessor.setFilteredNotes(activeNotes);
  };
  addAndMakeVisible(noteEditor);

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

  // Double-click to return playhead to start
  stopButton.onDoubleClick = [this] {
    audioProcessor.stopPlayback();
    audioProcessor.setPlaybackPosition(0.0); // rewind transport
    isPlaying = false;
    stopTimer();
    playButton.setToggleState(false, juce::dontSendNotification);
    waveformDisplay.setPlayheadPosition(0.0); // rewind visual playhead
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

  // Make editor resizable with proportional minimum size
  setResizable(true, true);
  // Use proportional resize limits (min 800x500, max 1600x1000)
  setResizeLimits(800, 500, 1600, 1000);

  // Initial size: proportional to minimum but slightly larger
  setSize(1100, 700);

  // Enable keyboard focus for spacebar shortcuts
  setWantsKeyboardFocus(true);
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
  int w = bounds.getWidth();
  int h = bounds.getHeight();

  // ---- Top Bar (8% of height) ----
  int topHeight = (int)(h * 0.08f);
  auto topArea = bounds.removeFromTop(topHeight);
  g.setColour(Colors::borderSubtle);
  g.drawHorizontalLine(topArea.getBottom() - 1, 0.0f,
                       (float)topArea.getWidth());

  int topMargin = (int)(w * 0.022f);
  auto topPadded = topArea.reduced(topMargin, 0);

  // Title "Sample2MIDI" - proportional font size
  float titleSize = h * 0.043f;
  titleSize = juce::jmax(20.0f, juce::jmin(titleSize, 36.0f));
  g.setColour(Colors::textWhite);
  auto titleFont = juce::Font(juce::FontOptions(titleSize, juce::Font::bold));
  titleFont.setExtraKerningFactor(-0.02f);
  g.setFont(titleFont);
  int titleWidth = (int)(w * 0.23f);
  auto titleArea = topPadded.removeFromLeft(titleWidth);
  g.drawText(juce::String("Sample2MIDI"), titleArea,
             juce::Justification::centredLeft);

  // Subtitle "AUDIO TO MIDI CONVERSION ENGINE" - proportional font size
  float subSize = h * 0.016f;
  subSize = juce::jmax(9.0f, juce::jmin(subSize, 14.0f));
  g.setColour(Colors::textDarkGray);
  g.setFont(juce::Font(juce::FontOptions(subSize)));
  int subWidth = (int)(w * 0.26f);
  auto subArea = topPadded.removeFromLeft(subWidth);
  g.drawText(juce::String("AUDIO TO MIDI CONVERSION ENGINE"), subArea,
             juce::Justification::centredLeft);

  // Brand badge - proportional size
  int badgeWidth = (int)(w * 0.13f);
  int badgeHeight = (int)(topArea.getHeight() * 0.5f);
  auto badgeArea =
      topPadded.removeFromRight(badgeWidth)
          .reduced(0, (int)((topArea.getHeight() - badgeHeight) / 2));
  auto badgeBounds = badgeArea.toFloat();

  // Gradient badge background
  juce::ColourGradient gradient(Colors::accentCyan, badgeBounds.getTopLeft(),
                                Colors::accentCyan.withAlpha(0.7f),
                                badgeBounds.getBottomLeft(), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(badgeBounds, 8.0f);

  // Badge text - proportional font size
  g.setColour(juce::Colours::black);
  float badgeTextSize = badgeHeight * 0.5f;
  g.setFont(juce::Font(juce::FontOptions(badgeTextSize, juce::Font::bold)));
  g.drawText(juce::String("Feddy Beatz"), badgeArea,
             juce::Justification::centred);

  // ---- Waveform Display (use proportional height, 50% of remaining space)
  // ----
  int wfMargin = (int)(w * 0.022f);
  auto waveformArea = bounds.reduced(wfMargin, 0);
  int wfHeight = (int)(bounds.getHeight() * 0.50f);
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
    float dashMargin = wfMargin * 0.67f;
    g.drawDashedLine(juce::Line<float>(waveformBounds.getX() + dashMargin,
                                       (float)centerY,
                                       waveformBounds.getRight() - dashMargin,
                                       (float)centerY),
                     dash, 2, 1.0f);
  }

  // ---- Control Bar background (gradient panel) ----
  int controlMargin = (int)(w * 0.015f);
  auto controlBgArea = bounds.reduced(controlMargin, 0);
  g.setColour(Colors::controlBar);
  g.fillRoundedRectangle(controlBgArea.toFloat(), 8.0f);

  // Row separator line inside control bar
  auto sepY = controlBgArea.getY() + controlBgArea.getHeight() / 2;
  g.setColour(Colors::borderSubtle);
  g.drawHorizontalLine(sepY, (float)controlBgArea.getX() + controlMargin,
                       (float)controlBgArea.getRight() - controlMargin);

  // ---- Labels above controls ----
  float labelSize = h * 0.017f;
  labelSize = juce::jmax(10.0f, juce::jmin(labelSize, 14.0f));
  g.setFont(juce::Font(juce::FontOptions(labelSize)));
  g.setColour(Colors::textGray);

  // Use same layout math as resized()
  auto cb = getLocalBounds();
  cb.removeFromTop((int)(h * 0.08f));
  if (hasSample)
    cb.removeFromBottom((int)(h * 0.06f));
  int ctrlHeight = juce::jmax(100, (int)(cb.getHeight() * 0.22f));
  auto ctrlArea = cb.removeFromBottom(ctrlHeight).reduced(controlMargin, 0);
  auto innerArea = ctrlArea.reduced(controlMargin, 8);
  auto labelRow = innerArea.removeFromTop(innerArea.getHeight() / 2);

  auto paintLabel = [&](juce::Rectangle<int> area, const char *text) {
    g.drawText(juce::String(text), area.withHeight(14).withY(area.getY() - 16),
               juce::Justification::centredLeft);
  };

  auto labelW1 = (int)(w * 0.11f);
  paintLabel(labelRow.removeFromLeft(labelW1), "Scale");
  labelRow.removeFromLeft((int)(w * 0.04f));
  auto labelW2 = (int)(w * 0.16f);
  paintLabel(labelRow.removeFromLeft(labelW2), "Quantize");
  labelRow.removeFromLeft((int)(w * 0.015f));
  auto labelW3 = (int)(w * 0.10f);
  paintLabel(labelRow.removeFromLeft(labelW3), "Range");
  labelRow.removeFromLeft((int)(w * 0.015f));
  auto labelW4 = (int)(w * 0.08f);
  paintLabel(labelRow.removeFromLeft(labelW4), "Pitch Bend");
  labelRow.removeFromLeft((int)(w * 0.01f));
  auto labelW5 = (int)(w * 0.08f);
  paintLabel(labelRow.removeFromLeft(labelW5), "Chord Mode");
}

// ---------------------------------------------------------------------------
// resized
// ---------------------------------------------------------------------------
void Sample2MidiAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();

  // Calculate responsive sizes based on current editor size
  int editorWidth = bounds.getWidth();
  int editorHeight = bounds.getHeight();

  // Top bar: 8% of height
  bounds.removeFromTop((int)(editorHeight * 0.08f));

  // ---- Drag zone (bottom, 6% of height, only when sample loaded) ----
  if (hasSample) {
    int dragHeight = (int)(editorHeight * 0.06f);
    auto dragArea = bounds.removeFromBottom(dragHeight);
    int margin = (int)(editorWidth * 0.015f);
    dragZone.setBounds(dragArea.reduced(margin, (int)(dragHeight * 0.15f)));
  }

  // ---- Control bar (bottom 22% of remaining space, min 100px) ----
  int controlHeight = juce::jmax(100, (int)(bounds.getHeight() * 0.22f));
  auto controlsArea = bounds.removeFromBottom(controlHeight);
  int controlMargin = (int)(editorWidth * 0.015f);
  auto inner = controlsArea.reduced(controlMargin, 8);

  auto row1 = inner.removeFromTop(inner.getHeight() / 2);
  auto row2 = inner;

  // -- Row 1: Scale dropdown (12% of width) --
  auto scaleWidth = (int)(editorWidth * 0.12f);
  scaleWidth = juce::jmax(80, scaleWidth);
  scaleDropdown.setBounds(row1.removeFromLeft(scaleWidth).reduced(0, 6));

  // Auto-detect button (proportional width)
  auto detectWidth = (int)(editorWidth * 0.04f);
  detectWidth = juce::jmax(36, detectWidth);
  autoDetectButton.setBounds(row1.removeFromLeft(detectWidth).reduced(2, 6));

  // Gap
  auto gap1 = (int)(editorWidth * 0.01f);
  row1.removeFromLeft(juce::jmax(8, gap1));

  // -- Quantize slider (18% of width) --
  auto quantizeWidth = (int)(editorWidth * 0.18f);
  quantizeWidth = juce::jmax(120, quantizeWidth);
  auto quantizeArea = row1.removeFromLeft(quantizeWidth);
  auto labelWidth = (int)(quantizeArea.getWidth() * 0.25f);
  quantizeLabel.setBounds(
      quantizeArea.removeFromRight(labelWidth).reduced(0, 6));
  quantizeSlider.setBounds(quantizeArea.reduced(0, 8));

  // Gap
  row1.removeFromLeft(juce::jmax(8, (int)(editorWidth * 0.01f)));

  // -- Range dropdown (10% of width) --
  auto rangeWidth = (int)(editorWidth * 0.10f);
  rangeWidth = juce::jmax(80, rangeWidth);
  rangeDropdown.setBounds(row1.removeFromLeft(rangeWidth).reduced(0, 6));

  // Gap
  row1.removeFromLeft(juce::jmax(8, (int)(editorWidth * 0.01f)));

  // -- Toggles (proportional width) --
  auto toggleWidth = (int)(editorWidth * 0.08f);
  toggleWidth = juce::jmax(60, toggleWidth);
  pitchBendToggle.setBounds(row1.removeFromLeft(toggleWidth).reduced(0, 6));

  auto gap2 = (int)(editorWidth * 0.005f);
  row1.removeFromLeft(juce::jmax(4, gap2));

  auto chordWidth = (int)(editorWidth * 0.09f);
  chordWidth = juce::jmax(70, chordWidth);
  chordModeToggle.setBounds(row1.removeFromLeft(chordWidth).reduced(0, 6));

  // -- Row 2: transport controls (proportional) --
  auto transportWidth = (int)(editorWidth * 0.05f);
  transportWidth = juce::jmax(40, transportWidth);
  playButton.setBounds(row2.removeFromLeft(transportWidth).reduced(0, 4));

  auto transportGap = (int)(editorWidth * 0.01f);
  row2.removeFromLeft(juce::jmax(8, transportGap));
  stopButton.setBounds(row2.removeFromLeft(transportWidth).reduced(0, 4));

  // Note editor toggle button
  auto noteToggleWidth = (int)(editorWidth * 0.10f);
  noteToggleWidth = juce::jmax(70, noteToggleWidth);
  row2.removeFromLeft(juce::jmax(8, (int)(editorWidth * 0.01f)));
  noteEditorToggle.setBounds(
      row2.removeFromLeft(noteToggleWidth).reduced(0, 4));

  // Export button: right aligned (15% width)
  auto exportWidth = (int)(editorWidth * 0.15f);
  exportWidth = juce::jmax(100, exportWidth);
  exportButton.setBounds(row2.removeFromRight(exportWidth).reduced(0, 4));

  // ---- Status bar (5% of height) ----
  int statusHeight = (int)(editorHeight * 0.05f);
  auto statusArea = bounds.removeFromBottom(statusHeight);
  int statusMargin = (int)(editorWidth * 0.02f);
  statusArea = statusArea.reduced(statusMargin, 0);

  // Status dot: small circle to the left of the label
  int dotSize = (int)(statusHeight * 0.5f);
  dotSize = juce::jmax(8, juce::jmin(dotSize, 16));
  statusDot.setBounds(statusArea.getX(),
                      statusArea.getY() +
                          (statusArea.getHeight() - dotSize) / 2,
                      dotSize, dotSize);

  // Status label: after the dot
  auto labelArea = statusArea.removeFromLeft((int)(editorWidth * 0.35f));
  statusLabel.setBounds(
      labelArea.removeFromLeft(labelArea.getWidth() - dotSize - 4));

  // Load button: use setVisible instead of empty bounds
  loadButton.setVisible(!hasSample);
  if (!hasSample) {
    auto loadArea = statusArea.removeFromRight((int)(editorWidth * 0.1f));
    loadButton.setBounds(loadArea);
  }

  // ---- Waveform & Spectral displays (use remaining space) ----
  int waveformMargin = (int)(editorWidth * 0.02f);
  auto waveformArea = bounds.reduced(waveformMargin, 8);

  // Save waveform bounds before any modifications for zoom button positioning
  auto waveformDisplayBounds = waveformArea;

  // If note editor is enabled, reserve space for it first (40% of height)
  if (noteEditor.isVisible()) {
    int noteEditorHeight = (int)(waveformArea.getHeight() * 0.40f);
    auto noteEditorArea = waveformArea.removeFromBottom(noteEditorHeight);
    noteEditor.setBounds(noteEditorArea);
  }

  // If chord mode is enabled, show spectral display below waveform
  if (chordModeToggle.getToggleState()) {
    // Split the area: top half waveform, bottom half spectral
    auto spectralArea =
        waveformArea.removeFromBottom(waveformArea.getHeight() / 2);
    spectralDisplay.setBounds(spectralArea);
    waveformDisplay.setBounds(waveformArea);
  } else {
    waveformDisplay.setBounds(waveformArea);
  }

  // ---- Zoom buttons (top right of waveformDisplay, always use saved bounds)
  // ----
  auto zoomW =
      (int)(waveformDisplayBounds.getWidth() * 0.04f); // 4% of waveform width
  auto zoomH =
      (int)(waveformDisplayBounds.getHeight() * 0.08f); // 8% of waveform height
  zoomW = juce::jmax(25, juce::jmin(zoomW, 40));        // Clamp between 25-40px
  zoomH = juce::jmax(20, juce::jmin(zoomH, 30));        // Clamp between 20-30px
  // Always anchor to top of waveformDisplay (use saved bounds)
  auto zoomY = waveformDisplayBounds.getY() +
               (int)(waveformDisplayBounds.getHeight() * 0.02f); // 2% from top
  zoomOutButton.setBounds(waveformDisplayBounds.getRight() - zoomW * 2 - 8,
                          zoomY, zoomW, zoomH);
  zoomInButton.setBounds(waveformDisplayBounds.getRight() - zoomW - 4, zoomY,
                         zoomW, zoomH);
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
  if (isPlaying && !waveformDisplay.isDraggingPlayhead) {
    // Get current playback position from processor
    double position = audioProcessor.getTransportSourcePosition();
    if (position >= 0) {
      waveformDisplay.setPlayheadPosition(position);
    }
  }
}

// ---------------------------------------------------------------------------
// Keyboard handling for spacebar play/pause
// ----------------------------------------------------------------------------
bool Sample2MidiAudioProcessorEditor::keyPressed(const juce::KeyPress &key) {
  if (key == juce::KeyPress::spaceKey && hasSample) {
    playButton.triggerClick(); // reuse existing playButton.onClick logic
    return true;
  }
  return false;
}
