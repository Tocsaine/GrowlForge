#pragma once

#include <cstdint>

namespace growlforge {

class GateEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void processFrame(float& left, float& right, uint32_t channelCount, double amount);
    float reductionPercent() const { return (1.0f - gain_) * 100.0f; }

private:
    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;
    float gain_ = 1.0f;
    bool open_ = true;
    uint32_t holdSamples_ = 0;
};

} // namespace growlforge
