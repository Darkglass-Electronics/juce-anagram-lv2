# JUCE Anagram LV2 Wrapper
# Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
# SPDX-License-Identifier: ISC

# store current source dir for later usage
set(JUCE_ANAGRAM_LV2_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE STRING "Root directory of the JUCE Anagram LV2 Wrapper")

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
#   BLOCK_IMAGE_OFF "fx-off.png"
#   BLOCK_IMAGE_ON "fx-on.png"
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
#   `BLOCK_IMAGE_OFF`
#       path to a in-bundle 200x200 PNG image file to be used as the "off" plugin block image
#
#   `BLOCK_IMAGE_ON`
#       path to a in-bundle 200x200 PNG image file to be used as the "on" plugin block image
#
#   `CATEGORY`
#       a string URI for an LV2 category, can use "lv2:" suffix (e.g. "lv2:UtilityPlugin")
#
#   `ENABLE_FREEWHEEL`
#       enable free-wheel control port (offline mode)
#
#   `ENABLE_LATENCY`
#       enable latency control port (reporting latency to host)
#
function(juce_anagram_lv2_setup TARGET)
  set(options ENABLE_LATENCY ENABLE_FREEWHEEL)
  set(oneValueArgs BLOCK_IMAGE_OFF BLOCK_IMAGE_ON CATEGORY)
  set(multiValueArgs TODO)
  cmake_parse_arguments(_anagram_juce_plugin "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # disable superfulous Linux deps that we will never use, use system libs
  if(CMAKE_CROSSCOMPILING_EMULATOR)
    find_package(PkgConfig)
    pkg_check_modules(libpng IMPORTED_TARGET libpng REQUIRED)
    pkg_check_modules(zlib IMPORTED_TARGET zlib REQUIRED)

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
  get_target_property(module_path juce::juce_audio_plugin_client INTERFACE_JUCE_MODULE_PATH)
  target_compile_definitions(${TARGET}
    PUBLIC
      juce_CheckSettingMacros_h="${module_path}/juce_audio_plugin_client/detail/juce_CheckSettingMacros.h"
  )
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

  # custom LV2 wrapper arguments
  if (_anagram_juce_plugin_BLOCK_IMAGE_OFF)
    target_compile_definitions(${TARGET}_LV2 PUBLIC JucePlugin_LV2BlockImageOff="${_anagram_juce_plugin_BLOCK_IMAGE_OFF}")
  endif()
  if (_anagram_juce_plugin_BLOCK_IMAGE_ON)
    target_compile_definitions(${TARGET}_LV2 PUBLIC JucePlugin_LV2BlockImageOn="${_anagram_juce_plugin_BLOCK_IMAGE_ON}")
  endif()
  if (_anagram_juce_plugin_CATEGORY)
    target_compile_definitions(${TARGET}_LV2 PUBLIC JucePlugin_LV2Category="${_anagram_juce_plugin_CATEGORY}")
  endif()
  if (_anagram_juce_plugin_ENABLE_FREEWHEEL)
    target_compile_definitions(${TARGET}_LV2 PUBLIC JucePlugin_LV2WantsFreeWheel=1)
  endif()
  if (_anagram_juce_plugin_ENABLE_LATENCY)
    target_compile_definitions(${TARGET}_LV2 PUBLIC JucePlugin_LV2WantsLatency=1)
  endif()

  # do not use "lib" prefix for plugin binaries
  set_target_properties(${TARGET}_LV2 PROPERTIES PREFIX "")

  # do not use spaces in output bundle and binary name
  get_target_property(PRODUCT_NAME_NOSPACES ${TARGET} JUCE_PRODUCT_NAME)
  string(REPLACE " " "_" PRODUCT_NAME_NOSPACES "${PRODUCT_NAME_NOSPACES}")
  set_target_properties(${TARGET}_LV2 PROPERTIES OUTPUT_NAME "${PRODUCT_NAME_NOSPACES}")

  get_target_property(LIBRARY_OUTPUT_DIRECTORY ${TARGET} LIBRARY_OUTPUT_DIRECTORY)
  set(LV2_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIRECTORY}/LV2/${PRODUCT_NAME_NOSPACES}.lv2")
  set_target_properties(${TARGET}_LV2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${LV2_OUTPUT_DIRECTORY}")

  # copy block images to LV2 bundle
  if (_anagram_juce_plugin_BLOCK_IMAGE_OFF)
    add_custom_command(TARGET ${TARGET}_LV2 POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}"
        "-E"
        "copy"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_anagram_juce_plugin_BLOCK_IMAGE_OFF}"
        "${LV2_OUTPUT_DIRECTORY}/${_anagram_juce_plugin_BLOCK_IMAGE_OFF}"
      VERBATIM
    )
  endif()
  if (_anagram_juce_plugin_BLOCK_IMAGE_ON)
    add_custom_command(TARGET ${TARGET}_LV2 POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}"
        "-E"
        "copy"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_anagram_juce_plugin_BLOCK_IMAGE_ON}"
        "${LV2_OUTPUT_DIRECTORY}/${_anagram_juce_plugin_BLOCK_IMAGE_ON}"
      VERBATIM
    )
  endif()
endfunction()

# ---------------------------------------------------------------------------------------------------------------------
