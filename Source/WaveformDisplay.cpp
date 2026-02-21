#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(juce::AudioFormatManager &formatManager,
                                 juce::AudioThumbnailCache &cache)
    : thumbnail(512, formatManager, cache) {
  setBufferedToImage(true);
}

void WaveformDisplay::paint(juce::Graphics &g) {
  auto area = getLocalBounds();

  // Background
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRoundedRectangle(area.toFloat(), 8.0f);

  if (thumbnail.getTotalLength() > 0.0) {
    auto waveformArea = area.reduced(10, 20);

    // Calculate visible range based on zoom and viewStart
    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / zoomLevel;
    double visibleStart = viewStart;
    double visibleEnd = visibleStart + visibleDuration;

    // Ensure visible range is valid
    if (visibleEnd > totalLength) {
      visibleEnd = totalLength;
      visibleStart = std::max(0.0, totalLength - visibleDuration);
    }

    // Draw the visible portion of the waveform
    // Glow layer
    g.setColour(juce::Colour(0x4000e5ff));
    thumbnail.drawChannels(g, waveformArea, visibleStart, visibleEnd, 1.2f);

    // Main layer
    g.setColour(juce::Colour(0xff00e5ff));
    thumbnail.drawChannels(g, waveformArea, visibleStart, visibleEnd, 1.0f);

    // Draw playhead at correct X position relative to viewStart
    if (playheadPosition >= visibleStart && playheadPosition <= visibleEnd) {
      float playheadX =
          (float)((playheadPosition - visibleStart) / visibleDuration) *
          waveformArea.getWidth();
      playheadX = juce::jlimit(0.0f, (float)waveformArea.getWidth(), playheadX);

      // Playhead line
      g.setColour(juce::Colour(0xffff4444));
      g.drawVerticalLine((int)playheadX, waveformArea.getY(),
                         waveformArea.getBottom());

      // Playhead handle (triangle at top)
      juce::Path playheadHandle;
      playheadHandle.addTriangle(playheadX - 6, waveformArea.getY(),
                                 playheadX + 6, waveformArea.getY(), playheadX,
                                 waveformArea.getY() + 10);
      g.fillPath(playheadHandle);
    }

    // Draw time markers
    g.setColour(juce::Colour(0xff666666));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    int numMarkers = 5;
    for (int i = 0; i <= numMarkers; ++i) {
      double time = visibleStart + (visibleEnd - visibleStart) * i / numMarkers;
      float x =
          waveformArea.getX() + (float)i / numMarkers * waveformArea.getWidth();

      // Draw tick
      g.drawVerticalLine((int)x, waveformArea.getBottom(),
                         waveformArea.getBottom() + 5);

      // Draw time label
      juce::String timeStr;
      if (time >= 60.0) {
        timeStr = juce::String((int)(time / 60)) + ":" +
                  juce::String::formatted("%02d", (int)time % 60);
      } else {
        timeStr = juce::String::formatted("%.1f", time);
      }
      g.drawText(timeStr, (int)x - 20, waveformArea.getBottom() + 6, 40, 10,
                 juce::Justification::centred);
    }

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
  playheadPosition = 0.0;
  viewStart = 0.0;
  zoomLevel = 1.0;
  repaint();
}

void WaveformDisplay::setPlayheadPosition(double positionSeconds) {
  playheadPosition = positionSeconds;

  // Auto-scroll if playhead goes out of view
  double totalLength = thumbnail.getTotalLength();
  double visibleEnd = viewStart + totalLength / zoomLevel;

  if (positionSeconds < viewStart) {
    viewStart = std::max(0.0, positionSeconds - 0.5);
  } else if (positionSeconds > visibleEnd) {
    viewStart =
        std::max(0.0, positionSeconds - (totalLength / zoomLevel) + 0.5);
  }

  repaint();
}

void WaveformDisplay::setZoom(double newZoom) {
  zoomLevel = std::clamp(newZoom, 1.0, 50.0);

  double totalLength = thumbnail.getTotalLength();
  if (totalLength <= 0.0) {
    repaint();
    return;
  }

  // Calculate visible duration based on new zoom level
  double visibleDuration = totalLength / zoomLevel;

  // Center view on playhead position
  viewStart = playheadPosition - (visibleDuration / 2.0);

  // Clamp viewStart between 0 and (totalLength - visibleDuration)
  viewStart =
      std::clamp(viewStart, 0.0, std::max(0.0, totalLength - visibleDuration));

  repaint();
}

void WaveformDisplay::setViewStart(double startSeconds) {
  double totalLength = thumbnail.getTotalLength();
  viewStart = std::clamp(startSeconds, 0.0,
                         std::max(0.0, totalLength - totalLength / zoomLevel));
  repaint();
}

void WaveformDisplay::mouseDown(const juce::MouseEvent &e) {
  auto waveformArea = getLocalBounds().reduced(10, 20);

  if (thumbnail.getTotalLength() > 0.0 &&
      waveformArea.contains(e.getPosition())) {
    double totalLength = thumbnail.getTotalLength();
    double visibleStart = viewStart;
    double visibleEnd =
        std::min(viewStart + totalLength / zoomLevel, totalLength);

    // Check if click is near playhead (within 10 pixels)
    float playheadX =
        waveformArea.getX() + (float)((playheadPosition - visibleStart) /
                                      (visibleEnd - visibleStart)) *
                                  waveformArea.getWidth();

    if (std::abs(e.getPosition().x - playheadX) < 10 ||
        waveformArea.contains(e.getPosition())) {
      isDraggingPlayhead = true;
      mouseDrag(e);
    }
  }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent &e) {
  if (!isDraggingPlayhead)
    return;

  if (thumbnail.getTotalLength() > 0.0) {
    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / zoomLevel;

    // Calculate dragged position in seconds
    double draggedPos =
        viewStart + (e.x / (double)getWidth()) * visibleDuration;
    draggedPos = juce::jlimit(0.0, totalLength, draggedPos);

    playheadPosition = draggedPos;

    // Notify callback
    if (onPlayheadDrag) {
      onPlayheadDrag(playheadPosition);
    }

    repaint();
  }
}

void WaveformDisplay::mouseUp(const juce::MouseEvent &e) {
  isDraggingPlayhead = false;
}
