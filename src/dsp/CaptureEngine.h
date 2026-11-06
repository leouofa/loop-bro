#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include "EnvelopeFollower.h"
#include "PreRollRingBuffer.h"

namespace loopbro
{

enum class State
{
    Idle = 0,
    Armed,
    Recording,
    Captured,
    Playing
};

struct CaptureSettings
{
    float startThresholdDb = -30.0f;
    float stopThresholdDb = -48.0f;
    float preRollMs = 50.0f;
    float stopHoldMs = 500.0f;
};

class CaptureEngine
{
public:
    CaptureEngine();
    ~CaptureEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels = 2, float maxTakeMinutes = 5.0f);
    void reset();

    // Audio thread processing
    void processBlock(const juce::AudioBuffer<float>& inputBuffer,
                      juce::AudioBuffer<float>& loopOutputBuffer,
                      const CaptureSettings& settings);

    // Lock-free commands from UI / main thread
    void cmdArm();
    void cmdDisarm();
    void cmdPlay();
    void cmdStopPlayback();
    void cmdClear();
    void cmdStopCaptureEarly();

    // Lock-free queries for UI
    State getState() const noexcept { return currentState.load(std::memory_order_relaxed); }
    int getCapturedLength() const noexcept { return capturedLengthSamples.load(std::memory_order_relaxed); }
    int getCaptureVersion() const noexcept { return captureVersion.load(std::memory_order_relaxed); }
    int getPlaybackPos() const noexcept { return playbackReadPos.load(std::memory_order_relaxed); }
    int getRecordingSamples() const noexcept { return recordingSamplesCount.load(std::memory_order_relaxed); }
    float getCurrentEnvelopeDb() const noexcept { return currentEnvelopeDb.load(std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return currentSampleRate; }

    // Safe to read when state is Captured or Playing (capturedLength samples)
    const juce::AudioBuffer<float>& getCaptureBuffer() const noexcept { return captureBuffer; }

    // Thread-safe copy of captured audio for WAV export or UI waveform cache
    bool copyCapturedAudio(juce::AudioBuffer<float>& destBuffer) const;

private:
    void handleCommands();
    void startRecording(float preRollMs);
    void finalizeRecording(int dropSampleIndex);
    void applyCosineFadeOut(int startSample, int endSample);

    double currentSampleRate = 44100.0;
    int channels = 2;
    int maxBufferSamples = 0;

    EnvelopeFollower envelopeFollower;
    PreRollRingBuffer preRollBuffer;
    juce::AudioBuffer<float> captureBuffer;

    // Real-time state
    std::atomic<State> currentState { State::Idle };
    std::atomic<int> capturedLengthSamples { 0 };
    std::atomic<int> captureVersion { 0 };
    std::atomic<int> playbackReadPos { 0 };
    std::atomic<int> recordingSamplesCount { 0 };
    std::atomic<float> currentEnvelopeDb { -100.0f };

    int captureWritePos = 0;
    int belowStopCounterSamples = 0;

    // Command flags (written by UI thread, consumed by audio thread)
    std::atomic<bool> cmdArmFlag { false };
    std::atomic<bool> cmdDisarmFlag { false };
    std::atomic<bool> cmdPlayFlag { false };
    std::atomic<bool> cmdStopPlaybackFlag { false };
    std::atomic<bool> cmdClearFlag { false };
    std::atomic<bool> cmdStopCaptureEarlyFlag { false };
};

} // namespace loopbro
