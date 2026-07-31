#include "StateManager.h"
#include "../plugin/GrowlForgePlugin.h"
#include "../parameters/ParameterDefinitions.h"
#include "../common/Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace growlforge {

namespace {

constexpr uint32_t kStateMagic = 0x47465247;
constexpr uint32_t kStateVersion = 11;
constexpr uint32_t kVersion10ParamCount = 36;

struct StateHeader {
    uint32_t magic = kStateMagic;
    uint32_t version = kStateVersion;
};

bool writeFully(const clap_ostream_t* stream, const void* data, uint64_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint64_t written = 0;
    while (written < size) {
        const int64_t result = stream->write(stream, bytes + written, size - written);
        if (result <= 0) return false;
        written += static_cast<uint64_t>(result);
    }
    return true;
}

bool readFully(const clap_istream_t* stream, void* data, uint64_t size) {
    auto* bytes = static_cast<uint8_t*>(data);
    uint64_t read = 0;
    while (read < size) {
        const int64_t result = stream->read(stream, bytes + read, size - read);
        if (result <= 0) return false;
        read += static_cast<uint64_t>(result);
    }
    return true;
}

bool stateSave(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    if (!stream || !stream->write) return false;
    const StateHeader header;
    if (!writeFully(stream, &header, sizeof(header))) return false;

    const uint32_t count = kParamCount;
    if (!writeFully(stream, &count, sizeof(count))) return false;

    auto* instance = self(plugin);
    std::array<double, kParamCount> values{};
    for (size_t index = 0; index < kParamCount; ++index) {
        values[index] = (isReadOnlyParameter(static_cast<clap_id>(index)) || index == ApplyAutoGain)
            ? 0.0
            : instance->parameters.values[index].load();
    }
    if (!writeFully(stream, values.data(), sizeof(values))) return false;

    std::string presetName = instance->presets.currentName();
    if (!presetName.empty() && presetName.back() == '*') {
        while (!presetName.empty() && (presetName.back() == '*' || presetName.back() == ' ')) presetName.pop_back();
    }
    if (presetName.size() > 1024) presetName.resize(1024);
    const uint32_t nameLength = static_cast<uint32_t>(presetName.size());
    if (!writeFully(stream, &nameLength, sizeof(nameLength))) return false;
    return nameLength == 0 || writeFully(stream, presetName.data(), nameLength);
}

bool stateLoad(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    if (!stream || !stream->read) return false;
    StateHeader header;
    if (!readFully(stream, &header, sizeof(header)) || header.magic != kStateMagic) return false;

    std::array<double, kParamCount> loaded{};
    for (size_t index = 0; index < kParamCount; ++index) loaded[index] = defs[index].def;
    std::string presetName = "Project State";

    if (header.version == 11) {
        uint32_t count = 0;
        if (!readFully(stream, &count, sizeof(count)) || count > 4096) return false;
        const uint32_t toRead = std::min<uint32_t>(count, kParamCount);
        if (toRead > 0 && !readFully(stream, loaded.data(), sizeof(double) * toRead)) return false;
        if (count > toRead) {
            std::array<double, 64> discard{};
            uint32_t remaining = count - toRead;
            while (remaining > 0) {
                const uint32_t chunk = std::min<uint32_t>(remaining, discard.size());
                if (!readFully(stream, discard.data(), sizeof(double) * chunk)) return false;
                remaining -= chunk;
            }
        }
        uint32_t nameLength = 0;
        if (!readFully(stream, &nameLength, sizeof(nameLength)) || nameLength > 1024) return false;
        presetName.resize(nameLength);
        if (nameLength > 0 && !readFully(stream, presetName.data(), nameLength)) return false;
    } else if (header.version == 10) {
        std::array<double, kVersion10ParamCount> old{};
        if (!readFully(stream, old.data(), sizeof(old))) return false;
        for (size_t index = 0; index < old.size(); ++index) loaded[index] = old[index];
    } else if (header.version == 9) {
        std::array<double, 27> old{};
        if (!readFully(stream, old.data(), sizeof(old))) return false;
        for (size_t index = 0; index < old.size(); ++index) loaded[index] = old[index];
    } else if (header.version == 8) {
        std::array<double, 28> old{};
        if (!readFully(stream, old.data(), sizeof(old))) return false;
        for (size_t index = 0; index < 27; ++index) loaded[index] = old[index];
    } else if (header.version == 7) {
        std::array<double, 21> old{};
        if (!readFully(stream, old.data(), sizeof(old))) return false;
        for (size_t index = 0; index < old.size(); ++index) loaded[index] = old[index];
    } else {
        return false;
    }

    auto* instance = self(plugin);
    for (size_t index = 0; index < kParamCount; ++index) {
        const clap_id id = static_cast<clap_id>(index);
        if (isReadOnlyParameter(id) || id == ApplyAutoGain) {
            instance->parameters.values[index] = 0.0;
            continue;
        }
        const double value = clamp(loaded[index], defs[index].min, defs[index].max);
        instance->parameters.values[index] = isToggleParameter(id)
            ? (value >= 0.5 ? 1.0 : 0.0)
            : quantize01(value);
    }

    instance->presets.setCurrentNameFromState(presetName);
    instance->dsp.reset();
    instance->dsp.configure();
    instance->meters.reset();
    return true;
}

} // namespace

const clap_plugin_state_t stateExt{stateSave, stateLoad};

} // namespace growlforge
