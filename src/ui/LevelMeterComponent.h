#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace loopbro
{

class LevelMeterComponent : public juce::Component
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override = default;

    void setLevelDb(float db);
    void setThresholds(float startDb, float stopDb);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    float currentLevelDb = -100.0f;
    float displayedLevelDb = -100.0f;
    float startThresholdDb = -30.0f;
    float stopThresholdDb = -48.0f;

    static constexpr float minDb = -80.0f;
    static constexpr float maxDb = 0.0f;

    float dbToY(float db, float height) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};

} // namespace loopbro
