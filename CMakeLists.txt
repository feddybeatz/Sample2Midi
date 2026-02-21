cmake_minimum_required(VERSION 3.22)
project(Sample2MIDI VERSION 1.0.0 LANGUAGES C CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

add_subdirectory(JUCE)

# Add RTNeural from ThirdParty
if(EXISTS ${CMAKE_SOURCE_DIR}/ThirdParty/RTNeural/CMakeLists.txt)
    add_subdirectory(ThirdParty/RTNeural)
endif()

juce_add_plugin(Sample2MIDI
    COMPANY_NAME "FeddyBeatz"
    PLUGIN_MANUFACTURER_CODE Fedb
    PLUGIN_CODE S2Mi
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sample2MIDI"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VIRTUAL_IO_CHANNELS 2 2
    VST3_COPY_DIR "$ENV{PROGRAMFILES}/Common Files/VST3"
)

# Binary data - include model files
file(GLOB_RECURSE MODEL_FILES 
    ${CMAKE_SOURCE_DIR}/Models/ModelData/*.json
    ${CMAKE_SOURCE_DIR}/Models/ModelData/*.onnx
    ${CMAKE_SOURCE_DIR}/Models/ModelData/*.ort
)

juce_add_binary_data(bin_data SOURCES ${MODEL_FILES})

# Build BasicPitchCNN as static library if RTNeural is available
if(EXISTS ${CMAKE_SOURCE_DIR}/ThirdParty/RTNeural/CMakeLists.txt)
    add_library(BasicPitchCNN STATIC 
        NeuralModel/BasicPitchCNN.cpp
    )
    target_include_directories(BasicPitchCNN PRIVATE 
        ${CMAKE_SOURCE_DIR}/ThirdParty/RTNeural
        ${CMAKE_SOURCE_DIR}/NeuralModel
    )
    target_link_libraries(BasicPitchCNN PUBLIC RTNeural bin_data)
endif()

target_sources(Sample2MIDI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PitchDetector.cpp
    Source/PitchDetector.h
    Source/MidiBuilder.cpp
    Source/MidiBuilder.h
    Source/NoteEditor.cpp
    Source/NoteEditor.h
    Source/WaveformDisplay.cpp
    Source/WaveformDisplay.h
    Source/AudioFileLoader.cpp
    Source/AudioFileLoader.h
    Source/ScaleQuantizer.cpp
    Source/ScaleQuantizer.h
    Source/SpectralDisplay.cpp
    Source/SpectralDisplay.h
    # Neural Model sources (if not using BasicPitchCNN static lib)
    NeuralModel/BasicPitch.cpp
    NeuralModel/BasicPitch.h
    NeuralModel/Features.cpp
    NeuralModel/Features.h
    NeuralModel/Notes.cpp
    NeuralModel/Notes.h
)

# Include directories for NeuralModel
target_include_directories(Sample2MIDI PRIVATE
    ${CMAKE_SOURCE_DIR}/NeuralModel
    ${CMAKE_SOURCE_DIR}/onnxruntime/include
)

target_compile_definitions(Sample2MIDI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # Neural model defines
    USE_TEST_NOTE_FRAME_TO_TIME=0
    # Suppress Windows min/max macro conflicts
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)

# MSVC-specific compiler flags
if(MSVC)
    target_compile_options(Sample2MIDI PRIVATE
        /W3          # Warning level 3
        /MP          # Multi-processor compilation
        /EHsc        # C++ exception handling
        /permissive- # Strict conformance
    )
    # Suppress specific MSVC warnings that JUCE triggers
    target_compile_options(Sample2MIDI PRIVATE
        /wd4100  # unreferenced formal parameter
        /wd4244  # conversion, possible loss of data
        /wd4267  # size_t to int conversion
        /wd4996  # deprecated declarations
        /wd4305  # truncation: from 'type' to 'type'
    )
endif()

# ONNX Runtime - skip if not found
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
set(ONNX_FOUND FALSE)

# Check if ONNX Runtime lib exists - try multiple possible names
if(WIN32)
    # Try different possible library names
    if(EXISTS "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
        set(ONNX_LIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
        set(ONNX_FOUND TRUE)
        message(STATUS "Found ONNX Runtime library: ${ONNX_LIB}")
    elseif(EXISTS "${ONNXRUNTIME_DIR}/lib/onnxruntime")
        set(ONNX_LIB "${ONNXRUNTIME_DIR}/lib/onnxruntime")
        set(ONNX_FOUND TRUE)
        message(STATUS "Found ONNX Runtime library: ${ONNX_LIB}")
    else()
        message(WARNING "ONNX Runtime library not found in: ${ONNXRUNTIME_DIR}/lib/")
        message(WARNING "Download from: https://github.com/microsoft/onnxruntime/releases/tag/v1.17.1")
        message(WARNING "Copy onnxruntime.lib -> onnxruntime/lib/")
    endif()
else()
    if(EXISTS "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so.1.17.1")
        set(ONNX_FOUND TRUE)
        message(STATUS "Found ONNX Runtime library")
    endif()
endif()

# Configure ONNX only if found
if(ONNX_FOUND)
    include_directories(${ONNXRUNTIME_DIR}/include)
    link_directories(${ONNXRUNTIME_DIR}/lib)
    
    # Add onnxruntime library - link by full path for Windows
    if(WIN32)
        add_library(onnxruntime STATIC IMPORTED)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION "${ONNX_LIB}"
            IMPORTED_LOCATION_DEBUG "${ONNX_LIB}"
            IMPORTED_LOCATION_RELEASE "${ONNX_LIB}"
        )
    elseif(UNIX)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so.1.17.1"
        )
        set_target_properties(Sample2MIDI PROPERTIES
            BUILD_RPATH "${ONNXRUNTIME_DIR}/lib"
            INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib"
        )
    endif()
endif()

target_link_libraries(Sample2MIDI PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    bin_data
)

# Link ONNX Runtime only if found
if(ONNX_FOUND)
    target_link_libraries(Sample2MIDI PRIVATE onnxruntime)
    # ONNX Runtime on Windows may need these system libs
    if(WIN32)
        target_link_libraries(Sample2MIDI PRIVATE "winmm.lib" "ws2_32.lib")
    endif()
endif()

# Link RTNeural and BasicPitchCNN if available
if(EXISTS ${CMAKE_SOURCE_DIR}/ThirdParty/RTNeural/CMakeLists.txt)
    target_link_libraries(Sample2MIDI PRIVATE
        RTNeural
        BasicPitchCNN
    )
endif()

juce_generate_juce_header(Sample2MIDI)
