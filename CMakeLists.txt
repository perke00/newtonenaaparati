cmake_minimum_required(VERSION 3.22)
project(RoboticTune VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RoboticTune
    COMPANY_NAME "ProdByPerke"
    PLUGIN_MANUFACTURER_CODE Pbpk
    PLUGIN_CODE RbTn
    FORMATS VST3
    PRODUCT_NAME "Robotic Tune"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx)

target_sources(RoboticTune PRIVATE
    Source/Dsp.cpp
    Source/PluginProcessor.cpp
    Source/PianoRoll.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(RoboticTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RoboticTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)
