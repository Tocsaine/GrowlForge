#pragma once

#include <clap/clap.h>
#include <array>
#include <cstdint>

namespace growlforge {

inline constexpr uint32_t kParamCount = 37;

enum ParamId : clap_id {
    Input = 0, Gate, Tight, Punch, Body, Mass, Growl, Drive, Grind, Fuzz,
    Bite, Presence, Air, Smooth, PreCab, ParallelDry, Output, Ceiling, AutoGain,
    AutoGainCorrection, ApplyAutoGain,
    Bloom, Sag, Dynamics, Texture, Focus, Attack,
    Resonance, Compression, HarmonicBias, X2,
    MeterSaturation, MeterBloom, MeterCompression, MeterSag, MeterAttack,
    Bypass
};

struct ParamDef {
    clap_id id;
    const char* key;
    const char* name;
    const char* module;
    double min;
    double max;
    double def;
    const char* unit;
    uint32_t flags;
};

extern const std::array<ParamDef, kParamCount> defs;

inline bool isReadOnlyParameter(clap_id id) {
    return id == AutoGainCorrection || (id >= MeterSaturation && id <= MeterAttack);
}

inline bool isToggleParameter(clap_id id) {
    return id == AutoGain || id == ApplyAutoGain || id == X2 || id == Bypass;
}

inline bool isPresetParameter(clap_id id) {
    return id < kParamCount && !isReadOnlyParameter(id) && id != ApplyAutoGain && id != Bypass;
}

} // namespace growlforge
