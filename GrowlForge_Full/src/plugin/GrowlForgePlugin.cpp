#include "GrowlForgePlugin.h"
#include "PluginFactory.h"
#include "../gui/GrowlForgeGUI.h"
#include "../state/StateManager.h"
#include "../common/Math.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace growlforge {

GrowlForge::GrowlForge(const clap_host_t* hostIn)
    : host(hostIn), parameters(hostIn), dsp(parameters) {
    for (auto& value : guiInputPeak) value = 0.0f;
    for (auto& value : guiOutputPeak) value = 0.0f;
}

GrowlForge* self(const clap_plugin_t* plugin) {
    return static_cast<GrowlForge*>(plugin->plugin_data);
}

namespace {

void handleEvents(GrowlForge* instance, const clap_input_events_t* events) {
    if (!events || !events->size || !events->get) return;
    bool changed = false;
    for (uint32_t i = 0; i < events->size(events); ++i) {
        auto* header = events->get(events, i);
        if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID || header->type != CLAP_EVENT_PARAM_VALUE) continue;
        auto* valueEvent = reinterpret_cast<const clap_event_param_value_t*>(header);
        if (valueEvent->param_id >= kParamCount || valueEvent->param_id == AutoGainCorrection || valueEvent->param_id >= MeterSaturation) continue;

        double value = clamp(valueEvent->value, defs[valueEvent->param_id].min, defs[valueEvent->param_id].max);
        if (valueEvent->param_id == AutoGain || valueEvent->param_id == ApplyAutoGain || valueEvent->param_id == X2) {
            value = value >= 0.5 ? 1.0 : 0.0;
        } else {
            value = quantize01(value);
        }

        if (valueEvent->param_id == ApplyAutoGain && value >= 0.5) {
            instance->dsp.applyCurrentAutoGain();
            changed = true;
            continue;
        }

        if (valueEvent->param_id == AutoGain) {
            const double previous = instance->parameters.values[AutoGain].load();
            instance->parameters.values[AutoGain] = value;

            if (previous != value) {
                // Auto-Gain must measure from a neutral output reference.
                // Enabling it clears any previously committed/manual Output gain.
                if (value >= 0.5) instance->parameters.values[Output] = 0.0;
                instance->dsp.resetAutoGainMeasurement();
            }

            changed = true;
            continue;
        }

        instance->parameters.values[valueEvent->param_id] = value;
        changed = true;
    }
    if (changed) instance->dsp.configure();
}

bool pushGuiGesture(const clap_output_events_t* outputEvents, uint16_t type, clap_id id) {
    if (!outputEvents || !outputEvents->try_push) return false;
    clap_event_param_gesture_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = type;
    event.header.flags = CLAP_EVENT_IS_LIVE;
    event.param_id = id;
    return outputEvents->try_push(outputEvents, &event.header);
}

bool pushGuiValue(const clap_output_events_t* outputEvents, clap_id id, double value) {
    if (!outputEvents || !outputEvents->try_push) return false;
    clap_event_param_value_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.header.flags = CLAP_EVENT_IS_LIVE;
    event.param_id = id;
    event.cookie = nullptr;
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = value;
    return outputEvents->try_push(outputEvents, &event.header);
}

void applyDeferredGuiActions(GrowlForge* instance) {
    auto& parameters = instance->parameters;
    if (parameters.autoGainResetPending.exchange(false)) instance->dsp.resetAutoGainMeasurement();
    if (parameters.applyAutoGainPending.exchange(false)) {
        instance->dsp.applyCurrentAutoGain();
        parameters.guiPendingValue[Output] = parameters.values[Output].load();
        parameters.guiPendingValue[AutoGain] = parameters.values[AutoGain].load();
        parameters.guiPendingFlags[Output].fetch_or(2u, std::memory_order_release);
        parameters.guiPendingFlags[AutoGain].fetch_or(2u, std::memory_order_release);
    }
    if (parameters.configDirty.exchange(false)) instance->dsp.configure();
}

void flushGuiEvents(GrowlForge* instance, const clap_output_events_t* outputEvents) {
    applyDeferredGuiActions(instance);
    if (!outputEvents || !outputEvents->try_push) return;
    auto& parameters = instance->parameters;
    for (clap_id id = 0; id < kParamCount; ++id) {
        uint8_t flags = parameters.guiPendingFlags[id].exchange(0, std::memory_order_acq_rel);
        if (!flags) continue;
        if ((flags & 1u) && !pushGuiGesture(outputEvents, CLAP_EVENT_PARAM_GESTURE_BEGIN, id)) {
            parameters.guiPendingFlags[id].fetch_or(flags, std::memory_order_release);
            continue;
        }
        if ((flags & 2u) && !pushGuiValue(outputEvents, id, parameters.guiPendingValue[id].load())) {
            parameters.guiPendingFlags[id].fetch_or(static_cast<uint8_t>(flags & 6u), std::memory_order_release);
            continue;
        }
        if ((flags & 4u) && !pushGuiGesture(outputEvents, CLAP_EVENT_PARAM_GESTURE_END, id))
            parameters.guiPendingFlags[id].fetch_or(4u, std::memory_order_release);
    }
}

bool plugInit(const clap_plugin_t* plugin) {
    auto* instance = self(plugin);
    if (instance->host && instance->host->get_extension) {
        instance->hostParams = static_cast<const clap_host_params_t*>(
            instance->host->get_extension(instance->host, CLAP_EXT_PARAMS));
        instance->parameters.bindHostParams(instance->hostParams);
    }
    return true;
}

void plugDestroy(const clap_plugin_t* plugin) {
    auto* instance = self(plugin);
    destroyGrowlForgeGui(instance);
    delete instance;
}

bool plugActivate(const clap_plugin_t* plugin, double sampleRate, uint32_t, uint32_t) {
    auto* instance = self(plugin);
    if (sampleRate <= 1000) return false;
    instance->dsp.setSampleRate(sampleRate);
    instance->dsp.reset();
    instance->dsp.configure();
    return true;
}

void plugDeactivate(const clap_plugin_t*) {}
bool plugStart(const clap_plugin_t*) { return true; }
void plugStop(const clap_plugin_t*) {}
void plugReset(const clap_plugin_t* plugin) { self(plugin)->dsp.reset(); }

clap_process_status plugProcess(const clap_plugin_t* plugin, const clap_process_t* process) {
    if (!process) return CLAP_PROCESS_ERROR;
    auto* instance = self(plugin);
    handleEvents(instance, process->in_events);
    flushGuiEvents(instance, process->out_events);
    if (process->audio_inputs_count < 1 || process->audio_outputs_count < 1) return CLAP_PROCESS_CONTINUE;
    auto& input = process->audio_inputs[0];
    auto& output = process->audio_outputs[0];
    const uint32_t channels = std::min({input.channel_count, output.channel_count, 2u});
    const float decay = static_cast<float>(std::exp(
        -static_cast<double>(process->frames_count) / std::max(1.0, 0.34 * instance->dsp.sampleRate())));
    for (uint32_t channel = 0; channel < channels; ++channel) {
        if (!input.data32 || !output.data32 || !input.data32[channel] || !output.data32[channel]) continue;
        float peakIn = 0.0f, peakOut = 0.0f;
        for (uint32_t frame = 0; frame < process->frames_count; ++frame) {
            const float inputSample = input.data32[channel][frame];
            const float outputSample = instance->dsp.processSample(inputSample, static_cast<int>(channel));
            output.data32[channel][frame] = outputSample;
            peakIn = std::max(peakIn, std::abs(inputSample));
            peakOut = std::max(peakOut, std::abs(outputSample));
        }
        instance->guiInputPeak[channel] = std::max(peakIn, instance->guiInputPeak[channel].load() * decay);
        instance->guiOutputPeak[channel] = std::max(peakOut, instance->guiOutputPeak[channel].load() * decay);
    }
    for (uint32_t channel = channels; channel < 2; ++channel) {
        instance->guiInputPeak[channel] = instance->guiInputPeak[channel].load() * decay;
        instance->guiOutputPeak[channel] = instance->guiOutputPeak[channel].load() * decay;
    }
    instance->parameters.values[AutoGainCorrection] = instance->dsp.currentAutoGainDb();
    return CLAP_PROCESS_CONTINUE;
}

uint32_t audioCount(const clap_plugin_t*, bool) { return 1; }

bool audioGet(const clap_plugin_t*, uint32_t index, bool input, clap_audio_port_info_t* info) {
    if (index || !info) return false;
    std::memset(info, 0, sizeof(*info));
    info->id = input ? 0 : 1;
    std::snprintf(info->name, sizeof(info->name), "%s", input ? "Stereo Input" : "Stereo Output");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = input ? 1 : 0;
    return true;
}

uint32_t paramCount(const clap_plugin_t*) { return kParamCount; }

bool paramInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
    if (index >= kParamCount || !info) return false;
    const auto& definition = defs[index];
    std::memset(info, 0, sizeof(*info));
    info->id = definition.id;
    info->flags = definition.flags;
    info->min_value = definition.min;
    info->max_value = definition.max;
    info->default_value = definition.def;
    std::snprintf(info->name, sizeof(info->name), "%s", definition.name);
    std::snprintf(info->module, sizeof(info->module), "%s", definition.module);
    return true;
}

bool paramValue(const clap_plugin_t* plugin, clap_id id, double* value) {
    if (id >= kParamCount || !value) return false;
    if (id == AutoGainCorrection) {
        *value = self(plugin)->dsp.currentAutoGainDb();
        return true;
    }
    *value = self(plugin)->parameters.values[id];
    return true;
}

bool valueText(const clap_plugin_t*, clap_id id, double value, char* display, uint32_t size) {
    if (id >= kParamCount || !display || !size) return false;
    if (id == AutoGain || id == X2) std::snprintf(display, size, "%s", value >= 0.5 ? "On" : "Off");
    else if (id == ApplyAutoGain) std::snprintf(display, size, "%s", value >= 0.5 ? "Apply" : "Ready");
    else std::snprintf(display, size, "%.1f%s", value, defs[id].unit);
    return true;
}

bool textValue(const clap_plugin_t*, clap_id id, const char* text, double* value) {
    if (id >= kParamCount || !text || !value || id == AutoGainCorrection) return false;
    if (id == AutoGain || id == ApplyAutoGain || id == X2) {
        *value = (!std::strcmp(text, "On") || !std::strcmp(text, "on") || !std::strcmp(text, "Apply") ||
                  !std::strcmp(text, "apply") || !std::strcmp(text, "1")) ? 1.0 : 0.0;
        return true;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text) return false;
    *value = quantize01(clamp(parsed, defs[id].min, defs[id].max));
    return true;
}

void paramFlush(const clap_plugin_t* plugin, const clap_input_events_t* inputEvents,
                const clap_output_events_t* outputEvents) {
    auto* instance = self(plugin);
    handleEvents(instance, inputEvents);
    flushGuiEvents(instance, outputEvents);
}

const void* plugExtension(const clap_plugin_t*, const char* id) {
    if (!id) return nullptr;
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audioExt;
    if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &paramsExt;
    if (!std::strcmp(id, CLAP_EXT_STATE)) return &stateExt;
    if (!std::strcmp(id, CLAP_EXT_GUI)) return &guiExt;
    return nullptr;
}

void plugMain(const clap_plugin_t*) {}

const char* features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_DISTORTION,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr
};

const clap_plugin_descriptor_t descriptor{
    CLAP_VERSION,
    "audio.growlforge.effect",
    "GrowlForge",
    "OpenAI / User Project",
    "", "", "",
    "2.1.0-dev",
    "Post-amp guitar character processor with a scalable custom interface, live meters and tactile distortion shaping.",
    features
};

uint32_t factoryCount(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* factoryDesc(const clap_plugin_factory_t*, uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (!id || std::strcmp(id, descriptor.id)) return nullptr;
    auto* instance = new GrowlForge(host);
    instance->plugin = {
        &descriptor,
        instance,
        plugInit,
        plugDestroy,
        plugActivate,
        plugDeactivate,
        plugStart,
        plugStop,
        plugReset,
        plugProcess,
        plugExtension,
        plugMain
    };
    return &instance->plugin;
}

const clap_plugin_factory_t factory{factoryCount, factoryDesc, factoryCreate};

} // namespace

const clap_plugin_audio_ports_t audioExt{audioCount, audioGet};
const clap_plugin_params_t paramsExt{paramCount, paramInfo, paramValue, valueText, textValue, paramFlush};

const clap_plugin_factory_t* pluginFactory() { return &factory; }

} // namespace growlforge
