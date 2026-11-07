#include "PluginProcessor.h"
#include "OscBridge.h"
#include "../ui/PluginEditor.h"

namespace loopbro
{

LoopBroAudioProcessor::LoopBroAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    startThresholdParam = apvts.getRawParameterValue(ParameterIDs::startThresholdDb.getParamID());
    stopThresholdParam  = apvts.getRawParameterValue(ParameterIDs::stopThresholdDb.getParamID());
    preRollParam        = apvts.getRawParameterValue(ParameterIDs::preRollMs.getParamID());
    stopHoldParam       = apvts.getRawParameterValue(ParameterIDs::stopHoldMs.getParamID());
    loopGainParam       = apvts.getRawParameterValue(ParameterIDs::loopGainDb.getParamID());
    inputThruParam      = apvts.getRawParameterValue(ParameterIDs::inputThru.getParamID());

    oscBridge = std::make_unique<OscBridge>(*this, captureEngine, apvts);
    oscBridge->start();
}

LoopBroAudioProcessor::~LoopBroAudioProcessor()
{
    if (oscBridge != nullptr)
        oscBridge->stop();
    oscBridge.reset();
}

OscBridge* LoopBroAudioProcessor::getOscBridge() noexcept
{
    return oscBridge.get();
}

const juce::String LoopBroAudioProcessor::getName() const
{
    return "Loop Bro";
}

bool LoopBroAudioProcessor::acceptsMidi() const
{
    return false;
}

bool LoopBroAudioProcessor::producesMidi() const
{
    return false;
}

bool LoopBroAudioProcessor::isMidiEffect() const
{
    return false;
}

double LoopBroAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LoopBroAudioProcessor::getNumPrograms()
{
    return 1;
}

int LoopBroAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LoopBroAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String LoopBroAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void LoopBroAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

bool LoopBroAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    // Support Mono -> Mono, Mono -> Stereo (guitar DI), Stereo -> Stereo
    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::mono())
        return true;
    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo())
        return true;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

void LoopBroAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const int maxChannels = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int channels = std::max(2, maxChannels);

    captureEngine.prepare(sampleRate, samplesPerBlock, channels, 5.0f);

    loopBuffer.setSize(channels, samplesPerBlock, false, true, true);

    loopGainSmoother.reset(sampleRate, 0.02); // 20ms gain ramps
    inputThruGainSmoother.reset(sampleRate, 0.02);

    const float initialLoopGain = juce::Decibels::decibelsToGain(loopGainParam != nullptr ? loopGainParam->load() : 0.0f);
    const float initialThruGain = (inputThruParam != nullptr && inputThruParam->load() > 0.5f) ? 1.0f : 0.0f;

    loopGainSmoother.setCurrentAndTargetValue(initialLoopGain);
    inputThruGainSmoother.setCurrentAndTargetValue(initialThruGain);
}

void LoopBroAudioProcessor::releaseResources()
{
}

void LoopBroAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    // Clear unused output channels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // If mono input to stereo output, duplicate channel 0 to channel 1 before processing
    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2)
    {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    // Read parameter values
    CaptureSettings settings;
    if (startThresholdParam != nullptr) settings.startThresholdDb = startThresholdParam->load();
    if (stopThresholdParam != nullptr)  settings.stopThresholdDb  = stopThresholdParam->load();
    if (preRollParam != nullptr)        settings.preRollMs        = preRollParam->load();
    if (stopHoldParam != nullptr)       settings.stopHoldMs       = stopHoldParam->load();

    const float targetLoopGain = juce::Decibels::decibelsToGain(loopGainParam != nullptr ? loopGainParam->load() : 0.0f);
    const float targetThruGain = (inputThruParam != nullptr && inputThruParam->load() > 0.5f) ? 1.0f : 0.0f;

    loopGainSmoother.setTargetValue(targetLoopGain);
    inputThruGainSmoother.setTargetValue(targetThruGain);

    // Make sure loopBuffer matches size
    if (loopBuffer.getNumSamples() < numSamples)
        loopBuffer.setSize(std::max(2, totalNumOutputChannels), numSamples, false, false, true);

    // Run DSP Capture Engine
    captureEngine.processBlock(buffer, loopBuffer, settings);

    // Process output mixing
    const int numChannelsToProcess = std::min(totalNumOutputChannels, buffer.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        const float thruGain = inputThruGainSmoother.getNextValue();
        const float loopGain = loopGainSmoother.getNextValue();

        for (int ch = 0; ch < numChannelsToProcess; ++ch)
        {
            const float inSample = buffer.getSample(ch, i) * thruGain;
            const float loopSample = loopBuffer.getSample(ch, i) * loopGain;
            buffer.setSample(ch, i, inSample + loopSample);
        }
    }
}

juce::File LoopBroAudioProcessor::getStagedWavFile() const
{
    const juce::ScopedLock sl(stagedWavLock);
    return currentStagedWav;
}

void LoopBroAudioProcessor::setStagedWavFile(const juce::File& file)
{
    const juce::ScopedLock sl(stagedWavLock);
    currentStagedWav = file;
}

bool LoopBroAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* LoopBroAudioProcessor::createEditor()
{
    return new LoopBroAudioProcessorEditor(*this);
}

void LoopBroAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LoopBroAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace loopbro

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new loopbro::LoopBroAudioProcessor();
}
