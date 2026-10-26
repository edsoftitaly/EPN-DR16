cmake_minimum_required(VERSION 3.22)
project(EPN_DR16 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

# Use a local JUCE checkout if given (-DJUCE_DIR=...), otherwise fetch it.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout")
if(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

set(DR16_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND DR16_FORMATS AU)
endif()

juce_add_plugin(EPN_DR16
  COMPANY_NAME "EPN Audio"
  COMPANY_WEBSITE "https://www.instagram.com/eddiepaulnaples"
  PLUGIN_MANUFACTURER_CODE Epna
  PLUGIN_CODE Dr16
  FORMATS ${DR16_FORMATS}
  PRODUCT_NAME "EPN DR-16"
  BUNDLE_ID com.epnaudio.dr16
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Reverb
  AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(EPN_DR16 PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_include_directories(EPN_DR16 PRIVATE Source)

target_compile_definitions(EPN_DR16 PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EPN_DR16
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Offline DSP verification (no JUCE)
add_executable(dr16_tests tests/render.cpp)
target_include_directories(dr16_tests PRIVATE Source)

# Dev tool: render the editor to PNG (not shipped)
option(DR16_TOOLS "Build dev tools" OFF)
if(DR16_TOOLS)
  juce_add_console_app(dr16_snapshot PRODUCT_NAME "dr16_snapshot")
  target_sources(dr16_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(dr16_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="EPN DR-16")
  target_link_libraries(dr16_snapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_basics)
endif()
