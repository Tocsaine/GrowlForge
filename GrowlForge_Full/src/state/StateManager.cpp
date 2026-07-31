#include "StateManager.h"
#include "../plugin/GrowlForgePlugin.h"
#include "../parameters/ParameterDefinitions.h"
#include "../common/Math.h"
#include <array>
#include <cstdint>

namespace growlforge {

namespace {

struct StateHeader {
    uint32_t magic = 0x47465247;
    uint32_t version = 10;
};

struct StateBlob {
    uint32_t magic = 0x47465247;
    uint32_t version = 10;
    double values[kParamCount]{};
};

bool stateSave(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    if (!stream || !stream->write) return false;
    StateBlob blob;
    auto* instance = self(plugin);
    for (size_t index = 0; index < kParamCount; ++index) {
        blob.values[index] = (index >= MeterSaturation) ? 0.0 : instance->parameters.values[index].load();
    }
    return stream->write(stream, &blob, sizeof(blob)) == static_cast<int64_t>(sizeof(blob));
}

bool stateLoad(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    if (!stream || !stream->read) return false;
    StateHeader header;
    if (stream->read(stream, &header, sizeof(header)) != static_cast<int64_t>(sizeof(header)) ||
        header.magic != 0x47465247) return false;

    std::array<double, kParamCount> loaded{};
    for (size_t index = 0; index < kParamCount; ++index) loaded[index] = defs[index].def;

    if (header.version == 10) {
        if (stream->read(stream, loaded.data(), sizeof(double) * kParamCount) !=
            static_cast<int64_t>(sizeof(double) * kParamCount)) return false;
    } else if (header.version == 9) {
        std::array<double, 27> old{};
        if (stream->read(stream, old.data(), sizeof(old)) != static_cast<int64_t>(sizeof(old))) return false;
        for (size_t index = 0; index < old.size(); ++index) loaded[index] = old[index];
    } else if (header.version == 8) {
        std::array<double, 28> old{};
        if (stream->read(stream, old.data(), sizeof(old)) != static_cast<int64_t>(sizeof(old))) return false;
        for (size_t index = 0; index < 27; ++index) loaded[index] = old[index];
    } else if (header.version == 7) {
        std::array<double, 21> old{};
        if (stream->read(stream, old.data(), sizeof(old)) != static_cast<int64_t>(sizeof(old))) return false;
        for (size_t index = 0; index < old.size(); ++index) loaded[index] = old[index];
    } else {
        return false;
    }

    auto* instance = self(plugin);
    for (size_t index = 0; index < kParamCount; ++index) {
        if (index == AutoGainCorrection || index == ApplyAutoGain || index >= MeterSaturation) {
            instance->parameters.values[index] = 0.0;
            continue;
        }
        const double value = clamp(loaded[index], defs[index].min, defs[index].max);
        instance->parameters.values[index] =
            (index == AutoGain || index == X2) ? (value >= 0.5 ? 1.0 : 0.0) : quantize01(value);
    }

    instance->dsp.reset();
    instance->dsp.configure();
    return true;
}

} // namespace

const clap_plugin_state_t stateExt{stateSave, stateLoad};

} // namespace growlforge
