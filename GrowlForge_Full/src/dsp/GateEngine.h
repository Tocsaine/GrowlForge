#pragma once

namespace growlforge {

class GateEngine {
public:
    static float process(float input, double amount, float& envelope, double sampleRate);
};

} // namespace growlforge
