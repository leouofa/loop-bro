#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <cassert>
#include <cmath>

#include "../src/dsp/EnvelopeFollower.h"
#include "../src/dsp/PreRollRingBuffer.h"
#include "../src/dsp/CaptureEngine.h"
#include "../src/plugin/WavExporter.h"
#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/OscBridge.h"

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion FAILED: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::abs((a) - (b)) > (eps)) { \
            std::cerr << "Assertion FAILED: |" #a " - " #b "| <= " #eps \
                      << " (got " << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool testEnvelopeFollower()
{
    std::cout << "[RUNNING] testEnvelopeFollower..." << std::endl;
    loopbro::EnvelopeFollower env;
    env.prepare(44100.0);
    env.setAttackRelease(0.0f, 20.0f);

    // Initial silence
    ASSERT_NEAR(env.getCurrentLevel(), 0.0f, 1e-5f);
    ASSERT_TRUE(env.getCurrentDb() <= -99.0f);

    // Feed a sharp pulse
    for (int i = 0; i < 20; ++i)
    {
        env.processSample(0.8f);
    }
    ASSERT_TRUE(env.getCurrentLevel() > 0.75f);
    ASSERT_TRUE(env.getCurrentDb() > -3.0f);

    // Release over 1000 samples of silence
    for (int i = 0; i < 2000; ++i)
    {
        env.processSample(0.0f);
    }
    ASSERT_TRUE(env.getCurrentLevel() < 0.15f);

    std::cout << "[PASSED] testEnvelopeFollower" << std::endl;
    return true;
}

bool testPreRollRingBuffer()
{
    std::cout << "[RUNNING] testPreRollRingBuffer..." << std::endl;
    const double sampleRate = 44100.0;
    loopbro::PreRollRingBuffer ring;
    ring.prepare(sampleRate, 2, 1.0f); // 1 second buffer

    // Write ramp signal
    const int blockSize = 512;
    juce::AudioBuffer<float> block(2, blockSize);
    for (int i = 0; i < blockSize; ++i)
    {
        const float val = static_cast<float>(i + 1) / 1000.0f;
        block.setSample(0, i, val);
        block.setSample(1, i, val);
    }

    ring.writeBlock(block);
    ASSERT_TRUE(ring.getAvailableSamples() == blockSize);

    juce::AudioBuffer<float> dest(2, 256);
    int copied = ring.copyRecentTo(dest, 0, 256, 0.0f); // no fade for raw test
    ASSERT_TRUE(copied == 256);

    // The most recent 256 samples in block were indices [256 .. 511]
    for (int i = 0; i < 256; ++i)
    {
        const float expected = static_cast<float>(256 + i + 1) / 1000.0f;
        ASSERT_NEAR(dest.getSample(0, i), expected, 1e-4f);
    }

    std::cout << "[PASSED] testPreRollRingBuffer" << std::endl;
    return true;
}

bool testCaptureEngineWorkflow()
{
    std::cout << "[RUNNING] testCaptureEngineWorkflow..." << std::endl;
    const double sampleRate = 44100.0;
    const int blockSize = 128;
    loopbro::CaptureEngine engine;
    engine.prepare(sampleRate, blockSize, 2, 1.0f);

    ASSERT_TRUE(engine.getState() == loopbro::State::Idle);

    // Setup settings: Start at -20 dB, Stop at -40 dB, Pre-Roll 50 ms, Stop Hold 100 ms
    loopbro::CaptureSettings settings;
    settings.startThresholdDb = -20.0f;
    settings.stopThresholdDb = -40.0f;
    settings.preRollMs = 50.0f;
    settings.stopHoldMs = 100.0f;

    juce::AudioBuffer<float> inBlock(2, blockSize);
    juce::AudioBuffer<float> loopBlock(2, blockSize);

    // 1. Arm plugin
    engine.cmdArm();

    // 2. Feed silence (-60 dB) -> should remain armed
    inBlock.clear();
    for (int b = 0; b < 20; ++b)
    {
        engine.processBlock(inBlock, loopBlock, settings);
        ASSERT_TRUE(engine.getState() == loopbro::State::Armed);
    }

    // 3. Strum guitar! (amplitude 0.5 = -6 dB, above -20 dB start threshold)
    for (int i = 0; i < blockSize; ++i)
    {
        inBlock.setSample(0, i, 0.5f);
        inBlock.setSample(1, i, 0.5f);
    }
    engine.processBlock(inBlock, loopBlock, settings);

    // Must have triggered recording
    ASSERT_TRUE(engine.getState() == loopbro::State::Recording);
    ASSERT_TRUE(engine.getRecordingSamples() > 0);

    // 4. Continue playing for ~300 ms (about 100 blocks at 128 samples)
    for (int b = 0; b < 100; ++b)
    {
        // Add a decaying guitar-like oscillation
        for (int i = 0; i < blockSize; ++i)
        {
            const float s = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * (b * blockSize + i) / 44100.0f);
            inBlock.setSample(0, i, s);
            inBlock.setSample(1, i, s);
        }
        engine.processBlock(inBlock, loopBlock, settings);
        ASSERT_TRUE(engine.getState() == loopbro::State::Recording);
    }

    // 5. Strum dies down (silence). Must hold for 100 ms (approx 35 blocks) then auto-stop
    inBlock.clear();
    int blocksUntilStop = 0;
    while (engine.getState() == loopbro::State::Recording && blocksUntilStop < 100)
    {
        engine.processBlock(inBlock, loopBlock, settings);
        blocksUntilStop++;
    }

    // Must have stopped and transitioned to Captured
    ASSERT_TRUE(engine.getState() == loopbro::State::Captured);
    ASSERT_TRUE(engine.getCapturedLength() > 0);
    ASSERT_TRUE(engine.getCaptureVersion() == 1);

    // 6. Test Loop Playback
    engine.cmdPlay();
    engine.processBlock(inBlock, loopBlock, settings);
    ASSERT_TRUE(engine.getState() == loopbro::State::Playing);

    // Loop output must contain non-zero audio once playback passes the pre-roll window
    float totalLoopEnergy = 0.0f;
    for (int b = 0; b < 30; ++b)
    {
        engine.processBlock(inBlock, loopBlock, settings);
        for (int i = 0; i < blockSize; ++i)
        {
            totalLoopEnergy += std::abs(loopBlock.getSample(0, i));
        }
    }
    ASSERT_TRUE(totalLoopEnergy > 0.0f);

    // 7. Stop Playback
    engine.cmdStopPlayback();
    engine.processBlock(inBlock, loopBlock, settings);
    ASSERT_TRUE(engine.getState() == loopbro::State::Captured);

    std::cout << "[PASSED] testCaptureEngineWorkflow" << std::endl;
    return true;
}

bool testWavExport()
{
    std::cout << "[RUNNING] testWavExport..." << std::endl;
    const double sampleRate = 48000.0;
    const int numSamples = 4800; // 0.1s
    juce::AudioBuffer<float> buffer(2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float val = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / sampleRate);
        buffer.setSample(0, i, val);
        buffer.setSample(1, i, val);
    }

    auto tempWav = loopbro::WavExporter::stageTempWavFile(buffer, sampleRate, 24);
    ASSERT_TRUE(tempWav.existsAsFile());
    ASSERT_TRUE(tempWav.getSize() > 1000);

    // Read file back and check properties
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(tempWav));
    ASSERT_TRUE(reader != nullptr);
    ASSERT_TRUE(reader->numChannels == 2);
    ASSERT_NEAR(reader->sampleRate, sampleRate, 1.0);
    ASSERT_TRUE(reader->bitsPerSample == 24);
    ASSERT_TRUE(reader->lengthInSamples == numSamples);

    tempWav.deleteFile();
    std::cout << "[PASSED] testWavExport" << std::endl;
    return true;
}

bool testOscBridgeWorkflow()
{
    std::cout << "[RUNNING] testOscBridgeWorkflow..." << std::endl;
    loopbro::LoopBroAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto* osc = proc.getOscBridge();
    ASSERT_TRUE(osc != nullptr);

    juce::AudioBuffer<float> inBuf(2, 512);
    juce::AudioBuffer<float> outBuf(2, 512);
    loopbro::CaptureSettings settings;

    // 1. Test Arm via OSC Message
    juce::OSCMessage armMsg(juce::OSCAddressPattern("/loopbro/arm"), 1);
    osc->oscMessageReceived(armMsg);
    proc.getCaptureEngine().processBlock(inBuf, outBuf, settings);
    ASSERT_TRUE(proc.getCaptureEngine().getState() == loopbro::State::Armed);

    // 2. Test Disarm via OSC Message
    juce::OSCMessage disarmMsg(juce::OSCAddressPattern("/loopbro/arm"), 0);
    osc->oscMessageReceived(disarmMsg);
    proc.getCaptureEngine().processBlock(inBuf, outBuf, settings);
    ASSERT_TRUE(proc.getCaptureEngine().getState() == loopbro::State::Idle);

    // 3. Test Parameter updates via OSC
    juce::OSCMessage paramMsg1(juce::OSCAddressPattern("/loopbro/param/startThresholdDb"), -24.5f);
    osc->oscMessageReceived(paramMsg1);
    auto* p1 = proc.getAPVTS().getRawParameterValue("startThresholdDb");
    ASSERT_TRUE(p1 != nullptr);
    ASSERT_NEAR(p1->load(), -24.5f, 0.2f);

    juce::OSCMessage paramMsg2(juce::OSCAddressPattern("/loopbro/param/preRollMs"), 150.0f);
    osc->oscMessageReceived(paramMsg2);
    auto* p2 = proc.getAPVTS().getRawParameterValue("preRollMs");
    ASSERT_TRUE(p2 != nullptr);
    ASSERT_NEAR(p2->load(), 150.0f, 1.0f);

    juce::OSCMessage paramMsg3(juce::OSCAddressPattern("/loopbro/param/inputThru"), 0.0f);
    osc->oscMessageReceived(paramMsg3);
    auto* p3 = proc.getAPVTS().getRawParameterValue("inputThru");
    ASSERT_TRUE(p3 != nullptr);
    ASSERT_TRUE(p3->load() < 0.5f);

    // 4. Test Early Capture Stop via OSC Disarm during Recording
    osc->oscMessageReceived(armMsg);
    proc.getCaptureEngine().processBlock(inBuf, outBuf, settings);
    ASSERT_TRUE(proc.getCaptureEngine().getState() == loopbro::State::Armed);

    // Trigger recording by feeding loud audio
    inBuf.clear();
    for (int i = 0; i < 512; ++i) inBuf.setSample(0, i, 0.8f);
    settings.startThresholdDb = -20.0f;
    proc.getCaptureEngine().processBlock(inBuf, outBuf, settings);
    ASSERT_TRUE(proc.getCaptureEngine().getState() == loopbro::State::Recording);

    // Disarm message while recording should finalize capture early into Captured state
    osc->oscMessageReceived(disarmMsg);
    inBuf.clear();
    proc.getCaptureEngine().processBlock(inBuf, outBuf, settings);
    ASSERT_TRUE(proc.getCaptureEngine().getState() == loopbro::State::Captured);

    std::cout << "[PASSED] testOscBridgeWorkflow" << std::endl;
    return true;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "========================================" << std::endl;
    std::cout << "  LOOP BRO DSP & EXPORT UNIT TEST SUITE  " << std::endl;
    std::cout << "========================================" << std::endl;

    if (!testEnvelopeFollower()) return 1;
    if (!testPreRollRingBuffer()) return 1;
    if (!testCaptureEngineWorkflow()) return 1;
    if (!testWavExport()) return 1;
    if (!testOscBridgeWorkflow()) return 1;

    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
