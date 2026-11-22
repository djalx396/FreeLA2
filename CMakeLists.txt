cmake_minimum_required(VERSION 3.22)
project(FreeLA2 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

# Build number shown in the GUI. GitHub Actions passes its run number: -DFREELA2_BUILD_NUMBER=<n>
set(FREELA2_BUILD_NUMBER "local" CACHE STRING "Build number shown in the GUI")

# JUCE: use ./JUCE if present, otherwise fetch it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FLA2_FORMATS VST3)
if(APPLE)
    list(APPEND FLA2_FORMATS AU)
endif()

juce_add_plugin(FreeLA2
    COMPANY_NAME "Chris"
    BUNDLE_ID "com.chris.freela2"
    PLUGIN_MANUFACTURER_CODE Chrs
    PLUGIN_CODE FLa2
    FORMATS ${FLA2_FORMATS}
    PRODUCT_NAME "FreeLA2"
    DESCRIPTION "Optical leveling amplifier: Pre > Leveler > Clipper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FreeLA2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FreeLA2 PUBLIC
    FREELA2_BUILD="${FREELA2_BUILD_NUMBER}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FreeLA2
    PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Local tests (not built on GitHub): cmake -DFLA2_BUILD_TESTS=ON
option(FLA2_BUILD_TESTS "Build the local plug-in test" OFF)
if(FLA2_BUILD_TESTS)
    juce_add_console_app(FreeLA2Test PRODUCT_NAME "FreeLA2Test")
    target_sources(FreeLA2Test PRIVATE tests/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(FreeLA2Test PRIVATE FREELA2_BUILD="test" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="FreeLA2" JucePlugin_VersionString="0.1.0" JucePlugin_Build_Standalone=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FreeLA2Test PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
