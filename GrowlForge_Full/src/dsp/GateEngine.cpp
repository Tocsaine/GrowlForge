#include "GateEngine.h"
#include "../common/Math.h"
#include <cmath>

namespace growlforge {

float GateEngine::process(float input, double amount, float& envelope, double sampleRate) {
    if (amount <= 0.0) return input;
    const double threshold = dbToGain(-90.0 + 58.0 * amount);
    const float absoluteInput = std::abs(input);
    const float attack = static_cast<float>(std::exp(-1.0 / (0.0012 * sampleRate)));
    const float release = static_cast<float>(std::exp(-1.0 / (0.070 * sampleRate)));
    envelope = absoluteInput > envelope
        ? attack * envelope + (1.0f - attack) * absoluteInput
        : release * envelope + (1.0f - release) * absoluteInput;
    input *= static_cast<float>(clamp((envelope - threshold * 0.30) / (threshold * 0.70), 0.0, 1.0));
    return input;
}

} // namespace growlforge
