#include "GateEngine.h"
#include "../common/Math.h"
#include <algorithm>
#include <cmath>

namespace growlforge {

void GateEngine::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
}

void GateEngine::reset() {
    envelope_ = 0.0f;
    gain_ = 1.0f;
    open_ = true;
    holdSamples_ = 0;
}

void GateEngine::processFrame(float& left, float& right, uint32_t channelCount, double amount) {
    amount = clamp(amount, 0.0, 1.0);
    if (amount <= 0.0) {
        reset();
        return;
    }

    const float detector = channelCount > 1
        ? std::max(std::abs(left), std::abs(right))
        : std::abs(left);
    const float attack = static_cast<float>(std::exp(-1.0 / (0.00045 * sampleRate_)));
    const float release = static_cast<float>(std::exp(-1.0 / (0.060 * sampleRate_)));
    envelope_ = detector > envelope_
        ? attack * envelope_ + (1.0f - attack) * detector
        : release * envelope_ + (1.0f - release) * detector;

    const double openThreshold = dbToGain(-90.0 + 58.0 * amount);
    const double closeThreshold = openThreshold * (0.48 + 0.10 * (1.0 - amount));

    if (!open_ && envelope_ >= openThreshold) {
        open_ = true;
        holdSamples_ = static_cast<uint32_t>(0.024 * sampleRate_);
    } else if (open_) {
        if (envelope_ >= closeThreshold) {
            holdSamples_ = static_cast<uint32_t>(0.024 * sampleRate_);
        } else if (holdSamples_ > 0) {
            --holdSamples_;
        } else {
            open_ = false;
        }
    }

    const float target = open_ ? 1.0f : 0.0f;
    const double transition = open_ ? 0.0010 : (0.075 + 0.085 * amount);
    const float coeff = static_cast<float>(std::exp(-1.0 / (transition * sampleRate_)));
    gain_ = coeff * gain_ + (1.0f - coeff) * target;
    if (gain_ < 1.0e-5f) gain_ = 0.0f;

    left *= gain_;
    if (channelCount > 1) right *= gain_;
}

} // namespace growlforge
