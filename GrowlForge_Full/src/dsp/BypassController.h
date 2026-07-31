#pragma once

namespace growlforge {

class BypassController {
public:
    void prepare(double sampleRate);
    void reset(bool bypassed);
    void processFrame(float dryLeft, float dryRight,
                      float wetLeft, float wetRight,
                      bool bypassed, float& outLeft, float& outRight);
    float wetMix() const { return wetMix_; }

private:
    double sampleRate_ = 48000.0;
    float wetMix_ = 1.0f;
};

} // namespace growlforge
