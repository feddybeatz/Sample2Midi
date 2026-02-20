#pragma once
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>


class WaveformDisplay : public juce::Component {
public:
  WaveformDisplay(juce::AudioFormatManager &formatManager,
                  juce::AudioThumbnailCache &cache);

  void paint(juce::Graphics &g) override;
  void setFile(const juce::File &file);

  double getTotalLength() const { return thumbnail.getTotalLength(); }

  // Playhead control
  void setPlayheadPosition(double positionSeconds);
  double getPlayheadPosition() const { return playheadPosition; }

  // Zoom control
  void setZoom(double zoomLevel);
  double getZoom() const { return zoomLevel; }
  void setViewStart(double startSeconds);
  double getViewStart() const { return viewStart; }

  // Callback for playhead drag
  std::function<void(double)> onPlayheadDrag;

  // Mouse handling for draggable playhead
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

private:
  juce::AudioThumbnail thumbnail;
  double playheadPosition = 0.0;
  double zoomLevel = 1.0;
  double viewStart = 0.0;
  bool isDraggingPlayhead = false;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
