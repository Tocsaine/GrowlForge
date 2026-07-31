#pragma once

#include "Filters.h"
#include "GateEngine.h"
#include "AutoGainEngine.h"
#include "BypassController.h"
#include "../parameters/ParameterStore.h"
#include <array>
#include <cstdint>

namespace growlforge {

struct ChannelDSP {
    OnePole tightHP, low110, low180, low650, low1600, presenceLP, airLP, fuzzLow;
    Highpass2 driveSubsonic;
    std::array<OnePole, 4> antiAlias;
    std::array<OnePole, 2> postLP;
    float previousInput = 0;
    float fastEnv = 0, slowEnv = 0, sagEnv = 0, attackMemory = 0, attackEnv = 0, compEnv = 0;
    float driveFastEnv = 0, driveSlowEnv = 0;
    float meterSat = 0, meterBloom = 0, meterComp = 0, meterSag = 0, meterAttack = 0;

    void reset();
};

struct FrameResult {
    float left = 0.0f;
    float right = 0.0f;
    float wetPreCeilingLeft = 0.0f;
    float wetPreCeilingRight = 0.0f;
    float gateReductionPercent = 0.0f;
};

class GrowlForgeDSP {
public:
    explicit GrowlForgeDSP(ParameterStore& parameters);

    void setSampleRate(double sampleRate);
    double sampleRate() const { return sampleRate_; }
    void reset();
    void configure();

    FrameResult processFrame(float left, float right, uint32_t channelCount);
    double currentAutoGainDb() const;
    void resetAutoGainMeasurement();
    void applyCurrentAutoGain();

private:
    bool enhancersZero() const;
    bool additionsZero() const;
    bool x2Enabled() const;
    double color(double value) const;
    float processCoreSample(float input, int channelIndex);
    float nonlinear(float x, float low, float growlBand, float high, ChannelDSP& channel);
    float applyNewEffects(float dry, float wet, float low, float growlBand, float high, ChannelDSP& channel);
    float applyCeiling(float sample, double ceilingDb) const;
    void publishActivityMeters();

    ParameterStore& parameters_;
    std::array<ChannelDSP, 2> channels_{};
    GateEngine gate_;
    AutoGainEngine autoGain_;
    BypassController bypass_;
    double sampleRate_ = 48000.0;
};

} // namespace growlforge
