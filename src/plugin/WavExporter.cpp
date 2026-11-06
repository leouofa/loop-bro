#include "WavExporter.h"

namespace loopbro
{

bool WavExporter::exportToWav(const juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              const juce::File& destinationFile,
                              int bitDepth)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return false;

    destinationFile.getParentDirectory().createDirectory();
    if (destinationFile.exists())
        destinationFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> fileStream(destinationFile.createOutputStream());
    if (fileStream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(fileStream.get(),
                                  sampleRate,
                                  static_cast<unsigned int>(numChannels),
                                  bitDepth,
                                  {},
                                  0));

    if (writer == nullptr)
        return false;

    // fileStream ownership transferred to writer if successful
    fileStream.release();

    const bool success = writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
    return success;
}

juce::File WavExporter::stageTempWavFile(const juce::AudioBuffer<float>& buffer,
                                        double sampleRate,
                                        int bitDepth)
{
    cleanTempDirectory();

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("LoopBro");
    tempDir.createDirectory();

    const juce::String filename = "LoopBro_Take_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".wav";
    auto targetFile = tempDir.getChildFile(filename);

    if (exportToWav(buffer, sampleRate, targetFile, bitDepth))
        return targetFile;

    return {};
}

void WavExporter::cleanTempDirectory()
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("LoopBro");
    if (!tempDir.isDirectory())
        return;

    auto files = tempDir.findChildFiles(juce::File::findFiles, false, "LoopBro_Take_*.wav");
    const auto cutoffTime = juce::Time::getCurrentTime() - juce::RelativeTime::hours(24);

    for (const auto& file : files)
    {
        if (file.getLastModificationTime() < cutoffTime)
        {
            file.deleteFile();
        }
    }
}

} // namespace loopbro
