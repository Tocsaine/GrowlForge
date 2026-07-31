#include "ParameterDefinitions.h"

namespace growlforge {

namespace {
constexpr uint32_t kAuto = CLAP_PARAM_IS_AUTOMATABLE;
constexpr uint32_t kToggle = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
constexpr uint32_t kReadOnly = CLAP_PARAM_IS_READONLY;
constexpr uint32_t kBypass = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED | (1u << 4); // CLAP_PARAM_IS_BYPASS
}

const std::array<ParamDef, kParamCount> defs{{
    {Input,"input","Input Trim","Gain",-12,12,0," dB",kAuto},
    {Gate,"gate","Gate","Dynamics",0,10,0,"",kAuto},
    {Tight,"tight","Tight","Enhancers",0,10,0,"",kAuto},
    {Punch,"punch","Punch","Enhancers",0,10,0,"",kAuto},
    {Body,"body","Body","Enhancers",0,10,0,"",kAuto},
    {Mass,"mass","Mass","Enhancers",0,10,0,"",kAuto},
    {Growl,"growl","Growl","Enhancers",0,10,0,"",kAuto},
    {Drive,"drive","Drive","Enhancers",0,10,0,"",kAuto},
    {Grind,"grind","Grind","Enhancers",0,10,0,"",kAuto},
    {Fuzz,"fuzz","Fuzz","Enhancers",0,10,0,"",kAuto},
    {Bite,"bite","Bite","Enhancers",0,10,0,"",kAuto},
    {Presence,"presence","Presence","Enhancers",0,10,0,"",kAuto},
    {Air,"air","Air","Enhancers",0,10,0,"",kAuto},
    {Smooth,"smooth","Smooth","Enhancers",0,10,0,"",kAuto},
    {PreCab,"preCab","Pre-Cab Filter","Enhancers",0,10,0,"",kAuto},
    {ParallelDry,"parallelDry","Parallel Dry","Output",0,100,0," %",kAuto},
    {Output,"output","Output","Output",-12,12,0," dB",kAuto},
    {Ceiling,"ceiling","Ceiling","Output",-12,0,0," dB",kAuto},
    {AutoGain,"autoGain","Auto-Gain","Output",0,1,0,"",kToggle},
    {AutoGainCorrection,"autoGainCorrection","Auto-Gain Correction","Output",-12,12,0," dB",kReadOnly},
    {ApplyAutoGain,"applyAutoGain","Apply Auto-Gain","Output",0,1,0,"",kToggle},
    {Bloom,"bloom","Bloom","Motion",0,10,0,"",kAuto},
    {Sag,"sag","Sag","Motion",0,10,0,"",kAuto},
    {Dynamics,"dynamics","Dynamics","Motion",0,10,0,"",kAuto},
    {Texture,"texture","Texture","Character",0,10,0,"",kAuto},
    {Focus,"focus","Focus","Character",0,10,0,"",kAuto},
    {Attack,"attack","Attack","Motion",0,10,0,"",kAuto},
    {Resonance,"resonance","Resonance","Enhancers",0,10,0,"",kAuto},
    {Compression,"compression","Compression","Dynamics",0,10,0,"",kAuto},
    {HarmonicBias,"harmonicBias","Harmonic Bias","Character",0,10,0,"",kAuto},
    {X2,"x2","x2","Enhancers",0,1,0,"",kToggle},
    {MeterSaturation,"meterSaturation","Saturation Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterBloom,"meterBloom","Bloom Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterCompression,"meterCompression","Compression Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterSag,"meterSag","Sag Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterAttack,"meterAttack","Attack Activity","Indicator",0,100,0," %",kReadOnly},
    {Bypass,"bypass","Bypass","Output",0,1,0,"",kBypass}
}};

} // namespace growlforge
