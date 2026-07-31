#pragma once

#include <clap/clap.h>
#include "../parameters/ParameterStore.h"
#include "../dsp/GrowlForgeDSP.h"
#include <array>
#include <atomic>

namespace growlforge {

struct GrowlForge {
    clap_plugin_t plugin{};
    const clap_host_t* host = nullptr;
    const clap_host_params_t* hostParams = nullptr;
    ParameterStore parameters;
    GrowlForgeDSP dsp;
    std::array<std::atomic<float>, 2> guiInputPeak{};
    std::array<std::atomic<float>, 2> guiOutputPeak{};
    void* guiState = nullptr;

    explicit GrowlForge(const clap_host_t* host);

    void beginGuiGesture(clap_id id) { parameters.beginGuiGesture(id); }
    void endGuiGesture(clap_id id) { parameters.endGuiGesture(id); }
    void setGuiParameter(clap_id id, double value) { parameters.setGuiParameter(id, value); }
};

GrowlForge* self(const clap_plugin_t* plugin);

extern const clap_plugin_audio_ports_t audioExt;
extern const clap_plugin_params_t paramsExt;

} // namespace growlforge
