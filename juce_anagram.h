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

// Class for declaring a parameter as non-automable if mono+stereo combo is used and plugin is currently mono
template <class BaseParameterClass>
class AudioParameterDisabledInMono : public BaseParameterClass
{
public:
    // AudioParameterBool
    AudioParameterDisabledInMono (const juce::ParameterID& parameterID,
                                  const juce::String& parameterName,
                                  bool defaultValue,
                                  const juce::AudioParameterBoolAttributes& attributes = {})
        : BaseParameterClass(parameterID, parameterName, defaultValue, attributes) {}

    // AudioParameterChoice
    AudioParameterDisabledInMono (const juce::ParameterID& parameterID,
                                  const juce::String& parameterName,
                                  const juce::StringArray& choices,
                                  int defaultItemIndex,
                                  const juce::AudioParameterChoiceAttributes& attributes = {})
        : BaseParameterClass(parameterID, parameterName, choices, defaultItemIndex, attributes) {}

    // AudioParameterInt
    AudioParameterDisabledInMono (const juce::ParameterID& parameterID,
                                  const juce::String& parameterName,
                                  int minValue,
                                  int maxValue,
                                  int defaultValue,
                                  const juce::AudioParameterIntAttributes& attributes = {})
        : BaseParameterClass(parameterID, parameterName, minValue, maxValue, defaultValue, attributes) {}

    // AudioParameterFloat
    AudioParameterDisabledInMono(const juce::ParameterID& parameterID,
                                 const juce::String& parameterName,
                                 juce::NormalisableRange<float> normalisableRange,
                                 float defaultValue,
                                 const juce::AudioParameterFloatAttributes& attributes = {})
        : BaseParameterClass(parameterID, parameterName, normalisableRange, defaultValue, attributes) {}

    // AudioParameterFloat
    AudioParameterDisabledInMono(const juce::ParameterID& parameterID,
                                 const juce::String& parameterName,
                                 float minValue,
                                 float maxValue,
                                 float defaultValue)
        : BaseParameterClass(parameterID, parameterName, minValue, maxValue, defaultValue) {}

    bool isAutomatable() const final
    {
        return _automatable;
    }

    void setAutomatable(bool shouldBeAutomatable)
    {
        _automatable = shouldBeAutomatable;
    }

private:
    bool _automatable = true;
};

}
