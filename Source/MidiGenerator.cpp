#include "MidiGenerator.h"

juce::MidiMessageSequence MidiGenerator::generateSequence(const std::vector<DetectedNote>& notes)
{
    juce::MidiMessageSequence seq;
    for (const auto& note : notes)
    {
        auto on = juce::MidiMessage::noteOn(1, note.midiNote, (juce::uint8)juce::jlimit(0, 127, (int)(note.velocity * 127)));
        auto off = juce::MidiMessage::noteOff(1, note.midiNote);
        
        on.setTimeStamp(note.startTime);
        off.setTimeStamp(note.endTime);
        
        seq.addEvent(on);
        seq.addEvent(off);
    }
    seq.updateMatchedPairs();
    return seq;
}

void MidiGenerator::exportToMidiFile(const std::vector<DetectedNote>& notes, const juce::File& file)
{
    juce::MidiFile midiFile;
    auto seq = generateSequence(notes);
    midiFile.setTicksPerQuarterNote(960);
    midiFile.addTrack(seq);

    juce::FileOutputStream out(file);
    if (out.openedOk())
    {
        midiFile.writeTo(out, 1);
    }
}
