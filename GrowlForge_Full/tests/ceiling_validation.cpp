#include <clap/clap.h>
#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
const void* hostExtension(const clap_host_t*, const char*) { return nullptr; }
void hostNoop(const clap_host_t*) {}
struct InputEvents {
    clap_input_events_t iface{};
    std::vector<clap_event_param_value_t> events;
    InputEvents() {
        iface.ctx = this;
        iface.size = [](const clap_input_events_t* list) { return static_cast<uint32_t>(static_cast<const InputEvents*>(list->ctx)->events.size()); };
        iface.get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            const auto* self = static_cast<const InputEvents*>(list->ctx);
            return index < self->events.size() ? &self->events[index].header : nullptr;
        };
    }
    void add(clap_id id, double value) {
        clap_event_param_value_t event{};
        event.header.size = sizeof(event);
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.param_id = id;
        event.note_id = event.port_index = event.channel = event.key = -1;
        event.value = value;
        events.push_back(event);
    }
};
struct OutputEvents {
    clap_output_events_t iface{};
    OutputEvents() { iface.ctx = this; iface.try_push = [](const clap_output_events_t*, const clap_event_header_t*) { return true; }; }
};
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) return 3;
    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
    if (!entry || !entry->init(argv[1])) return 4;
    auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "CeilingTest";
    host.vendor = "GrowlForge";
    host.url = "";
    host.version = "1";
    host.get_extension = hostExtension;
    host.request_restart = hostNoop;
    host.request_process = hostNoop;
    host.request_callback = hostNoop;
    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, descriptor->id);
    if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, 48000.0, 1, 128) || !plugin->start_processing(plugin)) return 5;

    constexpr uint32_t frames = 128;
    std::array<std::array<float, frames>, 2> input{}, output{};
    float* inputPointers[2]{input[0].data(), input[1].data()};
    float* outputPointers[2]{output[0].data(), output[1].data()};
    clap_audio_buffer_t inputBuffer{inputPointers, nullptr, 2, 0, 0};
    clap_audio_buffer_t outputBuffer{outputPointers, nullptr, 2, 0, 0};
    OutputEvents outputEvents;
    float peak = 0.0f;

    for (uint32_t block = 0; block < 20; ++block) {
        for (uint32_t frame = 0; frame < frames; ++frame) {
            const float sample = 2.0f * std::sin(2.0 * 3.14159265358979323846 * 997.0 * (block * frames + frame) / 48000.0);
            input[0][frame] = input[1][frame] = sample;
        }
        InputEvents events;
        if (block == 0) {
            events.add(7, 0.0);   // Drive
            events.add(16, 0.0);  // Output
            events.add(17, -12.0);// Ceiling
            events.add(18, 0.0);  // Auto-Gain
            events.add(36, 0.0);  // Bypass
        }
        clap_process_t process{};
        process.frames_count = frames;
        process.audio_inputs = &inputBuffer;
        process.audio_outputs = &outputBuffer;
        process.audio_inputs_count = process.audio_outputs_count = 1;
        process.in_events = &events.iface;
        process.out_events = &outputEvents.iface;
        if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR) return 6;
        for (float sample : output[0]) peak = std::max(peak, std::abs(sample));
    }

    const float expected = static_cast<float>(std::pow(10.0, -12.0 / 20.0));
    std::printf("ceiling peak: %.9f (%.4f dBFS)\n", peak, 20.0 * std::log10(peak));
    const bool ok = peak <= expected + 1.0e-6f && peak >= expected - 1.0e-5f;

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(library);
    return ok ? 0 : 7;
}
