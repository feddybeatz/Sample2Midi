#pragma once

#include "MidiBuilder.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>


// NoteEditor: A piano roll view for toggling individual notes on/off
class NoteEditor : public juce::Component {
public:
  NoteEditor();
  ~NoteEditor() override;

  // Set notes to display
  void setNotes(const std::vector<MidiNote> &notes, double sampleRate);

  // Get only enabled notes
  std::vector<MidiNote> getActiveNotes() const;

  // Callback when notes change
  std::function<void(std::vector<MidiNote>)> onNotesChanged;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &) override;

private:
  std::vector<MidiNote> notes;
  std::vector<bool> noteEnabled;
  double sampleRate = 44100.0;

  // Layout bounds (set in resized())
  int labelWidth = 40;
  int gridLeft = 0;
  int gridRight = 0;
  int gridTop = 0;
  int gridBottom = 0;

  // Piano range: MIDI notes 21 (A0) to 108 (C8)
  static constexpr int MIN_NOTE = 21;
  static constexpr int MAX_NOTE = 108;
  static constexpr int NUM_NOTES = MAX_NOTE - MIN_NOTE + 1;

  // Helper to get note name
  static juce::String getNoteName(int midiNote);

  // Helper to check if note is a C (octave line)
  static bool isOctaveLine(int midiNote) { return midiNote % 12 == 0; }

  // Get pixel Y for a given MIDI note
  int noteToY(int midiNote) const;

  // Get MIDI note from pixel Y
  int yToNote(int y) const;

  // Get start/end sample from pixel X
  int xToStartSample(int x) const;
  int xToEndSample(int x) const;

  // Get pixel X from sample
  int sampleToX(int sample) const;

  // Find note at position
  int findNoteAt(int x, int y) const;
};
