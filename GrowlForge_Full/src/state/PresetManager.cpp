#include "PresetManager.h"
#include "../common/Math.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace growlforge {

namespace {

void set(std::array<double, kParamCount>& values, clap_id id, double value) {
    values[id] = value;
}

Preset factoryPreset(const char* name, const char* description,
                     const std::initializer_list<std::pair<clap_id, double>>& changes) {
    Preset preset;
    preset.name = name;
    preset.author = "GrowlForge";
    preset.description = description;
    preset.factory = true;
    for (size_t i = 0; i < kParamCount; ++i) preset.values[i] = defs[i].def;
    for (const auto& [id, value] : changes) set(preset.values, id, value);
    return preset;
}

std::string parseStringField(const std::string& text, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t keyPosition = text.find(token);
    if (keyPosition == std::string::npos) return {};
    const size_t colon = text.find(':', keyPosition + token.size());
    if (colon == std::string::npos) return {};
    const size_t quote = text.find('"', colon + 1);
    if (quote == std::string::npos) return {};
    std::string result;
    bool escape = false;
    for (size_t i = quote + 1; i < text.size(); ++i) {
        const char c = text[i];
        if (escape) {
            if (c == 'n') result.push_back('\n');
            else if (c == 'r') result.push_back('\r');
            else if (c == 't') result.push_back('\t');
            else result.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            return result;
        } else {
            result.push_back(c);
        }
    }
    return {};
}

bool parseNumberField(const std::string& text, const std::string& key, double& value) {
    const std::string token = "\"" + key + "\"";
    const size_t keyPosition = text.find(token);
    if (keyPosition == std::string::npos) return false;
    const size_t colon = text.find(':', keyPosition + token.size());
    if (colon == std::string::npos) return false;
    const char* begin = text.c_str() + colon + 1;
    char* end = nullptr;
    const double parsed = std::strtod(begin, &end);
    if (end == begin) return false;
    value = parsed;
    return true;
}

} // namespace

PresetManager::PresetManager(ParameterStore& parameters) : parameters_(parameters) {
    refresh();
}

std::array<double, kParamCount> PresetManager::defaultValues() {
    std::array<double, kParamCount> result{};
    for (size_t i = 0; i < kParamCount; ++i) result[i] = defs[i].def;
    return result;
}

std::vector<Preset> PresetManager::makeFactoryPresets() {
    return {
        factoryPreset("Init", "Neutral starting point.", {}),
        factoryPreset("Controlled Fuzz", "Tactile fuzz with preserved pick response.",
                      {{Drive,3.8},{Fuzz,6.2},{Body,2.4},{Bite,1.8},{Smooth,2.0},{Attack,1.4},{Output,-1.0}}),
        factoryPreset("Modern Rhythm", "Tight, dense post-amp rhythm shaping.",
                      {{Gate,3.2},{Tight,6.4},{Punch,4.8},{Body,3.0},{Mass,2.5},{Growl,3.4},{Drive,6.5},{Grind,3.2},{Bite,4.0},{Presence,2.8},{Smooth,1.6},{Attack,2.8},{Compression,1.5},{Output,-1.5}}),
        factoryPreset("Palm Weight", "Extra low-mid impact without loose sub bass.",
                      {{Gate,2.2},{Tight,4.5},{Punch,6.0},{Body,4.0},{Mass,5.4},{Drive,4.8},{Resonance,3.8},{Sag,1.6},{Smooth,1.4},{Output,-1.0}}),
        factoryPreset("Post Amp Bite", "Adds articulation and controlled edge after an amp sim.",
                      {{Drive,3.0},{Grind,2.1},{Bite,5.8},{Presence,4.2},{Air,2.2},{Smooth,1.8},{Texture,2.0},{Attack,3.4},{ParallelDry,8.0},{Output,-0.8}}),
        factoryPreset("Synth Growl", "Harmonic movement for basses and monophonic synths.",
                      {{Body,3.2},{Mass,4.0},{Growl,7.0},{Drive,5.2},{Grind,2.7},{HarmonicBias,4.5},{Bloom,2.8},{Dynamics,3.0},{Texture,3.8},{Smooth,2.2},{Output,-2.0}}),
        factoryPreset("Crushed Bloom", "Destroyed sustain with audible motion.",
                      {{Drive,7.4},{Grind,5.2},{Fuzz,3.6},{Bloom,7.0},{Sag,5.0},{Compression,7.2},{Dynamics,4.2},{Texture,4.4},{Smooth,3.8},{Output,-3.0}}),
        factoryPreset("Parallel Attack", "Dense wet body with a clean transient edge.",
                      {{Tight,3.0},{Punch,3.8},{Drive,6.2},{Growl,2.6},{Compression,4.0},{Attack,4.2},{ParallelDry,22.0},{Presence,2.4},{Output,-1.2}}),
        factoryPreset("Wide Open", "Open, bright distortion with restrained smoothing.",
                      {{Drive,5.8},{Growl,2.0},{Bite,3.8},{Presence,3.5},{Air,4.5},{Texture,1.8},{Attack,2.0},{Output,-1.0}}),
        factoryPreset("Color x2", "A deliberately exaggerated character preset.",
                      {{X2,1.0},{Growl,4.0},{Grind,3.0},{Fuzz,2.0},{Bloom,2.5},{Sag,2.5},{Texture,2.8},{HarmonicBias,2.8},{Smooth,2.0},{Output,-2.0}})
    };
}

std::filesystem::path PresetManager::userPresetDirectory() const {
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"))
        return std::filesystem::path(appData) / "GrowlForge" / "Presets";
#endif
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(xdg) / "GrowlForge" / "Presets";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "GrowlForge" / "Presets";
    return std::filesystem::temp_directory_path() / "GrowlForge" / "Presets";
}

void PresetManager::refresh() {
    std::lock_guard lock(mutex_);
    std::string preserveName = currentName_;
    presets_ = makeFactoryPresets();
    const auto directory = userPresetDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (!error && std::filesystem::exists(directory)) {
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
            if (error) break;
            if (entry.is_regular_file() && entry.path().extension() == ".gfpreset") paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) {
            Preset preset;
            if (parsePreset(readTextFile(path), preset)) {
                preset.factory = false;
                preset.path = path;
                if (preset.name.empty()) preset.name = path.stem().string();
                presets_.push_back(std::move(preset));
            }
        }
    }
    currentIndex_ = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].name == preserveName) {
            currentIndex_ = i;
            break;
        }
    }
}

bool PresetManager::applyPreset(const Preset& preset, size_t index) {
    for (clap_id id = 0; id < kParamCount; ++id) {
        if (!isPresetParameter(id)) continue;
        double value = clamp(preset.values[id], defs[id].min, defs[id].max);
        value = isToggleParameter(id) ? (value >= 0.5 ? 1.0 : 0.0) : quantize01(value);
        parameters_.values[id] = value;
        parameters_.guiPendingValue[id] = value;
        parameters_.guiPendingFlags[id].fetch_or(2u, std::memory_order_release);
    }
    parameters_.values[ApplyAutoGain] = 0.0;
    parameters_.autoGainResetPending = preset.values[AutoGain] >= 0.5;
    parameters_.configDirty = true;
    {
        std::lock_guard lock(mutex_);
        currentIndex_ = index;
        currentName_ = preset.name;
        dirty_ = false;
    }
    // Request the host callback only after releasing the preset mutex. Some
    // hosts may flush synchronously and echo parameter events immediately.
    parameters_.requestParamFlush();
    return true;
}

bool PresetManager::selectIndex(size_t index) {
    Preset preset;
    {
        std::lock_guard lock(mutex_);
        if (index >= presets_.size()) return false;
        preset = presets_[index];
    }
    return applyPreset(preset, index);
}

bool PresetManager::selectPrevious() {
    Preset preset;
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        if (presets_.empty()) return false;
        index = currentIndex_ == 0 ? presets_.size() - 1 : currentIndex_ - 1;
        preset = presets_[index];
    }
    return applyPreset(preset, index);
}

bool PresetManager::selectNext() {
    Preset preset;
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        if (presets_.empty()) return false;
        index = (currentIndex_ + 1) % presets_.size();
        preset = presets_[index];
    }
    return applyPreset(preset, index);
}

bool PresetManager::loadFile(const std::filesystem::path& path) {
    Preset preset;
    if (!parsePreset(readTextFile(path), preset)) return false;
    preset.factory = false;
    preset.path = path;
    if (preset.name.empty()) preset.name = path.stem().string();
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        presets_.push_back(preset);
        index = presets_.size() - 1;
    }
    return applyPreset(preset, index);
}

bool PresetManager::saveFile(const std::filesystem::path& path, const std::string& requestedName) {
    Preset preset;
    preset.name = requestedName.empty() ? path.stem().string() : requestedName;
    preset.author = "User";
    preset.description = "User preset";
    preset.factory = false;
    preset.path = path;
    for (size_t i = 0; i < kParamCount; ++i) preset.values[i] = parameters_.values[i].load();

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream << serializePreset(preset);
    if (!stream.good()) return false;

    std::lock_guard lock(mutex_);
    presets_.push_back(preset);
    currentIndex_ = presets_.size() - 1;
    currentName_ = preset.name;
    dirty_ = false;
    return true;
}

std::string PresetManager::currentName() const {
    std::lock_guard lock(mutex_);
    return currentName_ + (dirty_ ? " *" : "");
}

bool PresetManager::isDirty() const { std::lock_guard lock(mutex_); return dirty_; }
size_t PresetManager::presetCount() const { std::lock_guard lock(mutex_); return presets_.size(); }
size_t PresetManager::currentIndex() const { std::lock_guard lock(mutex_); return currentIndex_; }
std::vector<std::string> PresetManager::presetNames() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(presets_.size());
    for (const auto& preset : presets_) names.push_back(preset.name);
    return names;
}
void PresetManager::markDirty() { std::lock_guard lock(mutex_); dirty_ = true; }

void PresetManager::setCurrentNameFromState(const std::string& name) {
    std::lock_guard lock(mutex_);
    currentName_ = name.empty() ? "Project State" : name;
    dirty_ = false;
    currentIndex_ = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].name == currentName_) { currentIndex_ = i; break; }
    }
}

std::string PresetManager::escapeJson(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result.push_back(c); break;
        }
    }
    return result;
}

std::string PresetManager::serializePreset(const Preset& preset) {
    std::ostringstream stream;
    stream << "{\n"
           << "  \"format\": \"GrowlForgePreset\",\n"
           << "  \"version\": 1,\n"
           << "  \"name\": \"" << escapeJson(preset.name) << "\",\n"
           << "  \"author\": \"" << escapeJson(preset.author) << "\",\n"
           << "  \"description\": \"" << escapeJson(preset.description) << "\",\n"
           << "  \"parameters\": {\n";
    bool first = true;
    stream << std::fixed << std::setprecision(4);
    for (clap_id id = 0; id < kParamCount; ++id) {
        if (!isPresetParameter(id)) continue;
        if (!first) stream << ",\n";
        first = false;
        stream << "    \"" << defs[id].key << "\": " << preset.values[id];
    }
    stream << "\n  }\n}\n";
    return stream.str();
}

bool PresetManager::parsePreset(const std::string& text, Preset& preset) {
    if (text.find("GrowlForgePreset") == std::string::npos) return false;
    preset.values = defaultValues();
    preset.name = parseStringField(text, "name");
    preset.author = parseStringField(text, "author");
    preset.description = parseStringField(text, "description");
    for (clap_id id = 0; id < kParamCount; ++id) {
        if (!isPresetParameter(id)) continue;
        double value = 0.0;
        if (parseNumberField(text, defs[id].key, value))
            preset.values[id] = clamp(value, defs[id].min, defs[id].max);
    }
    return true;
}

std::string PresetManager::readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

} // namespace growlforge
