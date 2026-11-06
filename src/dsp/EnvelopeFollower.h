#pragma once

#include <cmath>
#include <algorithm>

namespace loopbro
{

class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        updateCoefficients();
        reset();
    }

    void setAttackRelease(float attackMs, float releaseMs)
    {
        attackTimeMs = std::max(0.0f, attackMs);
        releaseTimeMs = std::max(0.1f, releaseMs);
        updateCoefficients();
    }

    void reset()
    {
        envelope = 0.0f;
    }

    inline float processSample(float inputSample) noexcept
    {
        const float absIn = std::abs(inputSample);
        if (absIn > envelope)
        {
            if (attackCoeff <= 1.0e-5f)
                envelope = absIn;
            else
                envelope = attackCoeff * envelope + (1.0f - attackCoeff) * absIn;
        }
        else
        {
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * absIn;
        }
        return envelope;
    }

    float getCurrentLevel() const noexcept
    {
        return envelope;
    }

    float getCurrentDb() const noexcept
    {
        if (envelope <= 1.0e-5f)
            return -100.0f;
        return 20.0f * std::log10(envelope);
    }

private:
    void updateCoefficients()
    {
        // One-pole smoothing: coeff = exp(-1.0 / (timeInSeconds * sampleRate))
        const double attackSamples = (attackTimeMs * 0.001) * currentSampleRate;
        const double releaseSamples = (releaseTimeMs * 0.001) * currentSampleRate;

        attackCoeff = attackSamples > 0.001 ? static_cast<float>(std::exp(-1.0 / attackSamples)) : 0.0f;
        releaseCoeff = releaseSamples > 0.001 ? static_cast<float>(std::exp(-1.0 / releaseSamples)) : 0.0f;
    }

    double currentSampleRate = 44100.0;
    float attackTimeMs = 0.0f;    // instantaneous transient capture on strum
    float releaseTimeMs = 20.0f;  // smooth over guitar bass cycles (82Hz low E is ~12.2ms period)
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
};

} // namespace loopbro
