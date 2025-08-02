// JUCE Anagram LV2 Wrapper
// Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
// SPDX-License-Identifier: ISC

#undef JucePlugin_Build_LV2
#define JucePlugin_Build_LV2 1

#include_next <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>

#undef JucePlugin_Build_LV2
#define JucePlugin_Build_LV2 0
