#include "GrowlForgeDSP.h"
#include "../common/Math.h"
#include <algorithm>
#include <cmath>

namespace growlforge {

void ChannelDSP::reset() {
    tightHP.reset(); low110.reset(); low180.reset(); low650.reset(); low1600.reset();
    presenceLP.reset(); airLP.reset(); fuzzLow.reset(); driveSubsonic.reset();
    for (auto& filter : antiAlias) filter.reset();
    for (auto& filter : postLP) filter.reset();
    previousInput = 0.0f;
    fastEnv = slowEnv = sagEnv = attackMemory = attackEnv = compEnv = 0.0f;
    driveFastEnv = driveSlowEnv = 0.0f;
    meterSat = meterBloom = meterComp = meterSag = meterAttack = 0.0f;
}

GrowlForgeDSP::GrowlForgeDSP(ParameterStore& parameters) : parameters_(parameters) {}

void GrowlForgeDSP::setSampleRate(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    gate_.prepare(sampleRate_);
    autoGain_.prepare(sampleRate_);
    bypass_.prepare(sampleRate_);
}

void GrowlForgeDSP::reset() {
    for (auto& channel : channels_) channel.reset();
    gate_.reset();
    autoGain_.reset();
    bypass_.reset(parameters_.values[Bypass].load() >= 0.5);
}

bool GrowlForgeDSP::enhancersZero() const {
    for (clap_id id = Tight; id <= PreCab; ++id)
        if (parameters_.values[id].load() > 1.0e-9) return false;
    return true;
}

bool GrowlForgeDSP::additionsZero() const {
    for (clap_id id = Bloom; id <= HarmonicBias; ++id)
        if (parameters_.values[id].load() > 1.0e-9) return false;
    return true;
}

bool GrowlForgeDSP::x2Enabled() const { return parameters_.values[X2].load() >= 0.5; }
double GrowlForgeDSP::color(double value) const { return x2Enabled() ? 2.0 * value : value; }

void GrowlForgeDSP::configure() {
    const double tight = color(parameters_.values[Tight].load() / 10.0);
    const double smooth = color(parameters_.values[Smooth].load() / 10.0);
    const double preCab = parameters_.values[PreCab].load() / 10.0;
    const double oversampledRate = sampleRate_ * kOversample;
    const double highpassHz = 45.0 + 115.0 * tight;
    const double antiAliasCutoff = clamp(sampleRate_ * (0.46 - 0.23 * smooth),
                                         5200.0, std::min(20500.0, sampleRate_ * 0.46));
    const double openCutoff = std::min(21000.0, sampleRate_ * 0.46);
    const double closedCutoff = 2600.0;
    const double postCutoff = openCutoff * std::pow(closedCutoff / openCutoff, preCab);
    for (auto& channel : channels_) {
        channel.tightHP.setLowpass(highpassHz, sampleRate_);
        channel.low110.setLowpass(110.0, sampleRate_);
        channel.low180.setLowpass(180.0, sampleRate_);
        channel.low650.setLowpass(650.0, sampleRate_);
        channel.low1600.setLowpass(1600.0, sampleRate_);
        channel.presenceLP.setLowpass(2600.0, sampleRate_);
        channel.airLP.setLowpass(6500.0, sampleRate_);
        channel.driveSubsonic.setHighpass(20.0, sampleRate_);
        for (auto& filter : channel.antiAlias) filter.setLowpass(antiAliasCutoff, oversampledRate);
        channel.fuzzLow.setLowpass(260.0, oversampledRate);
        for (auto& filter : channel.postLP) filter.setLowpass(postCutoff, sampleRate_);
    }
}

float GrowlForgeDSP::processCoreSample(float input, int channelIndex) {
    auto& channel = channels_[channelIndex];
    const double tight = color(parameters_.values[Tight].load() / 10.0);
    const double punch = color(parameters_.values[Punch].load() / 10.0);
    const double body = color(parameters_.values[Body].load() / 10.0);
    const double mass = color(parameters_.values[Mass].load() / 10.0);
    const double growl = color(parameters_.values[Growl].load() / 10.0);
    const double bite = color(parameters_.values[Bite].load() / 10.0);
    const double presence = color(parameters_.values[Presence].load() / 10.0);
    const double air = color(parameters_.values[Air].load() / 10.0);
    const double smooth = color(parameters_.values[Smooth].load() / 10.0);
    const double preCab = parameters_.values[PreCab].load() / 10.0;

    if (enhancersZero() && additionsZero()) return input;

    float x = input;
    if (tight > 0.0) {
        const float highpassed = channel.tightHP.hp(x);
        x = x * static_cast<float>(1.0 - tight) + highpassed * static_cast<float>(tight);
    }

    const float low = channel.low180.lp(x);
    const float low650 = channel.low650.lp(x);
    const float low1600 = channel.low1600.lp(x);
    const float lowMid = low650 - low;
    const float growlBand = low1600 - low650;
    const float high = x - low1600;
    const float shaped =
        low * static_cast<float>(1.0 + 0.95 * mass) +
        lowMid * static_cast<float>(0.58 * (1.0 + 1.25 * punch) + 0.42 * (1.0 + 1.18 * body)) +
        growlBand * static_cast<float>(1.0 + 1.65 * growl) +
        high * static_cast<float>(1.0 + 0.70 * bite);

    float wet = nonlinear(shaped, low, growlBand, high, channel);
    const float presenceBand = wet - channel.presenceLP.lp(wet);
    const float airBand = wet - channel.airLP.lp(wet);
    wet += presenceBand * static_cast<float>(0.95 * presence + 0.72 * bite);
    wet += airBand * static_cast<float>(0.78 * air);

    if (smooth > 0.0) {
        wet -= airBand * static_cast<float>(0.88 * smooth);
        wet -= presenceBand * static_cast<float>(0.24 * smooth);
    }
    if (preCab > 0.0)
        for (auto& filter : channel.postLP) wet = filter.lp(wet);

    return applyNewEffects(x, wet, low, growlBand, high, channel);
}

float GrowlForgeDSP::applyCeiling(float sample, double ceilingDb) const {
    if (ceilingDb >= 0.0) return sample;

    // Ceiling is an exact output cap. A narrow cubic knee keeps the transition
    // less abrupt, while the final clamp guarantees that the requested dBFS
    // value can never be exceeded in the active wet path.
    const float ceilingGain = static_cast<float>(dbToGain(ceilingDb));
    const float magnitude = std::abs(sample);
    const float kneeStart = ceilingGain * 0.90f;

    if (magnitude <= kneeStart) return sample;
    if (magnitude >= ceilingGain) return std::copysign(ceilingGain, sample);

    const float t = (magnitude - kneeStart) / std::max(ceilingGain - kneeStart, 1.0e-9f);
    const float shaped = t + t * t - t * t * t; // f(0)=0, f'(0)=1, f(1)=1, f'(1)=0
    const float limited = kneeStart + (ceilingGain - kneeStart) * shaped;
    return std::copysign(std::min(limited, ceilingGain), sample);
}

void GrowlForgeDSP::publishActivityMeters() {
    parameters_.values[MeterSaturation] = clamp(0.5 * (channels_[0].meterSat + channels_[1].meterSat), 0.0, 100.0);
    parameters_.values[MeterBloom] = clamp(0.5 * (channels_[0].meterBloom + channels_[1].meterBloom), 0.0, 100.0);
    parameters_.values[MeterCompression] = clamp(0.5 * (channels_[0].meterComp + channels_[1].meterComp), 0.0, 100.0);
    parameters_.values[MeterSag] = clamp(0.5 * (channels_[0].meterSag + channels_[1].meterSag), 0.0, 100.0);
    parameters_.values[MeterAttack] = clamp(0.5 * (channels_[0].meterAttack + channels_[1].meterAttack), 0.0, 100.0);
}

FrameResult GrowlForgeDSP::processFrame(float left, float right, uint32_t channelCount) {
    channelCount = std::clamp<uint32_t>(channelCount, 1u, 2u);
    const float rawLeft = left;
    const float rawRight = channelCount > 1 ? right : left;
    const float inputGain = static_cast<float>(dbToGain(parameters_.values[Input].load()));
    left = rawLeft * inputGain;
    right = rawRight * inputGain;

    const double gateAmount = parameters_.values[Gate].load() / 10.0;
    gate_.processFrame(left, right, channelCount, gateAmount);

    float wetLeft = processCoreSample(left, 0);
    float wetRight = channelCount > 1 ? processCoreSample(right, 1) : wetLeft;

    const bool autoGainEnabled = parameters_.values[AutoGain].load() >= 0.5;
    const float autoGain = autoGain_.processFrame(left, right, wetLeft, wetRight, autoGainEnabled, channelCount);
    wetLeft *= autoGain;
    wetRight *= autoGain;

    const float parallel = static_cast<float>(parameters_.values[ParallelDry].load() / 100.0);
    wetLeft = wetLeft * (1.0f - parallel) + left * parallel;
    wetRight = wetRight * (1.0f - parallel) + right * parallel;

    const float outputGain = static_cast<float>(dbToGain(parameters_.values[Output].load()));
    wetLeft *= outputGain;
    wetRight *= outputGain;

    FrameResult result;
    result.wetPreCeilingLeft = wetLeft;
    result.wetPreCeilingRight = wetRight;
    const double ceiling = parameters_.values[Ceiling].load();
    wetLeft = applyCeiling(wetLeft, ceiling);
    wetRight = applyCeiling(wetRight, ceiling);

    bypass_.processFrame(rawLeft, rawRight, wetLeft, wetRight,
                         parameters_.values[Bypass].load() >= 0.5,
                         result.left, result.right);
    result.left = static_cast<float>(clamp(result.left, -4.0, 4.0));
    result.right = static_cast<float>(clamp(result.right, -4.0, 4.0));
    result.gateReductionPercent = gate_.reductionPercent();
    publishActivityMeters();
    return result;
}

double GrowlForgeDSP::currentAutoGainDb() const {
    return autoGain_.correctionDb();
}

void GrowlForgeDSP::resetAutoGainMeasurement() {
    autoGain_.reset();
}

void GrowlForgeDSP::applyCurrentAutoGain() {
    const double correction = quantize01(autoGain_.commitCorrectionDb());
    const double currentOutput = parameters_.values[Output].load();
    parameters_.values[Output] = quantize01(clamp(currentOutput + correction, defs[Output].min, defs[Output].max));
    parameters_.values[AutoGain] = 0.0;
    parameters_.values[ApplyAutoGain] = 0.0;
}

} // namespace growlforge
