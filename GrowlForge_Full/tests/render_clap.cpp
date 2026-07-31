#include <clap/clap.h>
#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
void hostNoop(const clap_host_t*) {}

struct InputEvents {
    clap_input_events_t iface{};
    std::vector<clap_event_param_value_t> events;

    InputEvents() {
        iface.ctx = this;
        iface.size = [](const clap_input_events_t* list) -> uint32_t {
            return static_cast<uint32_t>(static_cast<const InputEvents*>(list->ctx)->events.size());
        };
        iface.get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            const auto* self = static_cast<const InputEvents*>(list->ctx);
            return index < self->events.size() ? &self->events[index].header : nullptr;
        };
    }

    void add(clap_id id, double value) {
        clap_event_param_value_t event{};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.param_id = id;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = value;
        events.push_back(event);
    }
};

struct OutputEvents {
    clap_output_events_t iface{};
    OutputEvents() {
        iface.ctx = this;
        iface.try_push = [](const clap_output_events_t*, const clap_event_header_t*) { return true; };
    }
};

std::vector<std::pair<clap_id, double>> parametersForScenario(int scenario) {
    switch (scenario) {
        case 0: return {};
        case 1: return {{7, 5.0}};
        case 2: return {{7, 10.0}, {6, 4.0}, {8, 5.0}, {9, 2.0}, {29, 3.0}};
        case 3: return {{1, 5.0}, {2, 6.0}, {3, 5.0}, {4, 4.0}, {5, 6.0}, {10, 3.0}, {11, 4.0}, {12, 2.0}, {13, 3.0}};
        case 4: return {{21, 4.0}, {22, 5.0}, {23, 3.0}, {24, 4.0}, {25, 3.0}, {26, 6.0}, {27, 5.0}, {28, 7.0}};
        case 5: return {{0, 3.0}, {7, 7.0}, {9, 4.0}, {15, 20.0}, {16, -2.0}, {17, -3.0}, {18, 1.0}};
        case 6: return {{2, 5.0}, {5, 6.0}, {6, 7.0}, {8, 5.0}, {9, 5.0}, {10, 4.0}, {11, 6.0}, {12, 3.0}, {13, 2.0}, {21, 4.0}, {22, 3.0}, {23, 5.0}, {24, 6.0}, {25, 4.0}, {26, 7.0}, {27, 5.0}, {28, 6.0}, {29, 4.0}, {30, 1.0}};
        case 7: return {{7, 10.0}, {30, 1.0}};
        default: return {};
    }
}

float testSignal(uint64_t sample, int channel, double sampleRate) {
    const double t = static_cast<double>(sample) / sampleRate;
    const double base = 0.19 * std::sin(2.0 * 3.141592653589793 * (channel ? 113.0 : 97.0) * t);
    const double harmonic = 0.08 * std::sin(2.0 * 3.141592653589793 * (channel ? 677.0 : 523.0) * t + 0.3);
    const double transient = (sample % 4096 < 48) ? (0.62 * std::exp(-static_cast<double>(sample % 4096) / 10.0)) : 0.0;
    uint32_t x = static_cast<uint32_t>((sample + 1) * 1664525u + 1013904223u + channel * 7919u);
    const double noise = (static_cast<int32_t>(x) / 2147483648.0) * 0.012;
    return static_cast<float>(base + harmonic + transient + noise);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s plugin.clap scenario output.raw\n", argv[0]);
        return 2;
    }
    const std::string pluginPath = argv[1];
    const int scenario = std::atoi(argv[2]);
    const std::string outputPath = argv[3];

    void* library = dlopen(pluginPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return 3; }
    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
    if (!entry || !entry->init(pluginPath.c_str())) return 4;
    auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) == 0) return 5;
    const auto* descriptor = factory->get_plugin_descriptor(factory, 0);

    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "RegressionHost";
    host.vendor = "GrowlForge";
    host.url = "";
    host.version = "1";
    host.get_extension = hostGetExtension;
    host.request_restart = hostNoop;
    host.request_process = hostNoop;
    host.request_callback = hostNoop;

    const clap_plugin_t* plugin = factory->create_plugin(factory, &host, descriptor->id);
    if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, 48000.0, 1, 128) || !plugin->start_processing(plugin)) return 6;

    constexpr uint32_t blockSize = 128;
    constexpr uint64_t totalSamples = 48000 * 3;
    std::array<std::array<float, blockSize>, 2> input{};
    std::array<std::array<float, blockSize>, 2> output{};
    float* inputPtrs[2]{input[0].data(), input[1].data()};
    float* outputPtrs[2]{output[0].data(), output[1].data()};
    clap_audio_buffer_t inputBuffer{inputPtrs, nullptr, 2, 0, 0};
    clap_audio_buffer_t outputBuffer{outputPtrs, nullptr, 2, 0, 0};
    OutputEvents outputEvents;
    std::ofstream file(outputPath, std::ios::binary);

    bool first = true;
    for (uint64_t offset = 0; offset < totalSamples; offset += blockSize) {
        const uint32_t frames = static_cast<uint32_t>(std::min<uint64_t>(blockSize, totalSamples - offset));
        for (uint32_t channel = 0; channel < 2; ++channel) {
            for (uint32_t frame = 0; frame < frames; ++frame) input[channel][frame] = testSignal(offset + frame, static_cast<int>(channel), 48000.0);
        }
        InputEvents inputEvents;
        if (first) {
            for (const auto& [id, value] : parametersForScenario(scenario)) inputEvents.add(id, value);
            first = false;
        }
        clap_process_t process{};
        process.steady_time = static_cast<int64_t>(offset);
        process.frames_count = frames;
        process.audio_inputs = &inputBuffer;
        process.audio_outputs = &outputBuffer;
        process.audio_inputs_count = 1;
        process.audio_outputs_count = 1;
        process.in_events = &inputEvents.iface;
        process.out_events = &outputEvents.iface;
        if (plugin->process(plugin, &process) == CLAP_PROCESS_ERROR) return 7;
        for (uint32_t frame = 0; frame < frames; ++frame) {
            file.write(reinterpret_cast<const char*>(&output[0][frame]), sizeof(float));
            file.write(reinterpret_cast<const char*>(&output[1][frame]), sizeof(float));
        }
    }

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(library);
    return file.good() ? 0 : 8;
}
