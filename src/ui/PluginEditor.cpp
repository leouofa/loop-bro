#include "PluginEditor.h"
#include "../plugin/PluginProcessor.h"

namespace loopbro
{

CustomKnobLook::CustomKnobLook()
{
}

void CustomKnobLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPosProportional, float rotaryStartAngle,
                                      float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.46f;
    const auto center = bounds.getCentre();

    // Background track arc
    juce::Path bgArc;
    bgArc.addCentredArc(center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff22262d));
    g.strokePath(bgArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value active arc
    const float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    if (currentAngle > rotaryStartAngle)
    {
        juce::Path valArc;
        valArc.addCentredArc(center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, currentAngle, true);
        g.setColour(juce::Colour(0xff0a84ff));
        g.strokePath(valArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Inner dial body
    const float dialRadius = radius * 0.78f;
    g.setColour(juce::Colour(0xff1e2229));
    g.fillEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

    g.setColour(juce::Colour(0xff333a46));
    g.drawEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.2f);

    // Indicator pointer line
    const float ptrLen = dialRadius * 0.85f;
    const float ptrX = center.x + ptrLen * std::sin(currentAngle);
    const float ptrY = center.y - ptrLen * std::cos(currentAngle);

    g.setColour(juce::Colours::white);
    g.drawLine(center.x, center.y, ptrX, ptrY, 2.0f);
}

LoopBroAudioProcessorEditor::LoopBroAudioProcessorEditor(LoopBroAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&customKnobLook);

    // Header buttons
    armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303642));
    armButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    armButton.onClick = [this]
    {
        auto& engine = processorRef.getCaptureEngine();
        if (engine.getState() == State::Armed)
            engine.cmdDisarm();
        else
            engine.cmdArm();
    };
    addAndMakeVisible(armButton);

    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303642));
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playButton.onClick = [this]
    {
        auto& engine = processorRef.getCaptureEngine();
        if (engine.getState() == State::Playing)
            engine.cmdStopPlayback();
        else
            engine.cmdPlay();
    };
    addAndMakeVisible(playButton);

    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a292b));
    clearButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff6961));
    clearButton.onClick = [this]
    {
        processorRef.getCaptureEngine().cmdClear();
        waveformDisplay.clearWaveform();
        processorRef.setStagedWavFile({});
    };
    addAndMakeVisible(clearButton);

    thruButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(thruButton);

    saveWavButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1d3f5e));
    saveWavButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff70bfff));
    saveWavButton.onClick = [this] { saveWavFile(); };
    addAndMakeVisible(saveWavButton);

    // Center Components
    addAndMakeVisible(levelMeter);
    addAndMakeVisible(waveformDisplay);

    // Bottom parameter sliders helper
    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(s);

        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(l);
    };

    setupSlider(startThreshSlider, startThreshLabel, "START THRESH");
    setupSlider(stopThreshSlider,  stopThreshLabel,  "STOP THRESH");
    setupSlider(preRollSlider,     preRollLabel,     "PRE-ROLL");
    setupSlider(stopHoldSlider,    stopHoldLabel,    "STOP HOLD");
    setupSlider(loopGainSlider,    loopGainLabel,    "LOOP LEVEL");

    // Attachments
    auto& apvts = processorRef.getAPVTS();
    startThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::startThresholdDb.getParamID(), startThreshSlider);
    stopThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::stopThresholdDb.getParamID(), stopThreshSlider);
    preRollAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::preRollMs.getParamID(), preRollSlider);
    stopHoldAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::stopHoldMs.getParamID(), stopHoldSlider);
    loopGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParameterIDs::loopGainDb.getParamID(), loopGainSlider);
    thruAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParameterIDs::inputThru.getParamID(), thruButton);

    setSize(760, 440);
    setResizable(true, true);
    setResizeLimits(640, 380, 1100, 700);

    startTimerHz(40); // 40 Hz smooth UI update
}

LoopBroAudioProcessorEditor::~LoopBroAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void LoopBroAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background gradient
    g.fillAll(juce::Colour(0xff121316));

    // Top Header Bar
    const auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), 56);
    g.setColour(juce::Colour(0xff1a1d24));
    g.fillRect(headerBounds);

    g.setColour(juce::Colour(0xff292e39));
    g.drawHorizontalLine(55, 0.0f, static_cast<float>(getWidth()));

    // Title & Branding
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    g.drawText("LOOP BRO", 18, 0, 130, 56, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xff0a84ff));
    g.drawText("SMART GUITAR CAPTURE", 140, 6, 160, 44, juce::Justification::centredLeft);
}

void LoopBroAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Top Header Controls
    int x = w - 16;
    const int btnH = 32;

    x -= 100;
    saveWavButton.setBounds(x, 12, 96, btnH);

    x -= 96;
    clearButton.setBounds(x, 12, 88, btnH);

    x -= 106;
    playButton.setBounds(x, 12, 98, btnH);

    x -= 88;
    armButton.setBounds(x, 12, 80, btnH);

    x -= 104;
    thruButton.setBounds(x, 12, 96, btnH);

    // Center area (from Y=68 to Y=h-148)
    const int centerTop = 68;
    const int bottomControlsH = 138;
    const int centerH = h - centerTop - bottomControlsH;

    const int meterW = 34;
    levelMeter.setBounds(18, centerTop, meterW, centerH);

    const int waveX = 18 + meterW + 12;
    const int waveW = w - waveX - 18;
    waveformDisplay.setBounds(waveX, centerTop, waveW, centerH);

    // Bottom Knobs (5 knobs evenly distributed)
    const int knobY = h - bottomControlsH + 12;
    const int knobW = (w - 36) / 5;
    const int dialH = 74;

    for (int i = 0; i < 5; ++i)
    {
        const int colX = 18 + i * knobW;
        juce::Label* l = nullptr;
        juce::Slider* s = nullptr;

        switch (i)
        {
            case 0: l = &startThreshLabel; s = &startThreshSlider; break;
            case 1: l = &stopThreshLabel;  s = &stopThreshSlider;  break;
            case 2: l = &preRollLabel;     s = &preRollSlider;     break;
            case 3: l = &stopHoldLabel;    s = &stopHoldSlider;    break;
            case 4: l = &loopGainLabel;    s = &loopGainSlider;    break;
        }

        if (l != nullptr && s != nullptr)
        {
            l->setBounds(colX, knobY, knobW, 16);
            s->setBounds(colX + (knobW - 84) / 2, knobY + 18, 84, dialH);
        }
    }
}

void LoopBroAudioProcessorEditor::timerCallback()
{
    auto& engine = processorRef.getCaptureEngine();
    const State state = engine.getState();
    const double sr = engine.getSampleRate();

    // Update Meter
    levelMeter.setLevelDb(engine.getCurrentEnvelopeDb());
    levelMeter.setThresholds(static_cast<float>(startThreshSlider.getValue()),
                             static_cast<float>(stopThreshSlider.getValue()));

    // Update Waveform & Playhead
    waveformDisplay.setPlaybackInfo(state, engine.getPlaybackPos(), engine.getRecordingSamples(), sr);

    // If new capture take completed, stage WAV and refresh waveform
    const int ver = engine.getCaptureVersion();
    if (ver != lastCaptureVersion)
    {
        lastCaptureVersion = ver;
        juce::AudioBuffer<float> temp;
        if (engine.copyCapturedAudio(temp))
        {
            waveformDisplay.updateWaveform(temp, temp.getNumSamples(), sr);
            auto stagedFile = processorRef.getStagedWavFile();
            if (!stagedFile.existsAsFile())
            {
                stagedFile = WavExporter::stageTempWavFile(temp, sr);
                processorRef.setStagedWavFile(stagedFile);
            }
            waveformDisplay.setStagedFile(stagedFile);
        }
    }

    if (state != lastState)
    {
        lastState = state;
        updateStatusUI(state);
    }
}

void LoopBroAudioProcessorEditor::updateStatusUI(State currentState)
{
    switch (currentState)
    {
        case State::Idle:
            armButton.setButtonText("ARM");
            armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303642));
            playButton.setButtonText("PLAY LOOP");
            playButton.setEnabled(processorRef.getCaptureEngine().getCapturedLength() > 0);
            break;

        case State::Armed:
            armButton.setButtonText("DISARM");
            armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff9f0a));
            playButton.setButtonText("PLAY LOOP");
            playButton.setEnabled(false);
            break;

        case State::Recording:
            armButton.setButtonText("STOP REC");
            armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff453a));
            playButton.setButtonText("PLAY LOOP");
            playButton.setEnabled(false);
            break;

        case State::Captured:
            armButton.setButtonText("RE-ARM");
            armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303642));
            playButton.setButtonText("PLAY LOOP");
            playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34c759));
            playButton.setEnabled(true);
            break;

        case State::Playing:
            armButton.setButtonText("ARM");
            armButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303642));
            playButton.setButtonText("STOP LOOP");
            playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff9f0a));
            playButton.setEnabled(true);
            break;
    }
}

void LoopBroAudioProcessorEditor::saveWavFile()
{
    auto& engine = processorRef.getCaptureEngine();
    if (engine.getCapturedLength() <= 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Clip Captured",
            "Please arm and record a guitar riff before saving.");
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Captured Riff as WAV",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("LoopBro_Riff.wav"),
        "*.wav");

    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result != juce::File{})
        {
            juce::AudioBuffer<float> temp;
            if (processorRef.getCaptureEngine().copyCapturedAudio(temp))
            {
                WavExporter::exportToWav(temp, processorRef.getCaptureEngine().getSampleRate(), result, 24);
            }
        }
    });
}

} // namespace loopbro
