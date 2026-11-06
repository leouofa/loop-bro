#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace loopbro
{

class PreRollRingBuffer
{
public:
    PreRollRingBuffer() = default;

    void prepare(double sampleRate, int numChannels, float maxDurationSec = 2.5f)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        channels = std::max(1, numChannels);
        bufferSize = static_cast<int>(std::ceil(maxDurationSec * currentSampleRate)) + 1024;
        ringBuffer.setSize(channels, bufferSize, false, true, true);
        reset();
    }

    void reset()
    {
        ringBuffer.clear();
        writePos = 0;
        totalRecordedSamples = 0;
    }

    void writeBlock(const juce::AudioBuffer<float>& input)
    {
        const int numSamples = input.getNumSamples();
        const int numChans = std::min(channels, input.getNumChannels());

        if (numSamples <= 0 || bufferSize <= 0)
            return;

        for (int ch = 0; ch < numChans; ++ch)
        {
            const float* src = input.getReadPointer(ch);
            int localWrite = writePos;
            int samplesLeft = numSamples;
            int srcOffset = 0;

            while (samplesLeft > 0)
            {
                const int spaceUntilEnd = bufferSize - localWrite;
                const int toCopy = std::min(samplesLeft, spaceUntilEnd);
                ringBuffer.copyFrom(ch, localWrite, src + srcOffset, toCopy);

                localWrite = (localWrite + toCopy) % bufferSize;
                srcOffset += toCopy;
                samplesLeft -= toCopy;
            }
        }

        // If input has fewer channels than ring buffer, duplicate channel 0
        for (int ch = numChans; ch < channels; ++ch)
        {
            ringBuffer.copyFrom(ch, 0, ringBuffer, 0, 0, bufferSize);
        }

        writePos = (writePos + numSamples) % bufferSize;
        totalRecordedSamples = std::min(totalRecordedSamples + numSamples, bufferSize);
    }

    int getAvailableSamples() const noexcept
    {
        return totalRecordedSamples;
    }

    int copyRecentTo(juce::AudioBuffer<float>& dest, int destStartSample, int requestedSamples, float fadeInMs = 3.0f) const
    {
        if (requestedSamples <= 0 || totalRecordedSamples <= 0 || bufferSize <= 0)
            return 0;

        const int actualSamples = std::min(requestedSamples, totalRecordedSamples);
        const int numChans = std::min(channels, dest.getNumChannels());

        // Start index in ring buffer: writePos - actualSamples (mod bufferSize)
        int readPos = (writePos - actualSamples + bufferSize) % bufferSize;

        for (int ch = 0; ch < numChans; ++ch)
        {
            int localRead = readPos;
            int localDest = destStartSample;
            int samplesLeft = actualSamples;

            while (samplesLeft > 0)
            {
                const int spaceUntilEnd = bufferSize - localRead;
                const int toCopy = std::min(samplesLeft, spaceUntilEnd);

                dest.copyFrom(ch, localDest, ringBuffer, ch, localRead, toCopy);

                localRead = (localRead + toCopy) % bufferSize;
                localDest += toCopy;
                samplesLeft -= toCopy;
            }

            // Apply fade-in to avoid clicks at pre-roll start
            const int fadeInSamples = std::min(actualSamples, static_cast<int>((fadeInMs * 0.001f) * currentSampleRate));
            if (fadeInSamples > 0)
            {
                float* destPtr = dest.getWritePointer(ch, destStartSample);
                for (int i = 0; i < fadeInSamples; ++i)
                {
                    // Raised cosine fade-in: 0.5 * (1 - cos(pi * i / N))
                    const float gain = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(fadeInSamples)));
                    destPtr[i] *= gain;
                }
            }
        }

        return actualSamples;
    }

private:
    double currentSampleRate = 44100.0;
    int channels = 2;
    int bufferSize = 0;
    int writePos = 0;
    int totalRecordedSamples = 0;
    juce::AudioBuffer<float> ringBuffer;
};

} // namespace loopbro
