#include "BypassController.h"
#include <algorithm>

namespace growlforge {

void BypassController::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
}

void BypassController::reset(bool bypassed) {
    wetMix_ = bypassed ? 0.0f : 1.0f;
}

void BypassController::processFrame(float dryLeft, float dryRight,
                                    float wetLeft, float wetRight,
                                    bool bypassed, float& outLeft, float& outRight) {
    const float target = bypassed ? 0.0f : 1.0f;
    const float step = static_cast<float>(1.0 / (0.015 * sampleRate_));
    if (wetMix_ < target) wetMix_ = std::min(target, wetMix_ + step);
    else if (wetMix_ > target) wetMix_ = std::max(target, wetMix_ - step);

    // A correlated-safe linear fade avoids the +3 dB bump that an equal-power
    // crossfade can produce when wet and dry are very similar.
    const float dryMix = 1.0f - wetMix_;
    outLeft = dryLeft * dryMix + wetLeft * wetMix_;
    outRight = dryRight * dryMix + wetRight * wetMix_;
}

} // namespace growlforge
