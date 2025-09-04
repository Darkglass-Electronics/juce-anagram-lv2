// JUCE Anagram LV2 Wrapper
// Copyright (C) 2025 Filipe Coelho <falktx@darkglass.com>
// SPDX-License-Identifier: ISC

// optional for testing
// #define ENABLE_JUCE_GUI

#ifdef ENABLE_JUCE_GUI
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/detail/juce_LinuxMessageThread.h>
#else
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#endif

#include "JuceLV2Defines.h"
#include <juce_audio_processors/format_types/juce_LV2Common.h>
#include <juce_audio_processors/format_types/juce_LegacyAudioParameter.cpp>

#include "juce_anagram.h"

#include <lv2/core/lv2_util.h>
#include <lv2/log/logger.h>

#include <fstream>

namespace juce::anagram_lv2_client
{

class JuceLv2Wrapper
{
public:
    // set to true if plugin initializes properly
    bool ok = false;

    JuceLv2Wrapper(double sampleRate, int32_t bufferSize, LV2_Log_Logger& logger, LV2_URID_Map* uridMap)
    {
        {
           #ifdef ENABLE_JUCE_GUI
            const MessageManagerLock mmLock;
           #endif
            filter = createPluginFilterOfType (AudioProcessor::wrapperType_LV2);
        }

        // Stop here if createPluginFilterOfType failed
        if (filter == nullptr)
        {
            lv2_log_error (&logger, "Failed to create plugin filter\n");
            return;
        }

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr int configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        filter->setPlayConfigDetails (configs[0][0], configs[0][1], sampleRate, bufferSize);
       #else
        filter->enableAllBuses();
       #endif
        filter->refreshParameterList();

        const Array<AudioProcessorParameter*>& parameters = filter->getParameters();

        numInputs = filter->getTotalNumInputChannels();
        numOutputs = filter->getTotalNumOutputChannels();
        numControls = parameters.size();
        bypassParameter = filter->getBypassParameter();

        // Stop here if filter has Anagram incompatible IO
        if (numInputs < 1 || numOutputs < 1 || numInputs > 2 || numOutputs > 2)
        {
            lv2_log_error (&logger, "Plugin filter has Anagram incompatible IO\n");
            return;
        }

        // Stop here if filter is missing bypass parameter
        if (bypassParameter == nullptr)
        {
            lv2_log_error (&logger, "Plugin filter is missing bypass parameter, required for Anagram\n");
            return;
        }

        // Stop here if filter has no parameters
        if (numControls <= 1)
        {
            lv2_log_error (&logger, "Plugin filter has no parameters, at least 1 is required for Anagram\n");
            return;
        }

        host.sampleRate = sampleRate;
        host.bufferSize = bufferSize;
        host.logger = logger;
        host.uridMap = uridMap;

        ports.audioIns.resize(static_cast<size_t> (numInputs));
        ports.audioOuts.resize(static_cast<size_t> (numOutputs));
        ports.controls.resize(static_cast<size_t> (numControls));
        lastControlValues.resize(static_cast<size_t> (numControls));

        for (int i = 0; i < numControls; ++i)
        {
            AudioProcessorParameter* const parameter = parameters.getUnchecked (i);

            if (auto* rangedParameter = dynamic_cast<const RangedAudioParameter*> (parameter))
                lastControlValues.setUnchecked(i, rangedParameter->convertFrom0to1 (rangedParameter->getValue()));
            else
                lastControlValues.setUnchecked(i, parameter->getValue());
        }

        ok = true;
    }

    ~JuceLv2Wrapper()
    {
    }

    void connect(int port, void* data)
    {
        if (port < numInputs)
        {
            ports.audioIns.setUnchecked(port, static_cast<const float*> (data));
            return;
        }
        port -= numInputs;

        if (port < numOutputs)
        {
            ports.audioOuts.setUnchecked(port, static_cast<float*> (data));
            return;
        }
        port -= numOutputs;

        if (port-- == 0)
        {
            ports.enabled = static_cast<const float*> (data);
            return;
        }

        if (port-- == 0)
        {
            ports.reset = static_cast<const float*> (data);
            return;
        }

       #if JucePlugin_LV2WantsFreeWheel
        if (port-- == 0)
        {
            ports.freeWheel = static_cast<const float*> (data);
            return;
        }
       #endif

       #if JucePlugin_LV2WantsLatency
        if (port-- == 0)
        {
            ports.latency = static_cast<float*> (data);
            return;
        }
       #endif

        if (port < numControls)
        {
            ports.controls.setUnchecked(port, static_cast<float*> (data));
            return;
        }
        // port -= numControls;
    }

    void activate()
    {
        filter->prepareToPlay (host.sampleRate, host.bufferSize);
        filter->setPlayConfigDetails (numInputs, numOutputs, host.sampleRate, host.bufferSize);

        audioBuffers.calloc (std::max (numInputs, numOutputs));
    }

    void deactivate()
    {
        audioBuffers.free();

        filter->releaseResources();
    }

    void run(int sampleCount)
    {
        if (ports.reset != nullptr && *ports.reset > 0.5f)
            filter->reset();

        if (ports.freeWheel != nullptr)
            filter->setNonRealtime (*ports.freeWheel > 0.5f);

        if (ports.latency != nullptr)
            *ports.latency = filter->getLatencySamples();

        if (sampleCount == 0)
        {
            // LV2 pre-roll
            // Hosts might use this to force plugins to update its output control ports.
            // (plugins can only access port locations during run)
            return;
        }

        // Check for updated parameters
        {
            const Array<AudioProcessorParameter*>& parameters = filter->getParameters();
            float value;

            for (int i = 0, offset = 0; i < numControls; ++i)
            {
                AudioProcessorParameter* const parameter = parameters.getUnchecked (i);

                if (parameter == bypassParameter)
                {
                    ++offset;
                    if (ports.enabled == nullptr)
                        continue;
                    value = 1.f - *ports.enabled;
                }
                else
                {
                    if (ports.controls.getUnchecked(i - offset) == nullptr)
                        continue;
                    value = *ports.controls.getUnchecked(i - offset);
                }

                if (approximatelyEqual(lastControlValues.getUnchecked(i), value))
                    continue;

                lastControlValues.setUnchecked(i, value);

                if (auto* rangedParameter = dynamic_cast<const RangedAudioParameter*> (parameter))
                    value = rangedParameter->convertTo0to1 (value);

                parameter->setValueNotifyingHost (value);
            }
        }

        // prepare audio buffers
        {
            int i;
            for (i = 0; i < numOutputs; ++i)
            {
                audioBuffers[i] = ports.audioOuts[i];

                if (i < numInputs && ports.audioIns[i] != ports.audioOuts[i])
                    FloatVectorOperations::copy (ports.audioOuts[i], ports.audioIns[i], sampleCount);
            }
            for (; i < numInputs; ++i)
                audioBuffers[i] = const_cast<float*>(ports.audioIns[i]);
        }

        // TODO send MIDI events as needed
        midiEvents.clear();

        // process filter
        {
            AudioSampleBuffer chans (audioBuffers, std::max (numInputs, numOutputs), sampleCount);

            const ScopedLock sl (filter->getCallbackLock());

            if (filter->isSuspended())
            {
                for (int i = 0; i < numOutputs; ++i)
                    FloatVectorOperations::clear (ports.audioOuts[i], sampleCount);
            }
            else
            {
                filter->processBlock (chans, midiEvents);
            }
        }
    }

private:
  #ifdef ENABLE_JUCE_GUI
    ScopedJuceInitialiser_GUI scopedJuceInitialiser;
   #if JUCE_LINUX || JUCE_BSD
    SharedResourcePointer<detail::MessageThread> messageThread;
   #endif
  #endif

    std::unique_ptr<AudioProcessor> filter;
    AudioProcessorParameter* bypassParameter = nullptr;
    int numInputs = 0;
    int numOutputs = 0;
    int numControls = 0;

    struct {
        double sampleRate;
        int32_t bufferSize;
        LV2_Log_Logger logger;
        LV2_URID_Map* uridMap;
    } host{};

    struct {
        Array<const float*> audioIns;
        Array<float*> audioOuts;
        Array<float*> controls;
        const float* enabled = nullptr;
        const float* reset = nullptr;
        const float* freeWheel = nullptr;
        float* latency = nullptr;
    } ports;

    HeapBlock<float*> audioBuffers;
    MidiBuffer midiEvents;
    Array<float> lastControlValues; // includes bypass/enabled
};

static int doRecall(const char* libraryPath)
{
    std::unique_ptr<AudioProcessor> filter = createPluginFilterOfType (AudioProcessor::wrapperType_LV2);

    // Stop here if createPluginFilterOfType failed
    if (filter == nullptr)
    {
        fprintf (stderr, "Failed to create plugin filter\n");
        return 1;
    }

   #ifdef JucePlugin_PreferredChannelConfigurations
    constexpr int configs[][2] = { JucePlugin_PreferredChannelConfigurations };
    filter->setPlayConfigDetails (configs[0][0], configs[0][1], 48000.0, 16);
   #else
    filter->enableAllBuses();
   #endif
    filter->refreshParameterList();

    const Array<AudioProcessorParameter*>& parameters = filter->getParameters();
    const int numInputs = filter->getTotalNumInputChannels();
    const int numOutputs = filter->getTotalNumOutputChannels();
    const int numControls = parameters.size();

    AudioProcessorParameter* const bypassParameter = filter->getBypassParameter();

    // Stop here if filter has Anagram incompatible IO
    if (numInputs < 1 || numOutputs < 1 || numInputs > 2 || numOutputs > 2)
    {
        fprintf (stderr, "Plugin filter has Anagram incompatible IO\n");
        return 1;
    }

    // Stop here if filter is missing bypass parameter
    if (bypassParameter == nullptr)
    {
        fprintf (stderr, "Plugin filter is missing bypass parameter, required for Anagram\n");
        return 1;
    }

    // Stop here if filter has no parameters
    if (numControls <= 1)
    {
        fprintf (stderr, "Plugin filter has no parameters, at least 1 is required for Anagram\n");
        return 1;
    }

    const String libraryPathString { CharPointer_UTF8 { libraryPath } };

    const File libraryPathAbsolute = File::isAbsolutePath (libraryPathString)
        ? File (libraryPathString)
        : File::getCurrentWorkingDirectory().getChildFile (libraryPathString);

    //=================================================================================================================
    // Create the manifest.ttl file contents

    std::cout << "Writing manifest.ttl...";
    std::cout.flush();

    {
        std::fstream ttl (libraryPathAbsolute.getSiblingFile ("manifest.ttl").getFullPathName().toRawUTF8(),
                          std::ios::out);

        // Header
        ttl << "@prefix lv2:  <" LV2_CORE_PREFIX "> .\n"
               "@prefix pset: <" LV2_PRESETS_PREFIX "> .\n"
               "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n"
               "\n";

        // Plugin
        ttl << "<" JucePlugin_LV2URI ">\n"
               "\ta lv2:Plugin ;\n"
               "\tlv2:binary <" << URL::addEscapeChars(libraryPathAbsolute.getFileName(), false) << "> ;\n"
               "\trdfs:seeAlso <dsp.ttl> .\n"
               "\n";
    }

    std::cout << "done!" << std::endl;

    //=================================================================================================================
    // Create the dsp.ttl file contents

    std::cout << "Writing dsp.ttl...";
    std::cout.flush();

    {
        std::fstream ttl (libraryPathAbsolute.getSiblingFile ("dsp.ttl").getFullPathName().toRawUTF8(),
                          std::ios::out);

        // Header
        ttl << "@prefix atom:  <" LV2_ATOM_PREFIX "> .\n"
               "@prefix bufs:  <http://lv2plug.in/ns/ext/buf-size#> .\n"
               "@prefix dg:    <http://www.darkglass.com/lv2/ns#> .\n"
               "@prefix doap:  <http://usefulinc.com/ns/doap#> .\n"
               "@prefix kx:    <http://kxstudio.sf.net/ns/lv2ext/props#> .\n"
               "@prefix foaf:  <http://xmlns.com/foaf/0.1/> .\n"
               "@prefix lv2:   <" LV2_CORE_PREFIX "> .\n"
               "@prefix opts:  <" LV2_OPTIONS_PREFIX "> .\n"
               "@prefix pprop: <http://lv2plug.in/ns/ext/port-props#> .\n"
               "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n"
               "@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .\n"
               "@prefix units: <" LV2_UNITS_PREFIX "> .\n"
               "@prefix urid:  <" LV2_URID_PREFIX "> .\n"
               "\n";

        // Plugin
        ttl << "<" JucePlugin_LV2URI ">\n"
               "\ta "
              #if JucePlugin_IsSynth
               "lv2:InstrumentPlugin"
              #elif defined(JucePlugin_LV2Category)
               JucePlugin_LV2Category
              #else
               "lv2:Plugin"
              #endif
               " , doap:Project ;\n"
               "\n"
               "\tlv2:requiredFeature bufs:boundedBlockLength , opts:options , urid:map ;\n"
               "\topts:requiredOption bufs:nominalBlockLength ;\n"
               "\n";

        int portIndex = 0;

        // Audio inputs
        if (numInputs == 1)
        {
            ttl << "\tlv2:port [\n"
                   "\t\ta lv2:InputPort , lv2:AudioPort ;\n"
                   "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
                   "\t\tlv2:symbol \"lv2_audio_in\" ;\n"
                   "\t\tlv2:name \"Audio Input\" ;\n"
                   "\t] ;\n\n";
        }
        else
        {
            for (int i = 0; i < numInputs; ++i)
            {
                if (i == 0)
                    ttl << "\tlv2:port [\n";

                ttl << "\t\ta lv2:InputPort , lv2:AudioPort ;\n"
                       "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
                       "\t\tlv2:symbol \"lv2_audio_in_" << std::to_string(i + 1) << "\" ;\n"
                       "\t\tlv2:name \"Audio Input " << std::to_string(i + 1) << "\" ;\n";

                if (i + 1 == numInputs)
                    ttl << "\t] ;\n\n";
                else
                    ttl << "\t] , [\n";
            }
        }

        // Audio outputs
        if (numOutputs == 1)
        {
            ttl << "\tlv2:port [\n"
                   "\t\ta lv2:OutputPort , lv2:AudioPort ;\n"
                   "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
                   "\t\tlv2:symbol \"lv2_audio_out\" ;\n"
                   "\t\tlv2:name \"Audio Output\" ;\n"
                   "\t] ;\n\n";
        }
        else
        {
            for (int i = 0; i < numOutputs; ++i)
            {
                if (i == 0)
                    ttl << "\tlv2:port [\n";

                ttl << "\t\ta lv2:OutputPort , lv2:AudioPort ;\n"
                       "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
                       "\t\tlv2:symbol \"lv2_audio_out_" << std::to_string(i + 1) << "\" ;\n"
                       "\t\tlv2:name \"Audio Output " << std::to_string(i + 1) << "\" ;\n";

                if (i + 1 == numOutputs)
                    ttl << "\t] ;\n\n";
                else
                    ttl << "\t] , [\n";
            }
        }

        // Bypass/Enabled parameter
        ttl << "\tlv2:port [\n"
               "\t\ta lv2:InputPort , lv2:ControlPort ;\n"
               "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
               "\t\tlv2:symbol \"lv2_enabled\" ;\n"
               "\t\tlv2:name \"Enabled\" ;\n"
               "\t\tlv2:default 1.0 ;\n"
               "\t\tlv2:minimum 0.0 ;\n"
               "\t\tlv2:maximum 1.0 ;\n"
               "\t\tlv2:designation lv2:enabled ;\n"
               "\t\tlv2:portProperty lv2:toggled , lv2:connectionOptional , pprop:notOnGUI ;\n"
               "\t] , [\n";

        // Reset parameter
        ttl << "\t\ta lv2:InputPort , lv2:ControlPort ;\n"
               "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
               "\t\tlv2:symbol \"lv2_reset\" ;\n"
               "\t\tlv2:name \"Reset\" ;\n"
               "\t\tlv2:default 0.0 ;\n"
               "\t\tlv2:minimum 0.0 ;\n"
               "\t\tlv2:maximum 1.0 ;\n"
               "\t\tlv2:designation kx:Reset ;\n"
               "\t\tlv2:portProperty lv2:toggled , lv2:connectionOptional , pprop:notOnGUI , pprop:trigger ;\n"
               "\t] , [\n";

       #if JucePlugin_LV2WantsFreeWheel
        // Free Wheeling parameter
        ttl << "\t\ta lv2:InputPort , lv2:ControlPort ;\n"
               "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
               "\t\tlv2:symbol \"lv2_freeWheeling\" ;\n"
               "\t\tlv2:name \"Free Wheeling\" ;\n"
               "\t\tlv2:default 0.0 ;\n"
               "\t\tlv2:minimum 0.0 ;\n"
               "\t\tlv2:maximum 1.0 ;\n"
               "\t\tlv2:designation lv2:freeWheeling ;\n"
               "\t\tlv2:portProperty lv2:toggled , lv2:connectionOptional , pprop:notOnGUI ;\n"
               "\t] , [\n";
       #endif

       #if JucePlugin_LV2WantsLatency
        // Latency parameter
        ttl << "\t\ta lv2:OutputPort , lv2:ControlPort ;\n"
               "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
               "\t\tlv2:symbol \"lv2_latency\" ;\n"
               "\t\tlv2:name \"Latency\" ;\n"
               "\t\tlv2:designation lv2:latency ;\n"
               "\t\tlv2:portProperty lv2:reportsLatency , lv2:integer , lv2:connectionOptional , pprop:notOnGUI ;\n"
               "\t\tunits:unit units:frame ;\n"
               "\t] , [\n";
       #endif

        // regular parameters
        for (int i = 0, offset = 0; i < numControls; ++i)
        {
            AudioProcessorParameter* const parameter = parameters.getUnchecked(i);

            if (parameter == bypassParameter)
            {
                ++offset;
                continue;
            }

            const String symbol = lv2_shared::sanitiseStringAsTtlName (
                URL::addEscapeChars (LegacyAudioParameter::getParamID (parameter, false), true))
                // mishandled in some JUCE versions, a '-' character is not allowed as symbol
                .replace("-","_");

            // TODO ask Jesse the real param size
            String name = parameter->getName(32);
            if (name.isEmpty())
                name = "Parameter " + String(i - offset + 1);

            ttl << "\t\ta lv2:InputPort , lv2:ControlPort ;\n"
                   "\t\tlv2:index " << std::to_string(portIndex++) << " ;\n"
                   "\t\tlv2:symbol \"" << symbol.toRawUTF8() << "\" ;\n"
                   "\t\tlv2:name \"" << name.replace("\"", "'").toRawUTF8() << "\" ;\n";

            float min, max;
            if (const auto rangedParameter = dynamic_cast<const RangedAudioParameter*>(parameter))
            {
                const NormalisableRange<float>& range = rangedParameter->getNormalisableRange();
                min = range.start;
                max = range.end;

                ttl << "\t\tlv2:default "
                    << std::to_string (rangedParameter->convertFrom0to1 (rangedParameter->getValue())) << " ;\n"
                       "\t\tlv2:minimum " << std::to_string (min) << " ;\n"
                       "\t\tlv2:maximum " << std::to_string (max) << " ;\n";

                if (rangedParameter->label == "dB")
                    ttl << "\t\tunits:unit units:db ;\n";
            }
            else
            {
                min = 0.f;
                max = 1.f;
                ttl << "\t\tlv2:default " << std::to_string(parameter->getValue()) << " ;\n"
                       "\t\tlv2:minimum 0.0 ;\n"
                       "\t\tlv2:maximum 1.0 ;\n";
            }

            if (parameter->isBoolean())
                ttl << "\t\tlv2:portProperty lv2:toggled ;\n";
            if (! parameter->isAutomatable())
                ttl << "\t\tlv2:portProperty pprop:expensive ;\n";

            if (const auto* anagramParameter = dynamic_cast<const anagram::AudioParameterWithScalePoints*> (parameter))
            {
                if (auto scalePoints = anagramParameter->getAllScalePoints(); ! scalePoints.isEmpty())
                {
                    ttl << "\t\tlv2:scalePoint [\n";

                    bool first = true;
                    for (const auto& scalePoint : scalePoints)
                    {
                        if (first)
                            first = false;
                        else
                            ttl << "\t\t] , [\n";

                        ttl << "\t\t\trdfs:label \"" << scalePoint.label.toRawUTF8() << "\" ;\n"
                               "\t\t\trdf:value " << std::to_string (scalePoint.value) << " ;\n";
                    }

                    ttl << "\t\t] ;\n";
                }
            }
            else if (int numSteps = parameter->getNumSteps();
                     parameter->isDiscrete() && ! parameter->isBoolean() && numSteps >= 2)
            {
                if (const auto strings = parameter->getAllValueStrings(); ! strings.isEmpty())
                {
                    ttl << "\t\tlv2:portProperty lv2:enumeration ;\n"
                           "\t\tlv2:scalePoint [\n";

                    for (const auto [counter, string] : enumerate (strings))
                    {
                        const auto value = jmap<double> (counter, 0, numSteps - 1, min, max);

                        if (counter != 0)
                            ttl << "\t\t] , [\n";

                        ttl << "\t\t\trdfs:label \"" << string.toRawUTF8() << "\" ;\n"
                               "\t\t\trdf:value " << std::to_string (value) << " ;\n";
                    }

                    ttl << "\t\t] ;\n";
                }
            }

            if (i + 1 == numControls)
                ttl << "\t] ;\n\n";
            else
                ttl << "\t] , [\n";
        }

        ttl << "\tdoap:name \"" << filter->getName().replace("\"", "'").toRawUTF8() << "\" ;\n"
               "\tdoap:description \"" JucePlugin_Desc << "\" ;\n"
               "\tdoap:maintainer [\n"
               "\t\ta foaf:Person ;\n"
               "\t\tfoaf:name \"" JucePlugin_Manufacturer "\" ;\n"
               "\t\tfoaf:homepage <" JucePlugin_ManufacturerWebsite "> ;\n"
               "\t\tfoaf:mbox <" JucePlugin_ManufacturerEmail "> ;\n"
               "\t] ;\n"
               "\tdoap:release [\n"
               "\t\ta doap:Version ;\n"
               "\t\tdoap:revision \"" JucePlugin_VersionString "\" ;\n"
               "\t] ;\n\n";

        juce::StringArray altNames = filter->getAlternateDisplayNames();
        for (const juce::String& altName : altNames)
        {
            if (altName.length() >= 2 && altName.length() <= 3)
            {
                static constexpr auto isValidChar = [](juce_wchar c)
                {
                    return c == 0 || (CharacterFunctions::isUpperCase (c) && c != '"');
                };

                if (isValidChar(altName[0]) && isValidChar(altName[1]) && isValidChar(altName[2]))
                {
                    ttl << "\tdg:abbreviation \"" << altName.toRawUTF8() << "\" ;\n\n" ;
                    break;
                }
            }
        }

        ttl << "\tlv2:minorVersion 0 ;\n"
               "\tlv2:microVersion 0 .\n";
    }

    std::cout << "done!" << std::endl;

    return 0;
}

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor (uint32_t index)
{
    static const LV2_Descriptor descriptor
    {
        JucePlugin_LV2URI,
        [] (const LV2_Descriptor*,
            double sampleRate,
            const char*,
            const LV2_Feature* const* features) -> LV2_Handle
        {
            // query optional and required LV2 features
            LV2_Log_Logger logger{};
            LV2_Options_Option* options;
            LV2_URID_Map* uridMap;

            const char* missing = lv2_features_query (features,
                LV2_LOG__log,         &logger.log, false,
                LV2_OPTIONS__options, &options,    true,
                LV2_URID__map,        &uridMap,    true,
                nullptr);

            lv2_log_logger_set_map (&logger, uridMap);
            if (missing != nullptr)
            {
                lv2_log_error (&logger, "Missing feature <%s>\n", missing);
                return nullptr;
            }

            // query buffer size from LV2 options
            int32_t bufferSize = 0;
            for (int i = 0; options[i].key != 0 && options[i].type != 0; ++i)
            {
                if (options[i].key == uridMap->map (uridMap->handle, LV2_BUF_SIZE__nominalBlockLength) &&
                    options[i].type == uridMap->map (uridMap->handle, LV2_ATOM__Int))
                {
                    bufferSize = *static_cast<const int32_t*> (options[i].value);
                    break;
                }
            }

            if (bufferSize == 0)
            {
                lv2_log_error (&logger, "Missing option <%s>\n", LV2_BUF_SIZE__nominalBlockLength);
                return nullptr;
            }

            std::unique_ptr<JuceLv2Wrapper> wrapper = std::make_unique<JuceLv2Wrapper> (sampleRate,
                                                                                        bufferSize,
                                                                                        logger,
                                                                                        uridMap);

            if (wrapper->ok)
                return wrapper.release();

            return nullptr;
        },
        [] (LV2_Handle instance, uint32_t port, void* data)
        {
            static_cast<JuceLv2Wrapper*> (instance)->connect(static_cast<int> (port), data);
        },
        [] (LV2_Handle instance)
        {
            static_cast<JuceLv2Wrapper*> (instance)->activate();
        },
        [] (LV2_Handle instance, uint32_t sampleCount)
        {
            static_cast<JuceLv2Wrapper*> (instance)->run(static_cast<int> (sampleCount));
        },
        [] (LV2_Handle instance)
        {
            static_cast<JuceLv2Wrapper*> (instance)->deactivate();
        },
        [] (LV2_Handle instance)
        {
            JUCE_AUTORELEASEPOOL
            {
                delete static_cast<JuceLv2Wrapper*> (instance);
            }
        },
        [] (const char* uri) -> const void*
        {
            static const struct {
                int (*doRecall) (const char*);
            } recall {
                [] (const char* libraryPath) -> int
                {
                    return doRecall(libraryPath);
                }
            };

            if (std::strcmp(uri, "https://lv2-extensions.juce.com/turtle_recall") == 0)
                return &recall;

            return nullptr;
        }
    };

    return index == 0 ? &descriptor : nullptr;
}

}
