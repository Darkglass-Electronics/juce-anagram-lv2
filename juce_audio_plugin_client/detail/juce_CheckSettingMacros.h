// JUCE Anagram LV2 Wrapper
// Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
// SPDX-License-Identifier: ISC

// This file exists because we purposefully disable the official JUCE LV2 Wrapper,
// which can only be done by setting JucePlugin_Build_LV2 to 0.
// When we do this we (unintentionally) also fully disable LV2 entirely, which is not what we want..
// So here we force the LV2 build to be enabled for just the 1 single header where it is checked by JUCE.

#undef JucePlugin_Build_LV2
#define JucePlugin_Build_LV2 1

#ifdef juce_CheckSettingMacros_h
#include juce_CheckSettingMacros_h
#else
#include_next <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#endif

#undef JucePlugin_Build_LV2
#define JucePlugin_Build_LV2 0
