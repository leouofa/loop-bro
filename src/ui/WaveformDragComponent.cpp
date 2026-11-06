#include "WaveformDragComponent.h"

namespace loopbro
{

WaveformDragComponent::WaveformDragComponent()
{
    setOpaque(false);
}

void WaveformDragComponent::updateWaveform(const juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
{
    totalSamples = numSamples;
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    generateThumbnail(buffer, numSamples);
    repaint();
}

void WaveformDragComponent::clearWaveform()
{
    thumbnailPeaks.clear();
    totalSamples = 0;
    repaint();
}

void WaveformDragComponent::setPlaybackInfo(State state, int playbackPosSamples, int recordingSamples, double sampleRate)
{
    currentState = state;
    currentPlaybackPos = playbackPosSamples;
    currentRecordingSamples = recordingSamples;
    if (sampleRate > 0.0) currentSampleRate = sampleRate;
    repaint();
}

void WaveformDragComponent::setStagedFile(const juce::File& file)
{
    stagedWav = file;
    repaint();
}

void WaveformDragComponent::generateThumbnail(const juce::AudioBuffer<float>& buffer, int numSamples)
{
    thumbnailPeaks.clear();
    if (numSamples <= 0 || buffer.getNumSamples() <= 0)
        return;

    const int numBins = 512;
    thumbnailPeaks.resize(numBins, { 0.0f, 0.0f });

    const int numChans = buffer.getNumChannels();
    const double samplesPerBin = static_cast<double>(numSamples) / static_cast<double>(numBins);

    for (int bin = 0; bin < numBins; ++bin)
    {
        const int startIdx = static_cast<int>(bin * samplesPerBin);
        const int endIdx = std::min(numSamples, static_cast<int>((bin + 1) * samplesPerBin));

        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (int i = startIdx; i < endIdx; ++i)
        {
            for (int ch = 0; ch < numChans; ++ch)
            {
                const float s = buffer.getSample(ch, i);
                if (s < minVal) minVal = s;
                if (s > maxVal) maxVal = s;
            }
        }

        thumbnailPeaks[bin] = { minVal, maxVal };
    }
}

void WaveformDragComponent::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    hasInitiatedDrag = false;
}

void WaveformDragComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (hasInitiatedDrag)
        return;

    // Only allow drag if we have a staged WAV file and drag distance > 4px
    if (stagedWav.existsAsFile() && e.getDistanceFromDragStart() > 4)
    {
        hasInitiatedDrag = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ stagedWav.getFullPathName() }, false, this);
    }
}

void WaveformDragComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float midY = h * 0.5f;

    // Background card
    g.setColour(juce::Colour(0xff16181d));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff282c34));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Center zero-crossing line
    g.setColour(juce::Colour(0xff22252c));
    g.drawHorizontalLine(static_cast<int>(midY), 0.0f, w);

    if (currentState == State::Idle && thumbnailPeaks.empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(15.0f).withStyle("Medium"));
        g.drawText("ARM LOOP BRO TO START LISTENING", bounds, juce::Justification::centred);
        return;
    }

    if (currentState == State::Armed && thumbnailPeaks.empty())
    {
        g.setColour(juce::Colour(0xffff9f0a));
        g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
        g.drawText("ARMED — LISTENING FOR FIRST GUITAR STRUM...", bounds, juce::Justification::centred);
        return;
    }

    if (currentState == State::Recording)
    {
        const double recSec = static_cast<double>(currentRecordingSamples) / currentSampleRate;
        g.setColour(juce::Colour(0xffff453a));
        g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
        g.drawText(juce::String::formatted("RECORDING...  %.2f s", recSec),
                   juce::Rectangle<float>(0.0f, 0.0f, w, h * 0.5f), juce::Justification::centred);

        g.setFont(juce::FontOptions(13.0f));
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawText("Will auto-stop when strum amplitude drops below Stop Threshold",
                   juce::Rectangle<float>(0.0f, h * 0.5f, w, h * 0.4f), juce::Justification::centred);
        return;
    }

    // Render waveform thumbnail
    if (!thumbnailPeaks.empty())
    {
        const size_t numBins = thumbnailPeaks.size();
        const float binWidth = w / static_cast<float>(numBins);

        juce::Path wavePath;
        wavePath.preallocateSpace(static_cast<int>(numBins * 4));

        // Draw upper waveform half
        for (size_t bin = 0; bin < numBins; ++bin)
        {
            const float x = static_cast<float>(bin) * binWidth;
            const float topY = midY - (thumbnailPeaks[bin].second * (h * 0.44f));
            if (bin == 0)
                wavePath.startNewSubPath(x, topY);
            else
                wavePath.lineTo(x, topY);
        }

        // Draw lower waveform half in reverse
        for (int bin = static_cast<int>(numBins) - 1; bin >= 0; --bin)
        {
            const float x = static_cast<float>(bin) * binWidth;
            const float btmY = midY - (thumbnailPeaks[static_cast<size_t>(bin)].first * (h * 0.44f));
            wavePath.lineTo(x, btmY);
        }

        wavePath.closeSubPath();

        // Gradient fill for waveform
        juce::ColourGradient waveGrad(
            juce::Colour(0xff29b6f6), 0.0f, 0.0f,
            juce::Colour(0xff0288d1), 0.0f, h,
            false);
        g.setGradientFill(waveGrad);
        g.fillPath(wavePath);

        g.setColour(juce::Colour(0xff4fc3f7).withAlpha(0.6f));
        g.strokePath(wavePath, juce::PathStrokeType(1.0f));

        // Playhead indicator if Playing
        if (currentState == State::Playing && totalSamples > 0)
        {
            const float playheadNorm = static_cast<float>(currentPlaybackPos) / static_cast<float>(totalSamples);
            const float playheadX = playheadNorm * w;

            g.setColour(juce::Colour(0xffffd60a));
            g.drawVerticalLine(static_cast<int>(playheadX), 0.0f, h);

            g.setColour(juce::Colour(0xffffd60a).withAlpha(0.2f));
            g.fillRect(playheadX - 1.0f, 0.0f, 3.0f, h);
        }

        // Clip Info & Drag Badge
        const double durationSec = static_cast<double>(totalSamples) / currentSampleRate;
        const juce::String infoText = juce::String::formatted("Length: %.2f s", durationSec);

        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawText(infoText, 12, 8, 200, 20, juce::Justification::topLeft);

        // "DRAG TO DAW" Badge in top-right
        if (stagedWav.existsAsFile())
        {
            const auto badgeBounds = juce::Rectangle<float>(w - 150.0f, 8.0f, 138.0f, 24.0f);
            g.setColour(juce::Colour(0xff0a84ff));
            g.fillRoundedRectangle(badgeBounds, 12.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
            g.drawText("DRAG TO DAW", badgeBounds, juce::Justification::centred);
        }
    }
}

void WaveformDragComponent::resized()
{
}

} // namespace loopbro
