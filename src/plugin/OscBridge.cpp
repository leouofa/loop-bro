#include "OscBridge.h"
#include "PluginProcessor.h"
#include "PluginParameters.h"
#include "WavExporter.h"

namespace loopbro
{

OscBridge::OscBridge(LoopBroAudioProcessor& p, CaptureEngine& eng, juce::AudioProcessorValueTreeState& vts)
    : processor(p), engine(eng), apvts(vts)
{
    // Listen to parameter changes from GUI or DAW automation
    apvts.addParameterListener(ParameterIDs::startThresholdDb.getParamID(), this);
    apvts.addParameterListener(ParameterIDs::stopThresholdDb.getParamID(), this);
    apvts.addParameterListener(ParameterIDs::preRollMs.getParamID(), this);
    apvts.addParameterListener(ParameterIDs::stopHoldMs.getParamID(), this);
    apvts.addParameterListener(ParameterIDs::loopGainDb.getParamID(), this);
    apvts.addParameterListener(ParameterIDs::inputThru.getParamID(), this);
}

OscBridge::~OscBridge()
{
    stop();

    apvts.removeParameterListener(ParameterIDs::startThresholdDb.getParamID(), this);
    apvts.removeParameterListener(ParameterIDs::stopThresholdDb.getParamID(), this);
    apvts.removeParameterListener(ParameterIDs::preRollMs.getParamID(), this);
    apvts.removeParameterListener(ParameterIDs::stopHoldMs.getParamID(), this);
    apvts.removeParameterListener(ParameterIDs::loopGainDb.getParamID(), this);
    apvts.removeParameterListener(ParameterIDs::inputThru.getParamID(), this);
}

void OscBridge::start(int receivePort, int sendPort)
{
    stop();

    currentReceivePort = receivePort;
    currentSendPort = sendPort;

    if (connect(currentReceivePort))
    {
        addListener(this);
    }

    sender.connect("127.0.0.1", currentSendPort);
    legacyBridgeSender.connect("127.0.0.1", LegacyBridgePort);

    startTimerHz(30); // 30 Hz updates for playhead, state, and headless capture staging
}

void OscBridge::stop()
{
    stopTimer();
    removeListener(this);
    disconnect();
    sender.disconnect();
    legacyBridgeSender.disconnect();
}

float OscBridge::getFloatOrInt(const juce::OSCArgument& arg)
{
    if (arg.isFloat32()) return arg.getFloat32();
    if (arg.isInt32())   return static_cast<float>(arg.getInt32());
    return 0.0f;
}

void OscBridge::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (isHandlingIncomingParam.load())
        return;

    sendParamValue(parameterID, newValue);
}

void OscBridge::broadcastWaveformFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    const auto path = file.getFullPathName();
    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/waveform/file"), path));

    // Also notify legacy bridge (port 8998) for auto clip insertion if listening
    legacyBridgeSender.send(juce::OSCMessage(juce::OSCAddressPattern("/insert_loop"), path));
}

void OscBridge::sendState(State state)
{
    juce::String stateStr;
    switch (state)
    {
        case State::Idle:      stateStr = "IDLE"; break;
        case State::Armed:     stateStr = "ARMED"; break;
        case State::Recording: stateStr = "RECORDING"; break;
        case State::Captured:  stateStr = "CAPTURED"; break;
        case State::Playing:   stateStr = "PLAYING"; break;
    }

    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/state"), static_cast<int32_t>(state), stateStr));
}

void OscBridge::sendPlayhead(float normalizedPos, int currentSamples, int totalSamples)
{
    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/playhead"),
                                 normalizedPos,
                                 static_cast<int32_t>(currentSamples),
                                 static_cast<int32_t>(totalSamples)));
}

void OscBridge::sendMeter(float levelDb)
{
    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/meter"), levelDb));
}

void OscBridge::sendParamValue(const juce::String& paramId, float value)
{
    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/param/" + paramId), value));
}

void OscBridge::sendFullSync()
{
    sendState(engine.getState());
    sendMeter(engine.getCurrentEnvelopeDb());

    auto sendSingleParam = [this](const juce::ParameterID& id)
    {
        if (auto* param = apvts.getParameter(id.getParamID()))
        {
            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                const float plainVal = rangedParam->getNormalisableRange().convertFrom0to1(param->getValue());
                sendParamValue(id.getParamID(), plainVal);
            }
        }
    };

    sendSingleParam(ParameterIDs::startThresholdDb);
    sendSingleParam(ParameterIDs::stopThresholdDb);
    sendSingleParam(ParameterIDs::preRollMs);
    sendSingleParam(ParameterIDs::stopHoldMs);
    sendSingleParam(ParameterIDs::loopGainDb);
    sendSingleParam(ParameterIDs::inputThru);

    auto staged = processor.getStagedWavFile();
    if (staged.existsAsFile())
    {
        broadcastWaveformFile(staged);
    }

    if (engine.getState() == State::Playing && engine.getCapturedLength() > 0)
    {
        const float norm = static_cast<float>(engine.getPlaybackPos()) / static_cast<float>(engine.getCapturedLength());
        sendPlayhead(norm, engine.getPlaybackPos(), engine.getCapturedLength());
    }
}

void OscBridge::handleArmCommand(const juce::OSCMessage& message)
{
    if (message.size() > 0)
    {
        const float val = getFloatOrInt(message[0]);
        if (val > 0.5f)
            engine.cmdArm();
        else
            engine.cmdDisarm();
    }
    else
    {
        if (engine.getState() == State::Armed)
            engine.cmdDisarm();
        else
            engine.cmdArm();
    }
}

void OscBridge::handlePlayCommand(const juce::OSCMessage& message)
{
    if (message.size() > 0)
    {
        const float val = getFloatOrInt(message[0]);
        if (val > 0.5f)
            engine.cmdPlay();
        else
            engine.cmdStopPlayback();
    }
    else
    {
        if (engine.getState() == State::Playing)
            engine.cmdStopPlayback();
        else
            engine.cmdPlay();
    }
}

void OscBridge::handleClearCommand()
{
    engine.cmdClear();
    processor.setStagedWavFile({});
    sender.send(juce::OSCMessage(juce::OSCAddressPattern("/loopbro/waveform/cleared"), 1));
}

void OscBridge::handleParamCommand(const juce::String& paramId, const juce::OSCMessage& message)
{
    if (message.size() == 0)
        return;

    const float targetValue = getFloatOrInt(message[0]);

    if (auto* param = apvts.getParameter(paramId))
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            const float normVal = rangedParam->getNormalisableRange().convertTo0to1(targetValue);
            isHandlingIncomingParam.store(true);
            param->setValueNotifyingHost(normVal);
            isHandlingIncomingParam.store(false);
        }
    }
}

void OscBridge::oscMessageReceived(const juce::OSCMessage& message)
{
    const juce::String pattern = message.getAddressPattern().toString();

    if (pattern == "/loopbro/arm")
    {
        handleArmCommand(message);
    }
    else if (pattern == "/loopbro/play")
    {
        handlePlayCommand(message);
    }
    else if (pattern == "/loopbro/clear")
    {
        handleClearCommand();
    }
    else if (pattern.startsWith("/loopbro/param/"))
    {
        const juce::String paramId = pattern.substring(15);
        handleParamCommand(paramId, message);
    }
    else if (pattern == "/loopbro/query" || pattern == "/loopbro/ping")
    {
        sendFullSync();
    }
}

void OscBridge::timerCallback()
{
    // 1. Headless take capture detection & staging
    const int ver = engine.getCaptureVersion();
    if (ver != lastCaptureVersion)
    {
        lastCaptureVersion = ver;
        juce::AudioBuffer<float> temp;
        if (engine.copyCapturedAudio(temp) && temp.getNumSamples() > 0)
        {
            const double sr = engine.getSampleRate();
            auto stagedFile = WavExporter::stageTempWavFile(temp, sr);
            processor.setStagedWavFile(stagedFile);
            broadcastWaveformFile(stagedFile);
        }
    }

    // 2. State change broadcasting
    const State state = engine.getState();
    if (state != lastState)
    {
        lastState = state;
        sendState(state);
    }

    // 3. Playhead broadcasting
    if (state == State::Playing)
    {
        const int len = engine.getCapturedLength();
        const int pos = engine.getPlaybackPos();
        if (len > 0)
        {
            const float norm = static_cast<float>(pos) / static_cast<float>(len);
            sendPlayhead(norm, pos, len);
        }
    }
    else if (state == State::Recording)
    {
        const int recSamples = engine.getRecordingSamples();
        sendPlayhead(-1.0f, recSamples, 0);
    }

    // 4. Input Meter broadcasting
    sendMeter(engine.getCurrentEnvelopeDb());
}

} // namespace loopbro
