#include "LevelMeterComponent.h"
#include <algorithm>

namespace loopbro
{

LevelMeterComponent::LevelMeterComponent()
{
    setOpaque(false);
}

void LevelMeterComponent::setLevelDb(float db)
{
    currentLevelDb = db;
    // Smooth decay for visual appeal
    if (currentLevelDb > displayedLevelDb)
        displayedLevelDb = currentLevelDb;
    else
        displayedLevelDb = displayedLevelDb * 0.85f + currentLevelDb * 0.15f;

    repaint();
}

void LevelMeterComponent::setThresholds(float startDb, float stopDb)
{
    if (startThresholdDb != startDb || stopThresholdDb != stopDb)
    {
        startThresholdDb = startDb;
        stopThresholdDb = stopDb;
        repaint();
    }
}

float LevelMeterComponent::dbToY(float db, float height) const
{
    const float clampedDb = juce::jlimit(minDb, maxDb, db);
    const float norm = (clampedDb - minDb) / (maxDb - minDb); // 0.0 at minDb, 1.0 at maxDb
    return height * (1.0f - norm);
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background track
    g.setColour(juce::Colour(0xff181a1f));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour(0xff2d3139));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Active level bar
    const float meterY = dbToY(displayedLevelDb, h);
    const float barHeight = h - meterY;

    if (barHeight > 0.0f)
    {
        juce::ColourGradient grad(
            juce::Colour(0xff4cd964), 0.0f, h,
            juce::Colour(0xffff9500), 0.0f, h * 0.35f,
            false);
        grad.addColour(0.85, juce::Colour(0xffff3b30));

        g.setGradientFill(grad);
        g.fillRoundedRectangle(1.0f, meterY, w - 2.0f, barHeight, 3.0f);
    }

    // Stop Threshold Marker Line (Cyan/Blue)
    const float stopY = dbToY(stopThresholdDb, h);
    g.setColour(juce::Colour(0xff34c759).withAlpha(0.9f));
    g.drawLine(0.0f, stopY, w, stopY, 1.5f);

    // Start Threshold Marker Line (Amber/Orange)
    const float startY = dbToY(startThresholdDb, h);
    g.setColour(juce::Colour(0xffff9f0a).withAlpha(0.9f));
    g.drawLine(0.0f, startY, w, startY, 2.0f);

    // Small tick marks and labels
    g.setFont(juce::FontOptions(9.0f));
    const float dBs[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f };
    for (float db : dBs)
    {
        const float y = dbToY(db, h);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawLine(w - 5.0f, y, w, y, 1.0f);

        if (w >= 30.0f)
        {
            g.drawText(juce::String(static_cast<int>(db)), 2, static_cast<int>(y - 6.0f), static_cast<int>(w - 8.0f), 12, juce::Justification::centredRight);
        }
    }
}

void LevelMeterComponent::resized()
{
}

} // namespace loopbro
