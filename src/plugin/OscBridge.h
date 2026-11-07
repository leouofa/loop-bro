#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include "../dsp/CaptureEngine.h"

namespace loopbro
{

class LoopBroAudioProcessor;

class OscBridge : public juce::OSCReceiver,
                  public juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                  public juce::AudioProcessorValueTreeState::Listener,
                  private juce::Timer
{
public:
    static constexpr int DefaultReceivePort = 9001; // M4L -> VST
    static constexpr int DefaultSendPort    = 9002; // VST -> M4L
    static constexpr int LegacyBridgePort   = 8998; // Ableton clip-slot auto-insert bridge

    OscBridge(LoopBroAudioProcessor& processor, CaptureEngine& engine, juce::AudioProcessorValueTreeState& apvts);
    ~OscBridge() override;

    void start(int receivePort = DefaultReceivePort, int sendPort = DefaultSendPort);
    void stop();

    // Broadcast file staging manually if needed
    void broadcastWaveformFile(const juce::File& file);

    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // OSCReceiver::Listener
    void oscMessageReceived(const juce::OSCMessage& message) override;

private:
    void timerCallback() override;

    void handleArmCommand(const juce::OSCMessage& message);
    void handlePlayCommand(const juce::OSCMessage& message);
    void handleClearCommand();
    void handleParamCommand(const juce::String& paramId, const juce::OSCMessage& message);
    void sendFullSync();

    void sendState(State state);
    void sendPlayhead(float normalizedPos, int currentSamples, int totalSamples);
    void sendMeter(float levelDb);
    void sendParamValue(const juce::String& paramId, float value);

    static float getFloatOrInt(const juce::OSCArgument& arg);

    LoopBroAudioProcessor& processor;
    CaptureEngine& engine;
    juce::AudioProcessorValueTreeState& apvts;

    juce::OSCSender sender;
    juce::OSCSender legacyBridgeSender;

    int currentReceivePort = DefaultReceivePort;
    int currentSendPort    = DefaultSendPort;

    State lastState = State::Idle;
    int lastCaptureVersion = 0;
    int lastPlaybackPos = -1;

    std::atomic<bool> isHandlingIncomingParam { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscBridge)
};

} // namespace loopbro
