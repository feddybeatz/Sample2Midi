#include "NoteEditor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// NoteEditor
// ---------------------------------------------------------------------------

NoteEditor::NoteEditor() {
  // Initialize with empty notes
  noteEnabled.clear();
}

NoteEditor::~NoteEditor() {}

void NoteEditor::setNotes(const std::vector<MidiNote> &newNotes,
                          double newSampleRate) {
  notes = newNotes;
  sampleRate = newSampleRate;

  // Initialize all notes as enabled
  noteEnabled.clear();
  noteEnabled.resize(notes.size(), true);

  repaint();
}

std::vector<MidiNote> NoteEditor::getActiveNotes() const {
  std::vector<MidiNote> activeNotes;
  for (size_t i = 0; i < notes.size(); ++i) {
    if (noteEnabled[i]) {
      activeNotes.push_back(notes[i]);
    }
  }
  return activeNotes;
}

void NoteEditor::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::panel);

  // Grid bounds
  int gridWidth = gridRight - gridLeft;
  int gridHeight = gridBottom - gridTop;

  if (gridWidth <= 0 || gridHeight <= 0 || notes.empty())
    return;

  // Get total duration in samples
  int maxSample = 0;
  for (const auto &note : notes) {
    if (note.endSample > maxSample)
      maxSample = note.endSample;
  }
  if (maxSample == 0)
    return;

  // Draw octave grid lines and note names
  g.setFont(juce::Font(juce::FontOptions(10.0f)));

  for (int noteNum = MIN_NOTE; noteNum <= MAX_NOTE; ++noteNum) {
    float y = gridTop + (MAX_NOTE - noteNum) * (float)gridHeight / NUM_NOTES;

    // C notes (octave markers) get darker lines and labels
    if (isOctaveLine(noteNum)) {
      g.setColour(Colors::borderSubtle);
      g.drawLine(gridLeft, y, gridRight, y, 1.0f);

      // Draw note name on left
      g.setColour(Colors::textGray);
      juce::String noteName = getNoteName(noteNum);
      g.drawText(noteName, 4, (int)y - 6, labelWidth - 8, 12,
                 juce::Justification::right);
    } else {
      // Subtle line for non-C notes
      g.setColour(Colors::borderSubtle.withAlpha(0.3f));
      g.drawLine(gridLeft, y, gridRight, y, 0.5f);
    }
  }

  // Draw notes
  for (size_t i = 0; i < notes.size(); ++i) {
    const auto &note = notes[i];

    int x1 = gridLeft + (int)((float)note.startSample / maxSample * gridWidth);
    int x2 = gridLeft + (int)((float)note.endSample / maxSample * gridWidth);

    int y1 = gridTop + (MAX_NOTE - note.noteNumber) * gridHeight / NUM_NOTES;
    int y2 =
        gridTop + (MAX_NOTE - note.noteNumber - 1) * gridHeight / NUM_NOTES;

    // Clamp to grid bounds
    x1 = juce::jlimit(gridLeft, gridRight, x1);
    x2 = juce::jlimit(gridLeft, gridRight, x2);

    juce::Rectangle<int> noteRect(x1, y2, x2 - x1, y1 - y2);

    // Draw note rectangle
    if (noteEnabled[i]) {
      // Active note: cyan
      g.setColour(Colors::accentCyan.withAlpha(0.7f));
    } else {
      // Disabled note: grey
      g.setColour(Colors::textDarkGray.withAlpha(0.5f));
    }
    g.fillRect(noteRect);

    // Border
    g.setColour(noteEnabled[i] ? Colors::accentCyan : Colors::textDarkGray);
    g.drawRect(noteRect, 1);
  }
}

void NoteEditor::resized() {
  // Layout: 40px for note labels on left, rest is grid
  labelWidth = 40;
  gridLeft = labelWidth;
  gridRight = getWidth() - 4;
  gridTop = 4;
  gridBottom = getHeight() - 4;

  repaint();
}

void NoteEditor::mouseDown(const juce::MouseEvent &event) {
  int x = event.x;
  int y = event.y;

  // Check if click is in grid area
  if (x < gridLeft || x > gridRight || y < gridTop || y > gridBottom)
    return;

  // Find which note was clicked
  int noteIndex = findNoteAt(x, y);
  if (noteIndex >= 0 && noteIndex < (int)notes.size()) {
    // Toggle note enabled state
    noteEnabled[noteIndex] = !noteEnabled[noteIndex];
    repaint();

    // Callback with active notes
    if (onNotesChanged) {
      onNotesChanged(getActiveNotes());
    }
  }
}

juce::String NoteEditor::getNoteName(int midiNote) {
  const char *noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                             "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (midiNote / 12) - 1;
  int note = midiNote % 12;
  return juce::String(noteNames[note]) + juce::String(octave);
}

int NoteEditor::noteToY(int midiNote) const {
  int gridHeight = gridBottom - gridTop;
  return gridTop + (MAX_NOTE - midiNote) * gridHeight / NUM_NOTES;
}

int NoteEditor::yToNote(int y) const {
  int gridHeight = gridBottom - gridTop;
  int note = MAX_NOTE - ((y - gridTop) * NUM_NOTES / gridHeight);
  return juce::jlimit(MIN_NOTE, MAX_NOTE, note);
}

int NoteEditor::xToStartSample(int x) const {
  int gridWidth = gridRight - gridLeft;
  if (gridWidth <= 0 || notes.empty())
    return 0;

  int maxSample = 0;
  for (const auto &note : notes) {
    if (note.endSample > maxSample)
      maxSample = note.endSample;
  }

  float ratio = (float)(x - gridLeft) / gridWidth;
  return (int)(ratio * maxSample);
}

int NoteEditor::xToEndSample(int x) const { return xToStartSample(x); }

int NoteEditor::sampleToX(int sample) const {
  int gridWidth = gridRight - gridLeft;
  if (gridWidth <= 0 || notes.empty())
    return gridLeft;

  int maxSample = 0;
  for (const auto &note : notes) {
    if (note.endSample > maxSample)
      maxSample = note.endSample;
  }

  if (maxSample == 0)
    return gridLeft;

  float ratio = (float)sample / maxSample;
  return gridLeft + (int)(ratio * gridWidth);
}

int NoteEditor::findNoteAt(int x, int y) const {
  if (notes.empty())
    return -1;

  int noteNum = yToNote(y);
  int startSample = xToStartSample(x);

  for (size_t i = 0; i < notes.size(); ++i) {
    const auto &note = notes[i];

    // Check if Y matches (same pitch)
    if (note.noteNumber != noteNum)
      continue;

    // Check if X is within note's time range
    if (startSample >= note.startSample && startSample <= note.endSample) {
      return (int)i;
    }
  }

  return -1;
}
