# JUCE Anagram LV2 Wrapper

This repository contains a custom JUCE LV2 Wrapper specially created for Anagram.

## Why

While JUCE (since version 7) officially supports LV2, it uses "Patch Parameters" which is not supported in Anagram at the moment.

There are a couple of other details where we want Anagram-specific integration (for example, custom parameter units).

As an extra, being an external codebase, it is able to get updates on a regular basis without having to update the entire JUCE codebase.

## Changes

This main change for this custom alternative LV2 wrapper is the use of Control Ports instead of the new "Patch Parameter" style. Plus:

 - disables regular desktop UI (we do not use it)
 - only enables the specific features in use by the plugin (e.g. Atom port only enabled if plugin needs time events)
 - has custom Anagram-specific plugin and parameter meta-data
 - is able to get updates on a regular basis without having to update the entire JUCE codebase

## Usage

Assuming you are using CMake, setting up this custom wrapper is quite simple. Steps:

 - import the root of this repository into your CMakeLists.txt (either with `add_subdirectory` or `FetchContent_Declare`)
 - after your `juce_add_plugin(TARGET ...)` call `juce_anagram_lv2_setup(TARGET)`

Example of a CMakeLists.txt file:

```cmake
# Minimal setup
cmake_minimum_required(VERSION 3.22...3.31)
set(CMAKE_CXX_STANDARD 17)
project(MyPlugin)

# Enable JUCE
add_subdirectory(JUCE)

# Enable the custom Anagram LV2 Wrapper
add_subdirectory(juce-anagram-lv2)

# Setup a JUCE plugin the usual way
juce_add_plugin(MyPlugin
    # ...
)

# this function will do the custom setup
juce_anagram_lv2_setup(MyPlugin)

target_sources(MyPlugin
    PRIVATE
        PluginProcessor.cpp
)
```
