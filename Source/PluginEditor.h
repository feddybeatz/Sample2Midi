#pragma once

#include "PluginProcessor.h"
#include "SpectralDisplay.h"
#include "WaveformDisplay.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

// ---------------------------------------------------------------------------
// Color palette (matches design spec)
// ---------------------------------------------------------------------------
namespace Colors {
const juce::Colour background(0xFF0D0D0D);
const juce::Colour controlBar(0xFF111111);
const juce::Colour panel(0xFF1A1A1A);
const juce::Colour inputBg(0xFF2A2A2A);
const juce::Colour inputHover(0xFF3A3A3A);
const juce::Colour accentCyan(0xFF00E5FF);
const juce::Colour accentCyanHover(0xFF00CCE5);
const juce::Colour borderDefault(0xFF3A3A3A);
const juce::Colour borderSubtle(0xFF1A1A1A);
const juce::Colour textWhite(0xFFFFFFFF);
const juce::Colour textGray(0xFF999999);
const juce::Colour textDarkGray(0xFF666666);
const juce::Colour successGreen(0xFF22C55E);
// Legacy aliases
const juce::Colour border = borderSubtle;
const juce::Colour button = inputBg;
const juce::Colour surface = panel;
} // namespace Colors

// ---------------------------------------------------------------------------
// Custom LookAndFeel
// ---------------------------------------------------------------------------
class CustomLookAndFeel : public juce::LookAndFeel_V4 {
public:
  CustomLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, Colors::background);
    setColour(juce::TextButton::buttonColourId, Colors::inputBg);
    setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, Colors::inputBg);
    setColour(juce::ComboBox::outlineColourId, Colors::borderDefault);
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::Slider::thumbColourId, Colors::accentCyan);
    setColour(juce::Slider::trackColourId, Colors::inputBg);
    setColour(juce::Slider::backgroundColourId, Colors::inputBg);
    setColour(juce::Label::textColourId, Colors::textGray);
    setColour(juce::PopupMenu::backgroundColourId, Colors::inputBg);
    setColour(juce::PopupMenu::textColourId, juce::Colours::white);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              Colors::accentCyan);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
  }

  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override {
    auto cornerSize = 4.0f;
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    auto baseColour = backgroundColour;
    if (shouldDrawButtonAsDown)
      baseColour = baseColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
      baseColour = baseColour.brighter(0.1f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(Colors::borderDefault);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
  }

  void drawComboBox(juce::Graphics &g, int width, int height, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH,
                    juce::ComboBox &box) override {
    auto cornerSize = 4.0f;
    auto bounds = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);

    g.setColour(Colors::inputBg);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(box.hasKeyboardFocus(false) ? Colors::accentCyan
                                            : Colors::borderDefault);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    // Arrow
    juce::Path path;
    path.startNewSubPath(width - 20.0f, height * 0.42f);
    path.lineTo(width - 15.0f, height * 0.58f);
    path.lineTo(width - 10.0f, height * 0.42f);

    g.setColour(Colors::textGray);
    g.strokePath(path, juce::PathStrokeType(1.5f));
  }

  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        juce::Slider::SliderStyle style,
                        juce::Slider &slider) override {
    const float trackH = 6.0f;
    const float thumbR = 8.0f;
    const float cy = y + height * 0.5f;

    // Track background
    juce::Rectangle<float> trackBg(x, cy - trackH * 0.5f, width, trackH);
    g.setColour(Colors::inputBg);
    g.fillRoundedRectangle(trackBg, trackH * 0.5f);

    // Filled portion
    juce::Rectangle<float> trackFill(x, cy - trackH * 0.5f, sliderPos - x,
                                     trackH);
    g.setColour(Colors::accentCyan);
    g.fillRoundedRectangle(trackFill, trackH * 0.5f);

    // Thumb
    g.setColour(Colors::accentCyan);
    g.fillEllipse(sliderPos - thumbR, cy - thumbR, thumbR * 2.0f,
                  thumbR * 2.0f);
  }
};

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
class Sample2MidiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::FileDragAndDropTarget,
                                        public juce::DragAndDropContainer,
                                        public juce::Timer {
public:
  Sample2MidiAudioProcessorEditor(Sample2MidiAudioProcessor &);
  ~Sample2MidiAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  bool isInterestedInFileDrag(const juce::StringArray &files) override;
  void filesDropped(const juce::StringArray &files, int x, int y) override;

  // Timer for playhead updates
  void timerCallback() override;

  void updateStatus(int noteCount);

private:
  // -------------------------------------------------------------------------
  // Icon helpers
  // -------------------------------------------------------------------------
  void drawPlayIcon(juce::Graphics &g, juce::Rectangle<float> area,
                    bool filled);
  void drawStopIcon(juce::Graphics &g, juce::Rectangle<float> area);
  void drawSparklesIcon(juce::Graphics &g, juce::Rectangle<float> area);
  void drawPlusIcon(juce::Graphics &g, juce::Rectangle<float> area);
  void drawMinusIcon(juce::Graphics &g, juce::Rectangle<float> area);
  void drawGripIcon(juce::Graphics &g, juce::Rectangle<float> area);

  // -------------------------------------------------------------------------
  // State
  // -------------------------------------------------------------------------
  bool hasSample = false;
  bool isPlaying = false;

  // -------------------------------------------------------------------------
  // Core references
  // -------------------------------------------------------------------------
  Sample2MidiAudioProcessor &audioProcessor;
  CustomLookAndFeel customLookAndFeel;

  juce::AudioThumbnailCache thumbnailCache{5};
  WaveformDisplay waveformDisplay;
  SpectralDisplay spectralDisplay;

  // -------------------------------------------------------------------------
  // Components
  // -------------------------------------------------------------------------

  // Status bar
  juce::Label statusLabel;
  juce::TextButton loadButton{"Load Sample"};

  // Row 1 controls
  juce::ComboBox scaleDropdown;
  juce::TextButton autoDetectButton;
  juce::Slider quantizeSlider;
  juce::Label quantizeLabel;
  juce::ComboBox rangeDropdown;
  juce::TextButton pitchBendToggle{"OFF"};
  juce::TextButton chordModeToggle{"OFF"};

  // Row 2 transport
  juce::TextButton playButton;
  juce::TextButton stopButton;
  juce::TextButton exportButton{"Export MIDI"};

  // Zoom
  juce::TextButton zoomInButton;
  juce::TextButton zoomOutButton;

  // Drag zone (only visible when hasSample)
  // Dragging this zone initiates an external file drag to the DAW
  struct DragZone : public juce::Component {
    bool isDragging = false;
    std::function<void()> onStartDrag;

    void paint(juce::Graphics &g) override {
      auto area = getLocalBounds().toFloat().reduced(1.0f);

      g.setColour(isDragging ? Colors::accentCyan.withAlpha(0.1f)
                             : juce::Colours::transparentBlack);
      g.fillRoundedRectangle(area, 4.0f);

      g.setColour(isDragging ? Colors::accentCyan : Colors::borderDefault);
      float dash[] = {4.0f, 4.0f};
      g.drawDashedLine(juce::Line<float>(area.getX(), area.getY(),
                                         area.getRight(), area.getY()),
                       dash, 2, 2.0f);
      g.drawDashedLine(juce::Line<float>(area.getRight(), area.getY(),
                                         area.getRight(), area.getBottom()),
                       dash, 2, 2.0f);
      g.drawDashedLine(juce::Line<float>(area.getRight(), area.getBottom(),
                                         area.getX(), area.getBottom()),
                       dash, 2, 2.0f);
      g.drawDashedLine(juce::Line<float>(area.getX(), area.getBottom(),
                                         area.getX(), area.getY()),
                       dash, 2, 2.0f);

      g.setColour(Colors::textGray);
      g.setFont(juce::Font(juce::FontOptions(14.0f)));
      g.drawText(isDragging ? juce::String("Drop into your DAW")
                            : juce::String("Drag MIDI to DAW"),
                 getLocalBounds(), juce::Justification::centred);
    }

    void mouseDrag(const juce::MouseEvent &) override {
      if (!isDragging) {
        isDragging = true;
        repaint();
        if (onStartDrag)
          onStartDrag();
        // Reset after drag completes
        isDragging = false;
        repaint();
      }
    }

    void mouseEnter(const juce::MouseEvent &) override { repaint(); }
    void mouseExit(const juce::MouseEvent &) override {
      isDragging = false;
      repaint();
    }
  } dragZone;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sample2MidiAudioProcessorEditor)
};
