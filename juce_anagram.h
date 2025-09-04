// JUCE Anagram LV2 Wrapper
// Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
// SPDX-License-Identifier: ISC

#include <juce_core/juce_core.h>

namespace anagram
{

// JUCE equivalent of an LV2 ScalePoint
// See http://lv2plug.in/ns/lv2core#ScalePoint
struct AudioParameterScalePoint
{
    juce::String label;
    float value;
};

// Class for attaching LV2 scalepoints to a parameter
// This allows to give special meaning to a value without having to ask the plugin binary,
// which Anagram cannot do due to its DSP/UI separation architecture
class AudioParameterWithScalePoints
{
public:
    virtual ~AudioParameterWithScalePoints() {};
    virtual juce::Array<AudioParameterScalePoint> getAllScalePoints() const = 0;
};

}
