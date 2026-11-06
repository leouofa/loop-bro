#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace loopbro
{

namespace ParameterIDs
{
    inline const juce::ParameterID startThresholdDb { "startThresholdDb", 1 };
    inline const juce::ParameterID stopThresholdDb  { "stopThresholdDb", 1 };
    inline const juce::ParameterID preRollMs        { "preRollMs", 1 };
    inline const juce::ParameterID stopHoldMs       { "stopHoldMs", 1 };
    inline const juce::ParameterID loopGainDb       { "loopGainDb", 1 };
    inline const juce::ParameterID inputThru        { "inputThru", 1 };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::startThresholdDb,
        "Start Threshold",
        juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f),
        -30.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::stopThresholdDb,
        "Stop Threshold",
        juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f),
        -48.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    auto preRollRange = juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f);
    preRollRange.setSkewForCentre(200.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::preRollMs,
        "Pre-Roll",
        preRollRange,
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    auto stopHoldRange = juce::NormalisableRange<float>(50.0f, 10000.0f, 1.0f);
    stopHoldRange.setSkewForCentre(500.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::stopHoldMs,
        "Stop Hold",
        stopHoldRange,
        500.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::loopGainDb,
        "Loop Level",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::inputThru,
        "Input Thru",
        true));

    return { params.begin(), params.end() };
}

} // namespace loopbro
