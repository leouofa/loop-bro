#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace loopbro
{

class WavExporter
{
public:
    WavExporter() = default;

    static bool exportToWav(const juce::AudioBuffer<float>& buffer,
                            double sampleRate,
                            const juce::File& destinationFile,
                            int bitDepth = 24);

    static juce::File stageTempWavFile(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       int bitDepth = 24);

    static void cleanTempDirectory();
};

} // namespace loopbro
