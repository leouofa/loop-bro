#include "CaptureEngine.h"

namespace loopbro
{

CaptureEngine::CaptureEngine()
{
}

void CaptureEngine::prepare(double sampleRate, int maxBlockSize, int numChannels, float maxTakeMinutes)
{
    juce::ignoreUnused(maxBlockSize);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    channels = std::max(1, numChannels);

    envelopeFollower.prepare(currentSampleRate);
    preRollBuffer.prepare(currentSampleRate, channels, 2.5f); // up to 2.5s pre-roll buffer

    const int totalTakeSec = static_cast<int>(std::ceil(maxTakeMinutes * 60.0f));
    maxBufferSamples = static_cast<int>(totalTakeSec * currentSampleRate);
    captureBuffer.setSize(channels, maxBufferSamples + static_cast<int>(currentSampleRate * 2.0), false, true, true);

    reset();
}

void CaptureEngine::reset()
{
    envelopeFollower.reset();
    preRollBuffer.reset();
    captureBuffer.clear();

    currentState.store(State::Idle, std::memory_order_relaxed);
    capturedLengthSamples.store(0, std::memory_order_relaxed);
    playbackReadPos.store(0, std::memory_order_relaxed);
    recordingSamplesCount.store(0, std::memory_order_relaxed);
    currentEnvelopeDb.store(-100.0f, std::memory_order_relaxed);

    captureWritePos = 0;
    belowStopCounterSamples = 0;

    cmdArmFlag.store(false, std::memory_order_relaxed);
    cmdDisarmFlag.store(false, std::memory_order_relaxed);
    cmdPlayFlag.store(false, std::memory_order_relaxed);
    cmdStopPlaybackFlag.store(false, std::memory_order_relaxed);
    cmdClearFlag.store(false, std::memory_order_relaxed);
    cmdStopCaptureEarlyFlag.store(false, std::memory_order_relaxed);
}

void CaptureEngine::cmdArm()
{
    cmdArmFlag.store(true, std::memory_order_release);
}

void CaptureEngine::cmdDisarm()
{
    cmdDisarmFlag.store(true, std::memory_order_release);
}

void CaptureEngine::cmdPlay()
{
    cmdPlayFlag.store(true, std::memory_order_release);
}

void CaptureEngine::cmdStopPlayback()
{
    cmdStopPlaybackFlag.store(true, std::memory_order_release);
}

void CaptureEngine::cmdClear()
{
    cmdClearFlag.store(true, std::memory_order_release);
}

void CaptureEngine::cmdStopCaptureEarly()
{
    cmdStopCaptureEarlyFlag.store(true, std::memory_order_release);
}

void CaptureEngine::handleCommands()
{
    if (cmdClearFlag.exchange(false, std::memory_order_acquire))
    {
        currentState.store(State::Idle, std::memory_order_release);
        capturedLengthSamples.store(0, std::memory_order_release);
        captureWritePos = 0;
        playbackReadPos.store(0, std::memory_order_release);
        recordingSamplesCount.store(0, std::memory_order_release);
        belowStopCounterSamples = 0;
    }

    if (cmdArmFlag.exchange(false, std::memory_order_acquire))
    {
        currentState.store(State::Armed, std::memory_order_release);
        playbackReadPos.store(0, std::memory_order_release);
        recordingSamplesCount.store(0, std::memory_order_release);
        belowStopCounterSamples = 0;
        envelopeFollower.reset();
        currentEnvelopeDb.store(-100.0f, std::memory_order_relaxed);
        preRollBuffer.reset();
    }

    if (cmdDisarmFlag.exchange(false, std::memory_order_acquire))
    {
        if (currentState.load(std::memory_order_relaxed) == State::Armed)
        {
            currentState.store(State::Idle, std::memory_order_release);
        }
    }

    if (cmdPlayFlag.exchange(false, std::memory_order_acquire))
    {
        if (capturedLengthSamples.load(std::memory_order_relaxed) > 0)
        {
            playbackReadPos.store(0, std::memory_order_relaxed);
            currentState.store(State::Playing, std::memory_order_release);
        }
    }

    if (cmdStopPlaybackFlag.exchange(false, std::memory_order_acquire))
    {
        if (currentState.load(std::memory_order_relaxed) == State::Playing)
        {
            currentState.store(State::Captured, std::memory_order_release);
        }
    }

    if (cmdStopCaptureEarlyFlag.exchange(false, std::memory_order_acquire))
    {
        if (currentState.load(std::memory_order_relaxed) == State::Recording)
        {
            finalizeRecording(captureWritePos);
        }
    }
}

void CaptureEngine::startRecording(float preRollMs)
{
    const int requestedPreRoll = static_cast<int>((preRollMs * 0.001f) * currentSampleRate);
    const int available = preRollBuffer.getAvailableSamples();
    const int preRollSamples = std::min(requestedPreRoll, available);

    captureWritePos = 0;
    if (preRollSamples > 0)
    {
        preRollBuffer.copyRecentTo(captureBuffer, 0, preRollSamples, 3.0f);
        captureWritePos = preRollSamples;
    }

    belowStopCounterSamples = 0;
    currentState.store(State::Recording, std::memory_order_release);
    recordingSamplesCount.store(captureWritePos, std::memory_order_relaxed);
}

void CaptureEngine::finalizeRecording(int dropSampleIndex)
{
    // Ensure we have a minimum valid clip (at least 50ms)
    const int minTakeSamples = static_cast<int>(0.05f * currentSampleRate);
    const int safeDropIndex = std::max(minTakeSamples, std::min(dropSampleIndex, captureWritePos));

    // Keep up to 15ms of natural decay after the drop point for smooth click-free cosine fade-out
    const int fadeLengthSamples = static_cast<int>(0.015f * currentSampleRate);
    const int availableDecay = std::max(0, captureWritePos - safeDropIndex);
    const int actualFadeSamples = std::min(fadeLengthSamples, availableDecay);

    const int finalLength = safeDropIndex + actualFadeSamples;
    if (actualFadeSamples > 0)
    {
        applyCosineFadeOut(safeDropIndex, finalLength);
    }

    capturedLengthSamples.store(finalLength, std::memory_order_release);
    captureVersion.fetch_add(1, std::memory_order_release);
    playbackReadPos.store(0, std::memory_order_relaxed);
    currentState.store(State::Captured, std::memory_order_release);
}

void CaptureEngine::applyCosineFadeOut(int startSample, int endSample)
{
    const int fadeSamples = endSample - startSample;
    if (fadeSamples <= 0)
        return;

    const int numChans = std::min(channels, captureBuffer.getNumChannels());
    for (int ch = 0; ch < numChans; ++ch)
    {
        float* data = captureBuffer.getWritePointer(ch, startSample);
        for (int i = 0; i < fadeSamples; ++i)
        {
            // Quarter-cycle cosine fade: 1.0 down to 0.0
            const float progress = static_cast<float>(i) / static_cast<float>(fadeSamples);
            const float gain = std::cos(0.5f * juce::MathConstants<float>::pi * progress);
            data[i] *= gain;
        }
    }
}

void CaptureEngine::processBlock(const juce::AudioBuffer<float>& inputBuffer,
                                 juce::AudioBuffer<float>& loopOutputBuffer,
                                 const CaptureSettings& settings)
{
    handleCommands();

    const int numSamples = inputBuffer.getNumSamples();
    const int numInputChans = inputBuffer.getNumChannels();
    const int numOutChans = loopOutputBuffer.getNumChannels();

    loopOutputBuffer.clear();

    if (numSamples <= 0)
        return;

    // Continuously buffer incoming audio in ring buffer
    preRollBuffer.writeBlock(inputBuffer);

    // Hysteresis: Ensure stop threshold is strictly lower than start threshold
    const float effectiveStopThresholdDb = std::min(settings.stopThresholdDb, settings.startThresholdDb - 1.0f);
    const int stopHoldSamples = std::max(1, static_cast<int>((settings.stopHoldMs * 0.001f) * currentSampleRate));

    State state = currentState.load(std::memory_order_relaxed);
    float lastEnvDb = -100.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Detect highest input across channels
        float maxSample = 0.0f;
        for (int ch = 0; ch < numInputChans; ++ch)
        {
            maxSample = std::max(maxSample, std::abs(inputBuffer.getSample(ch, i)));
        }

        envelopeFollower.processSample(maxSample);
        const float envDb = envelopeFollower.getCurrentDb();
        lastEnvDb = envDb;

        if (state == State::Armed)
        {
            if (envDb >= settings.startThresholdDb)
            {
                startRecording(settings.preRollMs);
                state = State::Recording;

                // Write current sample into capture buffer
                if (captureWritePos < maxBufferSamples)
                {
                    for (int ch = 0; ch < channels; ++ch)
                    {
                        const int srcCh = std::min(ch, numInputChans - 1);
                        captureBuffer.setSample(ch, captureWritePos, inputBuffer.getSample(srcCh, i));
                    }
                    captureWritePos++;
                }
            }
        }
        else if (state == State::Recording)
        {
            if (captureWritePos < maxBufferSamples)
            {
                for (int ch = 0; ch < channels; ++ch)
                {
                    const int srcCh = std::min(ch, numInputChans - 1);
                    captureBuffer.setSample(ch, captureWritePos, inputBuffer.getSample(srcCh, i));
                }
                captureWritePos++;
            }

            if (envDb < effectiveStopThresholdDb)
            {
                belowStopCounterSamples++;
                if (belowStopCounterSamples >= stopHoldSamples)
                {
                    const int dropIndex = captureWritePos - belowStopCounterSamples;
                    finalizeRecording(dropIndex);
                    state = State::Captured;
                }
            }
            else
            {
                belowStopCounterSamples = 0;
            }

            if (captureWritePos >= maxBufferSamples && state == State::Recording)
            {
                finalizeRecording(captureWritePos);
                state = State::Captured;
            }
        }
    }

    if (state == State::Recording)
    {
        recordingSamplesCount.store(captureWritePos, std::memory_order_relaxed);
    }

    currentEnvelopeDb.store(lastEnvDb, std::memory_order_relaxed);

    // Playback handling
    if (state == State::Playing)
    {
        const int clipLength = capturedLengthSamples.load(std::memory_order_relaxed);
        if (clipLength > 0)
        {
            int readPos = playbackReadPos.load(std::memory_order_relaxed);
            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < numOutChans; ++ch)
                {
                    const int srcCh = std::min(ch, captureBuffer.getNumChannels() - 1);
                    loopOutputBuffer.setSample(ch, i, captureBuffer.getSample(srcCh, readPos));
                }
                readPos = (readPos + 1) % clipLength;
            }
            playbackReadPos.store(readPos, std::memory_order_relaxed);
        }
    }
}

bool CaptureEngine::copyCapturedAudio(juce::AudioBuffer<float>& destBuffer) const
{
    const int len = capturedLengthSamples.load(std::memory_order_acquire);
    if (len <= 0)
        return false;

    destBuffer.setSize(channels, len, false, false, true);
    for (int ch = 0; ch < channels; ++ch)
    {
        destBuffer.copyFrom(ch, 0, captureBuffer, ch, 0, len);
    }
    return true;
}

} // namespace loopbro
