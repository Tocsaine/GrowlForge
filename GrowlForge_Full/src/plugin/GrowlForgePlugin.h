#pragma once

#include <clap/clap.h>
#include "../parameters/ParameterStore.h"
#include "../dsp/GrowlForgeDSP.h"
#include "../state/PresetManager.h"
#include <array>
#include <atomic>

namespace growlforge {

struct MeteringData {
    std::array<std::atomic<float>, 2> inputPeak{};
    std::array<std::atomic<float>, 2> inputRms{};
    std::array<std::atomic<float>, 2> outputPeak{};
    std::array<std::atomic<float>, 2> outputRms{};
    std::atomic<float> wetPreCeilingPeak{0.0f};
    std::atomic<float> gateReduction{0.0f};
    std::atomic<bool> internalClip{false};

    void reset() {
        for (auto& value : inputPeak) value = 0.0f;
        for (auto& value : inputRms) value = 0.0f;
        for (auto& value : outputPeak) value = 0.0f;
        for (auto& value : outputRms) value = 0.0f;
        wetPreCeilingPeak = 0.0f;
        gateReduction = 0.0f;
        internalClip = false;
    }
};

struct GrowlForge {
    clap_plugin_t plugin{};
    const clap_host_t* host = nullptr;
    const clap_host_params_t* hostParams = nullptr;
    ParameterStore parameters;
    GrowlForgeDSP dsp;
    PresetManager presets;
    MeteringData meters;
    void* guiState = nullptr;

    explicit GrowlForge(const clap_host_t* host);

    void beginGuiGesture(clap_id id) { parameters.beginGuiGesture(id); }
    void endGuiGesture(clap_id id) { parameters.endGuiGesture(id); }
    void setGuiParameter(clap_id id, double value) {
        parameters.setGuiParameter(id, value);
        if (isPresetParameter(id)) presets.markDirty();
    }
};

GrowlForge* self(const clap_plugin_t* plugin);

extern const clap_plugin_audio_ports_t audioExt;
extern const clap_plugin_params_t paramsExt;

} // namespace growlforge
