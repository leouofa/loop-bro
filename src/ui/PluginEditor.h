#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LevelMeterComponent.h"
#include "WaveformDragComponent.h"

namespace loopbro
{

class LoopBroAudioProcessor;

class CustomKnobLook : public juce::LookAndFeel_V4
{
public:
    CustomKnobLook();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};

class LoopBroAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit LoopBroAudioProcessorEditor(LoopBroAudioProcessor&);
    ~LoopBroAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LoopBroAudioProcessor& processorRef;

    CustomKnobLook customKnobLook;

    // Header buttons
    juce::TextButton armButton { "ARM" };
    juce::TextButton playButton { "PLAY LOOP" };
    juce::TextButton clearButton { "CLEAR" };
    juce::ToggleButton thruButton { "LIVE THRU" };
    juce::TextButton saveWavButton { "SAVE WAV..." };

    // Center Display
    LevelMeterComponent levelMeter;
    WaveformDragComponent waveformDisplay;

    // Bottom parameter sliders
    juce::Slider startThreshSlider;
    juce::Slider stopThreshSlider;
    juce::Slider preRollSlider;
    juce::Slider stopHoldSlider;
    juce::Slider loopGainSlider;

    juce::Label startThreshLabel;
    juce::Label stopThreshLabel;
    juce::Label preRollLabel;
    juce::Label stopHoldLabel;
    juce::Label loopGainLabel;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stopThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preRollAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stopHoldAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loopGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thruAttach;

    // State tracking
    State lastState = State::Idle;
    int lastCaptureVersion = -1;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void updateStatusUI(State currentState);
    void saveWavFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopBroAudioProcessorEditor)
};

} // namespace loopbro
