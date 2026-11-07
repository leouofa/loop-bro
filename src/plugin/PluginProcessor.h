#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/CaptureEngine.h"
#include "PluginParameters.h"
#include "WavExporter.h"

namespace loopbro
{

class LoopBroAudioProcessor : public juce::AudioProcessor
{
public:
    LoopBroAudioProcessor();
    ~LoopBroAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    CaptureEngine& getCaptureEngine() noexcept { return captureEngine; }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // OSC Bridge for M4L & external control
    class OscBridge* getOscBridge() noexcept;

    // Path to the currently staged WAV file for Drag-to-DAW
    juce::File getStagedWavFile() const;
    void setStagedWavFile(const juce::File& file);

private:
    juce::AudioProcessorValueTreeState apvts;
    CaptureEngine captureEngine;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> loopGainSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputThruGainSmoother;

    juce::AudioBuffer<float> loopBuffer;

    std::atomic<float>* startThresholdParam = nullptr;
    std::atomic<float>* stopThresholdParam = nullptr;
    std::atomic<float>* preRollParam = nullptr;
    std::atomic<float>* stopHoldParam = nullptr;
    std::atomic<float>* loopGainParam = nullptr;
    std::atomic<float>* inputThruParam = nullptr;

    juce::CriticalSection stagedWavLock;
    juce::File currentStagedWav;
    int lastStagedVersion = -1;

    std::unique_ptr<class OscBridge> oscBridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopBroAudioProcessor)
};

} // namespace loopbro
