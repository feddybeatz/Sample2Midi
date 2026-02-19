cmake_minimum_required(VERSION 3.22)
project(Sample2MIDI VERSION 1.0.0 LANGUAGES C CXX)

# Add JUCE
add_subdirectory(JUCE)

juce_add_plugin(Sample2MIDI
    COMPANY_NAME "FeddyBeatz"
    PLUGIN_MANUFACTURER_CODE Fedb
    PLUGIN_CODE S2Mi
    FORMATS VST3
    PRODUCT_NAME "Sample2MIDI"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VIRTUAL_IO_CHANNELS 2 2
)

target_sources(Sample2MIDI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PitchDetector.cpp
    Source/PitchDetector.h
    Source/MidiBuilder.cpp
    Source/MidiBuilder.h
    Source/WaveformDisplay.cpp
    Source/WaveformDisplay.h
    Source/AudioFileLoader.cpp
    Source/AudioFileLoader.h
    Source/ScaleQuantizer.cpp
    Source/ScaleQuantizer.h
)

target_compile_definitions(Sample2MIDI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Sample2MIDI PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_audio_formats
    juce::juce_dsp
)

juce_generate_juce_header(Sample2MIDI)
