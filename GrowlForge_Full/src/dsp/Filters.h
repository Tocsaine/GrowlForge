#pragma once

#include "../common/Math.h"

namespace growlforge {

struct OnePole {
    float z = 0;
    float a = 0;

    void setLowpass(double hz, double sr) {
        hz = clamp(hz, 10.0, sr * 0.45);
        a = static_cast<float>(std::exp(-2.0 * kPi * hz / sr));
    }
    void reset() { z = 0; }
    float lp(float x) { z = (1 - a) * x + a * z; return zap(z); }
    float hp(float x) { return x - lp(x); }
};

// Second-order Butterworth high-pass used only by Drive. Asymmetric clipping
// can legitimately create an average offset from an AC input; this removes the
// resulting DC/subsonic energy without touching the audible guitar range.
struct Highpass2 {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;

    void setHighpass(double hz, double sr) {
        hz = clamp(hz, 5.0, sr * 0.45);
        constexpr double q = 0.70710678118654752440;
        const double w0 = 2.0 * kPi * hz / sr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * q), a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) * 0.5) / a0;
        b1 = (-(1.0 + cw)) / a0;
        b2 = b0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }
    void reset() { z1 = z2 = 0.0; }
    float process(float x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::abs(z1) < 1.0e-24) z1 = 0.0;
        if (std::abs(z2) < 1.0e-24) z2 = 0.0;
        return zap(static_cast<float>(y));
    }
};

} // namespace growlforge
