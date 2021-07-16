cmake_minimum_required(VERSION 3.15)

project(Amorphetude VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(Amorphetude
    COMPANY_NAME "tycho"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Nodl
    FORMATS VST3
    PRODUCT_NAME "Amorphetude")

juce_generate_juce_header(Amorphetude)

target_sources(Amorphetude PRIVATE
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

target_compile_definitions(Amorphetude
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Amorphetude PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp)
