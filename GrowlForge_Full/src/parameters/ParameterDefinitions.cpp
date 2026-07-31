#include "ParameterDefinitions.h"

namespace growlforge {

namespace {
constexpr uint32_t kAuto = CLAP_PARAM_IS_AUTOMATABLE;
constexpr uint32_t kToggle = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
constexpr uint32_t kReadOnly = CLAP_PARAM_IS_READONLY;
}

const std::array<ParamDef, kParamCount> defs{{
    {Input,"Input Trim","Gain",-12,12,0," dB",kAuto},
    {Gate,"Gate","Dynamics",0,10,0,"",kAuto},
    {Tight,"Tight","Enhancers",0,10,0,"",kAuto},
    {Punch,"Punch","Enhancers",0,10,0,"",kAuto},
    {Body,"Body","Enhancers",0,10,0,"",kAuto},
    {Mass,"Mass","Enhancers",0,10,0,"",kAuto},
    {Growl,"Growl","Enhancers",0,10,0,"",kAuto},
    {Drive,"Drive","Enhancers",0,10,0,"",kAuto},
    {Grind,"Grind","Enhancers",0,10,0,"",kAuto},
    {Fuzz,"Fuzz","Enhancers",0,10,0,"",kAuto},
    {Bite,"Bite","Enhancers",0,10,0,"",kAuto},
    {Presence,"Presence","Enhancers",0,10,0,"",kAuto},
    {Air,"Air","Enhancers",0,10,0,"",kAuto},
    {Smooth,"Smooth","Enhancers",0,10,0,"",kAuto},
    {PreCab,"Pre-Cab Filter","Enhancers",0,10,0,"",kAuto},
    {ParallelDry,"Parallel Dry","Output",0,100,0," %",kAuto},
    {Output,"Output","Output",-12,12,0," dB",kAuto},
    {Ceiling,"Ceiling","Output",-12,0,0," dB",kAuto},
    {AutoGain,"Auto-Gain","Output",0,1,0,"",kToggle},
    {AutoGainCorrection,"Auto-Gain Correction","Output",-12,12,0," dB",kReadOnly},
    {ApplyAutoGain,"Apply Auto-Gain","Output",0,1,0,"",kToggle},
    {Bloom,"Bloom","Motion",0,10,0,"",kAuto},
    {Sag,"Sag","Motion",0,10,0,"",kAuto},
    {Dynamics,"Dynamics","Motion",0,10,0,"",kAuto},
    {Texture,"Texture","Character",0,10,0,"",kAuto},
    {Focus,"Focus","Character",0,10,0,"",kAuto},
    {Attack,"Attack","Motion",0,10,0,"",kAuto},
    {Resonance,"Resonance","Enhancers",0,10,0,"",kAuto},
    {Compression,"Compression","Dynamics",0,10,0,"",kAuto},
    {HarmonicBias,"Harmonic Bias","Character",0,10,0,"",kAuto},
    {X2,"x2","Enhancers",0,1,0,"",kToggle},
    {MeterSaturation,"Saturation Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterBloom,"Bloom Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterCompression,"Compression Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterSag,"Sag Activity","Indicator",0,100,0," %",kReadOnly},
    {MeterAttack,"Attack Activity","Indicator",0,100,0," %",kReadOnly}
}};

} // namespace growlforge
