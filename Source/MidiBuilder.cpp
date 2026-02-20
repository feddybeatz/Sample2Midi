#include "MidiBuilder.h"
#include <juce_gui_basics/juce_gui_basics.h>

std::vector<MidiNote>
MidiBuilder::buildNotes(const std::vector<int> &framePitches,
                        const std::vector<float> &frameAmps, int hopSize,
                        double sampleRate) {
  std::vector<MidiNote> notes;
  if (framePitches.empty())
    return notes;

  int currentNote = -1;
  int startFrame = 0;
  float maxAmp = 0;

  auto finishNote = [&](int endFrame) {
    if (currentNote != -1 && currentNote > 0) {
      int startSample = startFrame * hopSize;
      int endSample = endFrame * hopSize;
      double durationMs =
          (double)(endSample - startSample) / sampleRate * 1000.0;
      if (durationMs >= MIN_NOTE_DURATION_MS) {
        notes.push_back(
            {startSample, endSample, currentNote,
             juce::jlimit(0.1f, 1.0f,
                          maxAmp * 2.0f)}); // boost velocity slightly
      }
    }
  };

  for (int i = 0; i < (int)framePitches.size(); ++i) {
    int pitch = framePitches[i];
    float amp = frameAmps[i];

    if (pitch != currentNote) {
      finishNote(i);
      currentNote = pitch;
      startFrame = i;
      maxAmp = amp;
    } else {
      maxAmp = std::max(maxAmp, amp);
    }
  }
  finishNote((int)framePitches.size());

  return notes;
}

void MidiBuilder::exportMidi(const std::vector<MidiNote> &notes,
                             double sampleRate, const juce::File &file,
                             float bpm) {
  juce::MidiFile midiFile;

  // Add tempo track with detected BPM
  int microsPerBeat = (int)(60000000.0 / bpm);
  juce::MidiMessageSequence tempoTrack;
  tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(microsPerBeat), 0);
  midiFile.addTrack(tempoTrack);

  juce::MidiMessageSequence seq;

  // Ticks per second based on detected BPM
  double ticksPerSecond = 960.0 * (bpm / 60.0);

  for (const auto &note : notes) {
    double startTimeSec = (double)note.startSample / sampleRate;
    double endTimeSec = (double)note.endSample / sampleRate;

    auto on = juce::MidiMessage::noteOn(
        1, note.noteNumber,
        (juce::uint8)juce::jlimit(0, 127, (int)(note.velocity * 127)));
    auto off = juce::MidiMessage::noteOff(1, note.noteNumber);

    seq.addEvent(on, startTimeSec * ticksPerSecond);
    seq.addEvent(off, endTimeSec * ticksPerSecond);
  }

  midiFile.setTicksPerQuarterNote(960);
  midiFile.addTrack(seq);

  juce::FileOutputStream stream(file);
  if (stream.openedOk()) {
    midiFile.writeTo(stream, 1);
  }
}

void MidiBuilder::performDragDrop(const std::vector<MidiNote> &notes,
                                  double sampleRate) {
  if (notes.empty())
    return;

  juce::File tempDir =
      juce::File::getSpecialLocation(juce::File::tempDirectory);
  juce::File tempFile = tempDir.getChildFile("Sample2MIDI_Export.mid");

  if (tempFile.existsAsFile())
    tempFile.deleteFile();

  exportMidi(notes, sampleRate, tempFile);

  if (tempFile.existsAsFile()) {
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        {tempFile.getFullPathName()}, false);
  }
}
