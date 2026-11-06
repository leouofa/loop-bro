#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "../dsp/CaptureEngine.h"

namespace loopbro
{

class WaveformDragComponent : public juce::Component
{
public:
    WaveformDragComponent();
    ~WaveformDragComponent() override = default;

    void updateWaveform(const juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate);
    void clearWaveform();

    void setPlaybackInfo(State state, int playbackPosSamples, int recordingSamples, double sampleRate);
    void setStagedFile(const juce::File& file);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    std::vector<std::pair<float, float>> thumbnailPeaks; // min, max per column
    int totalSamples = 0;
    double currentSampleRate = 44100.0;
    State currentState = State::Idle;
    int currentPlaybackPos = 0;
    int currentRecordingSamples = 0;
    juce::File stagedWav;
    bool hasInitiatedDrag = false;

    void generateThumbnail(const juce::AudioBuffer<float>& buffer, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDragComponent)
};

} // namespace loopbro
