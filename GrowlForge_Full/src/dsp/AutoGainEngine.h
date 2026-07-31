#pragma once

#include "Filters.h"
#include <array>
#include <cstdint>

namespace growlforge {

class AutoGainEngine {
public:
    void prepare(double sampleRate);
    void reset();

    // Returns one stereo-linked gain for the current frame.
    float processFrame(float dryLeft, float dryRight,
                       float wetLeft, float wetRight,
                       bool enabled, uint32_t channelCount);

    double correctionDb() const;
    double commitCorrectionDb();

private:
    struct DetectorChannel {
        OnePole dryHighpass;
        OnePole dryLowpass;
        OnePole wetHighpass;
        OnePole wetLowpass;
        void reset();
    };

    float weightedSample(float sample, OnePole& highpass, OnePole& lowpass);

    std::array<DetectorChannel, 2> channels_{};
    double sampleRate_ = 48000.0;
    double dryFast2_ = 1.0e-10;
    double wetFast2_ = 1.0e-10;
    double drySlow2_ = 1.0e-10;
    double wetSlow2_ = 1.0e-10;
    double targetGain_ = 1.0;
    double appliedGain_ = 1.0;
};

} // namespace growlforge
