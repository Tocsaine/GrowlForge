#include "PresetManager.h"
#include "../common/Math.h"
#include <algorithm>
#include <cstdlib>
#include <cstddef>
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

std::filesystem::path ensurePresetExtension(std::filesystem::path path) {
    if (path.extension() != ".gfpreset") path.replace_extension(".gfpreset");
    return path;
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
                      {{X2,1.0},{Growl,4.0},{Grind,3.0},{Fuzz,2.0},{Bloom,2.5},{Sag,2.5},{Texture,2.8},{HarmonicBias,2.8},{Smooth,2.0},{Output,-2.0}}),

        // Personal starter bank for quick experimentation.
        factoryPreset("Tactile Crunch", "Responsive medium drive that keeps the pick close to the fingers.",
                      {{Drive,4.8},{Punch,2.8},{Body,2.3},{Growl,1.7},{Bite,2.0},{Attack,2.6},{Compression,0.8},{Output,-0.7}}),
        factoryPreset("Dense but Clear", "More density and sustain without burying chord detail.",
                      {{Tight,3.4},{Punch,3.0},{Body,3.2},{Mass,2.2},{Drive,6.0},{Grind,1.8},{Presence,2.4},{Smooth,1.7},{Compression,2.6},{ParallelDry,7.0},{Output,-1.3}}),
        factoryPreset("Low String Clamp", "Firm low-string control for fast riffs and hard palm mutes.",
                      {{Gate,3.8},{Tight,7.1},{Punch,5.6},{Mass,3.2},{Drive,5.4},{Grind,2.8},{Resonance,2.0},{Attack,3.5},{Output,-1.2}}),
        factoryPreset("Velvet Violence", "Heavy saturation with softened edges and a breathing tail.",
                      {{Drive,7.0},{Growl,3.6},{Fuzz,1.8},{Bloom,3.8},{Sag,3.4},{Compression,4.6},{Smooth,4.2},{Texture,2.5},{Air,1.2},{Output,-2.4}}),
        factoryPreset("Glass Teeth", "Bright, cutting articulation that stays controlled at the top.",
                      {{Drive,4.2},{Grind,3.8},{Bite,6.0},{Presence,5.2},{Air,3.4},{Smooth,2.5},{Attack,4.4},{ParallelDry,10.0},{Output,-1.5}}),
        factoryPreset("Living Fuzz", "A moving fuzz texture with useful note shape instead of pure noise.",
                      {{Drive,3.2},{Fuzz,7.0},{Growl,2.8},{HarmonicBias,3.6},{Bloom,2.6},{Dynamics,2.4},{Texture,4.0},{Smooth,2.8},{Attack,1.8},{Output,-2.2}})
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

bool PresetManager::samePath(const std::filesystem::path& a, const std::filesystem::path& b) {
    if (a.empty() || b.empty()) return false;
    std::error_code errorA, errorB;
    const auto ca = std::filesystem::weakly_canonical(a, errorA);
    const auto cb = std::filesystem::weakly_canonical(b, errorB);
    if (!errorA && !errorB) return ca == cb;
    return a.lexically_normal() == b.lexically_normal();
}

void PresetManager::refresh() {
    std::string preserveName;
    std::filesystem::path preservePath;
    {
        std::lock_guard lock(mutex_);
        preserveName = currentName_;
        if (currentIndex_ < presets_.size()) preservePath = presets_[currentIndex_].path;
    }

    std::vector<Preset> refreshed = makeFactoryPresets();
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
                refreshed.push_back(std::move(preset));
            }
        }
    }

    std::lock_guard lock(mutex_);
    presets_ = std::move(refreshed);
    currentIndex_ = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        if ((!preservePath.empty() && samePath(presets_[i].path, preservePath)) ||
            (preservePath.empty() && presets_[i].name == preserveName)) {
            currentIndex_ = i;
            currentName_ = presets_[i].name;
            return;
        }
    }
    if (!preservePath.empty()) {
        currentName_ = "Unsaved";
        dirty_ = true;
    } else if (!presets_.empty()) {
        currentName_ = presets_[0].name;
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
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        if (presets_.empty()) return false;
        index = currentIndex_ == 0 ? presets_.size() - 1 : currentIndex_ - 1;
    }
    return selectIndex(index);
}

bool PresetManager::selectNext() {
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        if (presets_.empty()) return false;
        index = (currentIndex_ + 1) % presets_.size();
    }
    return selectIndex(index);
}

bool PresetManager::loadFile(const std::filesystem::path& inputPath) {
    const auto path = ensurePresetExtension(inputPath);
    Preset preset;
    if (!parsePreset(readTextFile(path), preset)) return false;
    preset.factory = false;
    preset.path = path;
    if (preset.name.empty()) preset.name = path.stem().string();

    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        auto it = std::find_if(presets_.begin(), presets_.end(), [&](const Preset& item) {
            return !item.factory && samePath(item.path, path);
        });
        if (it == presets_.end()) {
            presets_.push_back(preset);
            index = presets_.size() - 1;
        } else {
            *it = preset;
            index = static_cast<size_t>(std::distance(presets_.begin(), it));
        }
    }
    return applyPreset(preset, index);
}

Preset PresetManager::captureCurrentPreset(const std::filesystem::path& path, const std::string& name) const {
    Preset preset;
    preset.name = name.empty() ? path.stem().string() : name;
    preset.author = "User";
    preset.description = "User preset";
    preset.factory = false;
    preset.path = path;
    for (size_t i = 0; i < kParamCount; ++i) preset.values[i] = parameters_.values[i].load();
    return preset;
}

bool PresetManager::writePresetFile(const std::filesystem::path& inputPath, const Preset& preset) const {
    const auto path = ensurePresetExtension(inputPath);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::path backup = path;
    backup += ".bak";
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << serializePreset(preset);
        if (!stream.good()) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return false;
        }
    }

    const bool hadOriginal = std::filesystem::exists(path);
    if (hadOriginal) {
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(path, backup, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
    }

    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(backup, path, restoreError);
        }
        std::filesystem::remove(temporary, error);
        return false;
    }
    if (hadOriginal) std::filesystem::remove(backup, error);
    return true;
}

bool PresetManager::saveFile(const std::filesystem::path& inputPath, const std::string& requestedName) {
    const auto path = ensurePresetExtension(inputPath);
    Preset preset = captureCurrentPreset(path, requestedName.empty() ? path.stem().string() : requestedName);
    if (!writePresetFile(path, preset)) return false;

    std::lock_guard lock(mutex_);
    auto it = std::find_if(presets_.begin(), presets_.end(), [&](const Preset& item) {
        return !item.factory && samePath(item.path, path);
    });
    if (it == presets_.end()) {
        presets_.push_back(preset);
        currentIndex_ = presets_.size() - 1;
    } else {
        *it = preset;
        currentIndex_ = static_cast<size_t>(std::distance(presets_.begin(), it));
    }
    currentName_ = preset.name;
    dirty_ = false;
    return true;
}

bool PresetManager::saveCurrent() {
    std::filesystem::path path;
    std::string name;
    {
        std::lock_guard lock(mutex_);
        if (currentIndex_ >= presets_.size() || presets_[currentIndex_].factory || presets_[currentIndex_].path.empty()) return false;
        path = presets_[currentIndex_].path;
        name = presets_[currentIndex_].name;
    }
    return saveFile(path, name);
}

bool PresetManager::renameCurrent(const std::filesystem::path& inputPath, const std::string& requestedName) {
    const auto newPath = ensurePresetExtension(inputPath);
    std::filesystem::path oldPath;
    size_t oldIndex = 0;
    {
        std::lock_guard lock(mutex_);
        if (currentIndex_ >= presets_.size() || presets_[currentIndex_].factory || presets_[currentIndex_].path.empty()) return false;
        oldIndex = currentIndex_;
        oldPath = presets_[currentIndex_].path;
    }
    if (!samePath(oldPath, newPath) && std::filesystem::exists(newPath)) return false;

    const std::string newName = requestedName.empty() ? newPath.stem().string() : requestedName;
    Preset renamed = captureCurrentPreset(newPath, newName);
    if (!writePresetFile(newPath, renamed)) return false;

    std::error_code error;
    if (!samePath(oldPath, newPath)) std::filesystem::remove(oldPath, error);

    std::lock_guard lock(mutex_);
    if (oldIndex >= presets_.size()) return false;
    presets_[oldIndex] = renamed;
    for (size_t i = presets_.size(); i-- > 0;) {
        if (i != oldIndex && !presets_[i].factory && samePath(presets_[i].path, newPath)) {
            presets_.erase(presets_.begin() + static_cast<std::ptrdiff_t>(i));
            if (i < oldIndex) --oldIndex;
        }
    }
    currentIndex_ = oldIndex;
    currentName_ = renamed.name;
    dirty_ = false;
    return true;
}

bool PresetManager::deleteCurrent() {
    std::filesystem::path path;
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        if (currentIndex_ >= presets_.size() || presets_[currentIndex_].factory || presets_[currentIndex_].path.empty()) return false;
        index = currentIndex_;
        path = presets_[currentIndex_].path;
    }
    std::error_code error;
    if (!std::filesystem::remove(path, error) && error) return false;

    std::lock_guard lock(mutex_);
    if (index < presets_.size()) presets_.erase(presets_.begin() + static_cast<std::ptrdiff_t>(index));
    currentIndex_ = 0;
    currentName_ = "Unsaved";
    dirty_ = true;
    return true;
}

std::string PresetManager::currentName() const {
    std::lock_guard lock(mutex_);
    return currentName_ + (dirty_ ? " *" : "");
}

std::string PresetManager::currentCleanName() const {
    std::lock_guard lock(mutex_);
    return currentName_;
}

std::string PresetManager::currentDescription() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() ? presets_[currentIndex_].description : std::string{};
}

std::filesystem::path PresetManager::currentPath() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() ? presets_[currentIndex_].path : std::filesystem::path{};
}

bool PresetManager::currentIsFactory() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() && presets_[currentIndex_].factory;
}

bool PresetManager::currentIsUser() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() && !presets_[currentIndex_].factory && !presets_[currentIndex_].path.empty();
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

void PresetManager::markDirty() {
    std::lock_guard lock(mutex_);
    dirty_ = true;
}

void PresetManager::setCurrentNameFromState(const std::string& name) {
    std::lock_guard lock(mutex_);
    currentName_ = name.empty() ? "Project State" : name;
    dirty_ = false;
    currentIndex_ = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].name == currentName_) {
            currentIndex_ = i;
            break;
        }
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
           << "  \"version\": 2,\n"
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
