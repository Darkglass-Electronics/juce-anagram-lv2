# Example plugin for JUCE Anagram LV2 Wrapper
# Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
# SPDX-License-Identifier: ISC

# minimum CMake version, JUCE8 requires xx so we match it here
cmake_minimum_required(VERSION 3.15)

# C and C++ standards to use
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Hide symbols by default
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Setup project languages and version
project(DarkglassExample
    LANGUAGES C CXX
    VERSION 0.0.1
)

# Fetch and import JUCE during build, as to not store it in this repository
# You will likely want to have it locally as either a copy or git submodule
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Import the custom Anagram LV2 Wrapper
# You will likely want to use `add_subdirectory` instead
set(JUCE_ANAGRAM_LV2_DIR "${PROJECT_SOURCE_DIR}/..")
include("${PROJECT_SOURCE_DIR}/../CMakeLists.txt")

# Setup a JUCE plugin the usual way
juce_add_plugin(DarkglassExample
    COMPANY_NAME "Darkglass Electronics Oy"
    COMPANY_WEBSITE "https://github.com/Darkglass-Electronics/juce-alv2"
    PRODUCT_NAME "Example"
    BUNDLE_ID "com.darkglass.anagram-lv2-example"
    COPY_PLUGIN_AFTER_BUILD FALSE
    DESCRIPTION "Example plugin for JUCE Anagram LV2 Wrapper"
    FORMATS LV2
    IS_MIDI_EFFECT FALSE
    IS_SYNTH FALSE
    LV2_URI "https://github.com/Darkglass-Electronics/juce-alv2#example"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

# Enable the custom Anagram LV2 wrapper
juce_anagram_lv2_setup(DarkglassExample)

# Set extra compile definitions to reduce features and thus our binary size too
target_compile_definitions(DarkglassExample
    PUBLIC
        # disable audio codecs that we will never use
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        # disable other stuff
        JUCE_WEB_BROWSER=0
)

# Use JUCE recommended flags
#target_link_libraries(DarkglassExample
    #PUBLIC
        #juce::juce_recommended_config_flags
        #juce::juce_recommended_lto_flags
        #juce::juce_recommended_warning_flags
#)

target_sources(DarkglassExample
    PRIVATE
        PluginProcessor.cpp
)

#LV2_CATEGORIES "Instrument Synth Stereo"

#PLUGIN_CODE Expl
#PLUGIN_MANUFACTURER_CODE Drkg
