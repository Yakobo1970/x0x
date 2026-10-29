cmake_minimum_required(VERSION 3.22)
project(x0x VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "$ENV{USERPROFILE}/JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(x0x
    COMPANY_NAME "Un(d)AbtanzBAR"
    PRODUCT_NAME "x0x"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Yako
    PLUGIN_CODE X0xA
    VST3_CATEGORIES Instrument Synth Drum
    ICON_BIG assets/icon_1024.png
    ICON_SMALL assets/icon_32.png
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(x0x PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AboutOverlay.cpp
    Source/Patterns.cpp
    Source/RbsImport.cpp
    Source/MidiExport.cpp
    Source/dsp/BassVoice.cpp
    Source/dsp/DrumKit.cpp
    Source/dsp/Engine.cpp)

target_compile_definitions(x0x PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if (MSVC)
    target_compile_options(x0x PRIVATE /utf-8)
endif()

target_link_libraries(x0x
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline regression renderer (not part of the plugin)
juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
target_sources(RenderTest PRIVATE
    Tools/RenderTest.cpp
    Source/Patterns.cpp
    Source/RbsImport.cpp
    Source/MidiExport.cpp
    Source/dsp/BassVoice.cpp
    Source/dsp/DrumKit.cpp
    Source/dsp/Engine.cpp)
target_compile_definitions(RenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(RenderTest PRIVATE juce::juce_audio_formats juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# Minimal VST3 host test (loads the built plugin like a DAW)
juce_add_console_app(HostTest PRODUCT_NAME "HostTest")
target_sources(HostTest PRIVATE Tools/HostTest.cpp)
target_compile_definitions(HostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(HostTest PRIVATE juce::juce_audio_processors juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Renders the editor to a PNG (UI checks without a DAW)
juce_add_gui_app(UiShot PRODUCT_NAME "UiShot")
target_sources(UiShot PRIVATE
    Tools/UiShot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AboutOverlay.cpp
    Source/Patterns.cpp
    Source/RbsImport.cpp
    Source/MidiExport.cpp
    Source/dsp/BassVoice.cpp
    Source/dsp/DrumKit.cpp
    Source/dsp/Engine.cpp)
target_compile_definitions(UiShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="x0x" JucePlugin_VersionString="${PROJECT_VERSION}")
if (MSVC)
    target_compile_options(UiShot PRIVATE /utf-8)
endif()
target_link_libraries(UiShot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
