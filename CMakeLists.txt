# JUCE Anagram LV2 Wrapper
# Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
# SPDX-License-Identifier: ISC

# store current source dir for later usage
option(JUCE_ANAGRAM_LV2_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "Root directory of the JUCE Anagram LV2 Wrapper project.")

# ---------------------------------------------------------------------------------------------------------------------
# Custom JUCE LV2 Wrapper for Darkglass Anagram
#
# Example project `CMakeLists.txt`:
#
# ```
# juce_add_plugin(MyPlugin ...)
#
# add_subdirectory(juce-alv2)
# juce_anagram_lv2_setup(MyPlugin
#   ENABLE_LATENCY
#   ENABLE_TIMEPOS)
# ```
# Make sure LV2 format is enabled in the `juce_add_plugin` step!

include(CMakeParseArguments)

# juce_anagram_lv2_setup(target <args...>)
# ---------------------------------------------------------------------------------------------------------------------
#
# Setup JUCE plugin to use our custom LV2 wrapper for Anagram.
#
# ---------------------------------------------------------------------------------------------------------------------
# Arguments:
#
#   `ENABLE_LATENCY`
#       enable latency control port
#
#   `ENABLE_FREEWHEEL`
#       enable free-wheel control port (offline mode)
#
#   `ENABLE_TIMEPOS`
#       enable receiving time position information through an LV2 Atom Sequence port
#
function(juce_anagram_lv2_setup TARGET)
  set(options ENABLE_LATENCY ENABLE_FREEWHEEL ENABLE_TIMEPOS)
  set(oneValueArgs TODO1)
  set(multiValueArgs TODO2)
  cmake_parse_arguments(_anagram_juce_plugin "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # disable superfulous Linux deps that we will never use, use system libs
  if(CMAKE_CROSSCOMPILING_EMULATOR)
    find_package(PkgConfig)
    pkg_check_modules(zlib IMPORTED_TARGET zlib REQUIRED)
    pkg_check_modules(libpng IMPORTED_TARGET libpng REQUIRED)

    target_compile_definitions(${TARGET}
      PUBLIC
        JUCE_ALSA=0
        JUCE_JACK=0
        JUCE_USE_CURL=0
        JUCE_USE_XCURSOR=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XRANDR=0
        JUCE_USE_XSHM=0
        JUCE_INCLUDE_PNGLIB_CODE=0
        JUCE_INCLUDE_ZLIB_CODE=0
    )

    target_link_libraries(${TARGET}
      PUBLIC
        PkgConfig::libpng
        PkgConfig::zlib
    )
  endif()

  # add ourselves to the include directories, so we can override JUCE files
  target_include_directories(${TARGET}
    PUBLIC
      "${JUCE_ANAGRAM_LV2_DIR}"
  )

  # disable official JUCE LV2 wrapper
  target_compile_options(${TARGET}_LV2
    PUBLIC
      -UJucePlugin_Build_LV2
      -DJucePlugin_Build_LV2=0
  )

  # use our custom Anagram LV2 wrapper
  target_sources(${TARGET}_LV2
    PUBLIC
      "${JUCE_ANAGRAM_LV2_DIR}/juce_audio_plugin_client_Anagram_LV2.cpp"
  )

  # do not use "lib" prefix for plugin binaries
  set_target_properties(${TARGET}_LV2 PROPERTIES PREFIX "")
endfunction()

# ---------------------------------------------------------------------------------------------------------------------
