#include "PluginProcessor.h"
#include "PluginEditor.h"

Sample2MidiAudioProcessor::Sample2MidiAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
}

Sample2MidiAudioProcessor::~Sample2MidiAudioProcessor() {}

void Sample2MidiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {}
void Sample2MidiAudioProcessor::releaseResources() {}

void Sample2MidiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear(); // Plugin is an effect but we don't pass thru audio unless playing
}

bool Sample2MidiAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* Sample2MidiAudioProcessor::createEditor() { return new Sample2MidiAudioProcessorEditor (*this); }

void Sample2MidiAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void Sample2MidiAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

void Sample2MidiAudioProcessor::loadAndAnalyze(const juce::File& file) {
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor(file));
    if (reader == nullptr) return;

    juce::AudioBuffer<float> buffer (1, (int)reader->lengthInSamples);
    reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, false);

    double sampleRate = reader->sampleRate;
    int windowSize = 2048;
    int hopSize = 512;
    
    std::vector<RawNoteEvent> rawEvents;
    const float* data = buffer.getReadPointer(0);

    for (int i = 0; i < buffer.getNumSamples() - windowSize; i += hopSize) {
        // RMS gate for silence
        float sum = 0;
        for (int j = 0; j < windowSize; ++j) sum += data[i + j] * data[i + j];
        float rms = std::sqrt(sum / windowSize);

        if (rms > 0.01f) {
            float freq = pitchDetector.detectPitch(data + i, windowSize, sampleRate);
            if (freq > 0) {
                int midi = (int)std::round(69.0 + 12.0 * std::log2(freq / 440.0));
                rawEvents.push_back({midi, (double)i / sampleRate, rms});
            }
        }
    }

    detectedNotes = midiBuilder.assembleNotes(rawEvents, sampleRate);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new Sample2MidiAudioProcessor();
}
