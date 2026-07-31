#pragma once

#include <algorithm>
#include <cmath>

namespace growlforge {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr int kOversample = 4;

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double clamp(double x, double a, double b) { return std::max(a, std::min(b, x)); }
inline double gainToDb(double gain) { return 20.0 * std::log10(std::max(gain, 1.0e-9)); }
inline double quantize01(double value) { return std::round(value * 10.0) / 10.0; }
inline double extremeCurve(double normalized, double start, double amount) {
    normalized = clamp(normalized, 0.0, 1.0);
    if (normalized <= start) return normalized;
    const double t = (normalized - start) / (1.0 - start);
    return clamp(normalized + amount * t * t, 0.0, 1.5);
}
inline float zap(float x) { return std::abs(x) < 1e-20f ? 0.0f : x; }

} // namespace growlforge
