#include "MidiBuilder.h"
#include <juce_gui_basics/juce_gui_basics.h>

std::vector<CleanNote> MidiBuilder::assembleNotes(const std::vector<RawNoteEvent>& rawEvents, double sampleRate) {
    std::vector<CleanNote> cleanNotes;
    if (rawEvents.empty()) return cleanNotes;

    int currentMidi = -1;
    double startTime = 0;
    float maxVel = 0;

    for (size_t i = 0; i < rawEvents.size(); ++i) {
        const auto& event = rawEvents[i];

        if (event.midiNote != currentMidi) {
            // Finish previous note
            if (currentMidi != -1) {
                double endTime = event.timestamp;
                if ((endTime - startTime) >= MIN_NOTE_DURATION) {
                    cleanNotes.push_back({currentMidi, startTime, endTime, maxVel});
                }
            }
            // Start new note
            currentMidi = event.midiNote;
            startTime = event.timestamp;
            maxVel = event.velocity;
        } else {
            maxVel = std::max(maxVel, event.velocity);
        }
    }

    // Final note
    if (currentMidi != -1) {
        cleanNotes.push_back({currentMidi, startTime, rawEvents.back().timestamp + 0.05, maxVel});
    }

    return cleanNotes;
}

void MidiBuilder::exportMidi(const std::vector<CleanNote>& notes, const juce::File& file) {
    juce::MidiFile midiFile;
    juce::MidiMessageSequence seq;
    
    // Default 960 ticks per quarter note
    double ticksPerSecond = 960.0 * (120.0 / 60.0); // Assuming 120 BPM for standard export

    for (const auto& note : notes) {
        auto on = juce::MidiMessage::noteOn(1, note.midiNote, (juce::uint8)juce::jlimit(0, 127, (int)(note.velocity * 127)));
        auto off = juce::MidiMessage::noteOff(1, note.midiNote);
        
        seq.addEvent(on, note.startTime * ticksPerSecond);
        seq.addEvent(off, note.endTime * ticksPerSecond);
    }
    
    midiFile.setTicksPerQuarterNote(960);
    midiFile.addTrack(seq);

    juce::FileOutputStream stream(file);
    if (stream.openedOk()) {
        midiFile.writeTo(stream, 1);
    }
}

void MidiBuilder::performDragDrop(const std::vector<CleanNote>& notes) {
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File tempFile = tempDir.getChildFile("Sample2MIDI_Export.mid");
    
    if (tempFile.existsAsFile()) tempFile.deleteFile();
    
    exportMidi(notes, tempFile);
    
    if (tempFile.existsAsFile()) {
        juce::DragAndDropContainer::performExternalDragDropOfFiles({tempFile.getFullPathName()}, false);
    }
}
