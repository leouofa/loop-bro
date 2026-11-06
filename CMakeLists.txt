cmake_minimum_required(VERSION 3.22)
project(LOOP_BRO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch JUCE 8.0.6
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(JUCE)

# Core DSP & File Export Library (Decoupled for headless unit testing)
add_library(LoopBroDSP STATIC
    src/dsp/CaptureEngine.cpp
    src/dsp/CaptureEngine.h
    src/dsp/EnvelopeFollower.h
    src/dsp/PreRollRingBuffer.h
    src/plugin/WavExporter.cpp
    src/plugin/WavExporter.h
)

target_include_directories(LoopBroDSP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LoopBroDSP PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
)

# Main Audio Plugin Target (AU, VST3, Standalone)
juce_add_plugin(LoopBro
    COMPANY_NAME "LoopBro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "LpBr"
    PLUGIN_CODE "LpBr"
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Loop Bro"
)

target_sources(LoopBro PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginProcessor.h
    src/plugin/PluginParameters.h
    src/ui/PluginEditor.cpp
    src/ui/PluginEditor.h
    src/ui/LevelMeterComponent.cpp
    src/ui/LevelMeterComponent.h
    src/ui/WaveformDragComponent.cpp
    src/ui/WaveformDragComponent.h
)

target_compile_definitions(LoopBro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LoopBro PRIVATE
    LoopBroDSP
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_events
    juce::juce_data_structures
)

# Automated Test Suite
enable_testing()

add_executable(LoopBroTests
    tests/LoopBroTests.cpp
)

target_link_libraries(LoopBroTests PRIVATE
    LoopBroDSP
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_core
)

add_test(NAME LoopBroTests COMMAND LoopBroTests)
