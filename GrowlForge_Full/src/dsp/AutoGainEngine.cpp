#include "AutoGainEngine.h"
#include "../common/Math.h"
#include <algorithm>
#include <cmath>

namespace growlforge {

void AutoGainEngine::DetectorChannel::reset() {
    dryHighpass.reset();
    dryLowpass.reset();
    wetHighpass.reset();
    wetLowpass.reset();
}

void AutoGainEngine::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    for (auto& channel : channels_) {
        channel.dryHighpass.setLowpass(75.0, sampleRate_);
        channel.dryLowpass.setLowpass(6500.0, sampleRate_);
        channel.wetHighpass.setLowpass(75.0, sampleRate_);
        channel.wetLowpass.setLowpass(6500.0, sampleRate_);
    }
}

void AutoGainEngine::reset() {
    for (auto& channel : channels_) channel.reset();
    dryFast2_ = wetFast2_ = drySlow2_ = wetSlow2_ = 1.0e-10;
    targetGain_ = appliedGain_ = 1.0;
}

float AutoGainEngine::weightedSample(float sample, OnePole& highpass, OnePole& lowpass) {
    // Remove subsonic energy, gently de-emphasize the extreme top, and retain a
    // small broadband component so pick attack still contributes to loudness.
    const float highpassed = highpass.hp(sample);
    const float midWeighted = lowpass.lp(highpassed);
    return 0.86f * midWeighted + 0.14f * sample;
}

float AutoGainEngine::processFrame(float dryLeft, float dryRight,
                                   float wetLeft, float wetRight,
                                   bool enabled, uint32_t channelCount) {
    channelCount = std::clamp<uint32_t>(channelCount, 1u, 2u);
    const float dry[2]{dryLeft, channelCount > 1 ? dryRight : dryLeft};
    const float wet[2]{wetLeft, channelCount > 1 ? wetRight : wetLeft};

    double dryFrame2 = 0.0;
    double wetFrame2 = 0.0;
    for (uint32_t channel = 0; channel < channelCount; ++channel) {
        auto& detector = channels_[channel];
        const float weightedDry = weightedSample(dry[channel], detector.dryHighpass, detector.dryLowpass);
        const float weightedWet = weightedSample(wet[channel], detector.wetHighpass, detector.wetLowpass);
        dryFrame2 += static_cast<double>(weightedDry) * weightedDry;
        wetFrame2 += static_cast<double>(weightedWet) * weightedWet;
    }
    dryFrame2 /= channelCount;
    wetFrame2 /= channelCount;

    const double fastCoeff = std::exp(-1.0 / (0.045 * sampleRate_));
    const double slowCoeff = std::exp(-1.0 / (0.480 * sampleRate_));
    dryFast2_ = fastCoeff * dryFast2_ + (1.0 - fastCoeff) * dryFrame2;
    wetFast2_ = fastCoeff * wetFast2_ + (1.0 - fastCoeff) * wetFrame2;
    drySlow2_ = slowCoeff * drySlow2_ + (1.0 - slowCoeff) * dryFrame2;
    wetSlow2_ = slowCoeff * wetSlow2_ + (1.0 - slowCoeff) * wetFrame2;

    if (enabled) {
        constexpr double silence2 = 1.0e-8; // approximately -80 dBFS power
        if (drySlow2_ > silence2 && wetSlow2_ > 1.0e-11) {
            double measured = std::sqrt((drySlow2_ + 1.0e-11) / (wetSlow2_ + 1.0e-11));
            measured = clamp(measured, dbToGain(-12.0), dbToGain(12.0));

            // During sharp transients, rely less on the instantaneous loudness
            // ratio. This prevents the correction from chasing pick peaks.
            const double dryCrest = std::sqrt((dryFast2_ + 1.0e-11) / (drySlow2_ + 1.0e-11));
            const double wetCrest = std::sqrt((wetFast2_ + 1.0e-11) / (wetSlow2_ + 1.0e-11));
            const double transient = clamp(std::max(dryCrest, wetCrest) - 1.0, 0.0, 2.0) * 0.5;
            measured = 1.0 + (measured - 1.0) * (1.0 - 0.32 * transient);

            const double targetCoeff = std::exp(-1.0 / (0.650 * sampleRate_));
            targetGain_ = targetCoeff * targetGain_ + (1.0 - targetCoeff) * measured;
        }
        // Hold the target during silence rather than drifting toward a large
        // correction based on numerical residue.
    } else {
        targetGain_ = 1.0;
    }

    const bool reducing = targetGain_ < appliedGain_;
    const double time = enabled ? (reducing ? 0.220 : 0.850) : 0.120;
    const double gainCoeff = std::exp(-1.0 / (time * sampleRate_));
    appliedGain_ = gainCoeff * appliedGain_ + (1.0 - gainCoeff) * targetGain_;
    appliedGain_ = clamp(appliedGain_, dbToGain(-12.0), dbToGain(12.0));
    return static_cast<float>(appliedGain_);
}

double AutoGainEngine::correctionDb() const {
    return clamp(gainToDb(appliedGain_), -12.0, 12.0);
}

double AutoGainEngine::commitCorrectionDb() {
    const double result = correctionDb();
    reset();
    return result;
}

} // namespace growlforge
