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

Preset factoryPreset(const char* category, const char* name, const char* description,
                     const std::initializer_list<std::pair<clap_id, double>>& changes,
                     const char* inspiredBy = "", const char* referenceChain = "") {
    Preset preset;
    preset.name = name;
    preset.author = "GrowlForge";
    preset.description = description;
    preset.category = category;
    preset.inspiredBy = inspiredBy;
    preset.referenceChain = referenceChain;
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
    constexpr const char* preNolly =
        "GrowlForge before Archetype Nolly X Rhythm amp; start with Nolly boost off, amp gain around 4-5, EQ near noon.";
    constexpr const char* postNolly =
        "Archetype Nolly X Rhythm amp and cab first, then GrowlForge; start with Nolly EQ near noon and output at unity.";

    return {
        factoryPreset("Utility", "Init", "Neutral starting point.", {}),

        // PRE-AMP: general-purpose starting points.
        factoryPreset("Pre-Amp", "Tight Foundation",
                      "A balanced front-end clamp: tighter lows, firmer pick response and very little extra distortion.",
                      {{Gate,2.2},{Tight,6.2},{Punch,3.8},{Body,1.6},{Mass,1.0},{Drive,1.2},{Grind,1.0},{Attack,2.8}},
                      "", preNolly),
        factoryPreset("Pre-Amp", "Low-Tuned Clamp",
                      "Hard low-string control for extended-range rhythm playing without hollowing out the note body.",
                      {{Gate,4.2},{Tight,8.0},{Punch,5.5},{Body,1.2},{Mass,2.0},{Growl,1.5},{Drive,1.5},{Grind,2.5},{Attack,3.5},{Output,-0.5}},
                      "", preNolly),
        factoryPreset("Pre-Amp", "Open Pick Push",
                      "A more open boost that emphasizes the hand and attack instead of aggressively filtering the low end.",
                      {{Gate,1.3},{Tight,3.2},{Punch,4.2},{Body,2.5},{Drive,2.8},{Bite,1.8},{Attack,4.5},{Output,-0.5}},
                      "", preNolly),
        factoryPreset("Pre-Amp", "Warm Saturation Push",
                      "Feeds the amp a rounded, harmonically dense signal for thick riffs and less clinical sustain.",
                      {{Tight,2.5},{Punch,2.0},{Body,3.2},{Mass,2.2},{Growl,2.0},{Drive,3.5},{Smooth,2.5},{Output,-1.0}},
                      "", preNolly),
        factoryPreset("Pre-Amp", "Controlled Fuzz Feed",
                      "A restrained fuzz layer before the amp: audible texture, but still playable and responsive.",
                      {{Gate,2.0},{Tight,4.0},{Body,2.0},{Drive,1.5},{Fuzz,3.8},{Smooth,1.8},{Attack,2.0},{Output,-1.5}},
                      "", preNolly),

        // PRE-AMP: album / artist inspired front-end shaping.
        factoryPreset("Pre-Amp", "156/Silence - People Watching",
                      "Bouncy, pummelling and slightly chaotic front-end shaping with a hard rhythmic center.",
                      {{Gate,3.5},{Tight,6.5},{Punch,5.0},{Body,1.5},{Mass,1.2},{Growl,2.4},{Drive,2.4},{Grind,2.8},{Bite,1.8},{Attack,4.0},{Compression,0.5},{Output,-0.7}},
                      "156/Silence - People Watching", preNolly),
        factoryPreset("Pre-Amp", "ERRA - CURE",
                      "Groove-first tightening with strong right-hand definition and controlled low-mid pressure.",
                      {{Gate,2.8},{Tight,6.2},{Punch,5.2},{Body,2.0},{Mass,1.5},{Drive,2.0},{Grind,1.7},{Bite,1.5},{Dynamics,1.5},{Attack,4.3},{Output,-0.4}},
                      "ERRA - CURE", preNolly),
        factoryPreset("Pre-Amp", "thrown - EXCESSIVE GUILT",
                      "Chunky, metallic and industrial: a tight modern hardcore feed with serrated upper-mid attack.",
                      {{Gate,5.0},{Tight,7.2},{Punch,5.6},{Body,1.3},{Mass,1.0},{Growl,1.8},{Drive,2.8},{Grind,4.2},{Bite,3.2},{Texture,1.5},{Smooth,1.0},{Attack,3.8},{Output,-1.2}},
                      "thrown - EXCESSIVE GUILT", preNolly),
        factoryPreset("Pre-Amp", "HLB - Abyssal Weight",
                      "Ultra-low, dense and punishing pre-shaping inspired by Humanity's Last Breath, with clarity preserved above the sub range.",
                      {{Gate,4.5},{Tight,8.7},{Punch,6.0},{Body,1.8},{Mass,2.8},{Growl,3.5},{Drive,2.8},{Grind,3.5},{Bite,1.2},{Smooth,2.0},{Attack,3.0},{Output,-1.3}},
                      "Humanity's Last Breath", preNolly),
        factoryPreset("Pre-Amp", "Sleep Token - Take Me Back to Eden",
                      "Thick and rounded djent pressure with a softer edge, intended to leave room for atmospheric layers.",
                      {{Gate,2.2},{Tight,5.0},{Punch,4.3},{Body,3.0},{Mass,2.8},{Growl,1.8},{Drive,2.6},{Grind,1.0},{Smooth,2.8},{Attack,2.3},{Output,-0.8}},
                      "Sleep Token - Take Me Back to Eden", preNolly),

        // POST-AMP: general-purpose finishing presets.
        factoryPreset("Post-Amp", "Finished Rhythm",
                      "A controlled final polish for an already good rhythm tone: density, edge and transient clarity.",
                      {{AutoGain,1.0},{Tight,1.5},{Punch,2.0},{Body,2.0},{Drive,2.6},{Grind,1.4},{Bite,2.8},{Presence,2.0},{Smooth,2.0},{Compression,1.5},{Attack,2.0},{ParallelDry,7.0}},
                      "", postNolly),
        factoryPreset("Post-Amp", "Dense but Clear",
                      "Adds sustain and low-mid density while keeping chord detail and a clean transient outline.",
                      {{AutoGain,1.0},{Tight,1.8},{Punch,3.0},{Body,3.2},{Mass,2.2},{Drive,3.2},{Grind,1.8},{Presence,2.4},{Smooth,1.7},{Compression,2.6},{ParallelDry,7.0},{Output,-0.5}},
                      "", postNolly),
        factoryPreset("Post-Amp", "Air and Bite",
                      "Brighter finished tone with extra articulation, top-end openness and restrained smoothing.",
                      {{AutoGain,1.0},{Drive,1.8},{Bite,4.5},{Presence,3.8},{Air,4.0},{Smooth,2.0},{Attack,2.5},{ParallelDry,10.0},{Output,-0.4}},
                      "", postNolly),
        factoryPreset("Post-Amp", "Low-Mid Weight",
                      "Adds physical weight and low-mid impact after the cabinet without relying on loose sub bass.",
                      {{AutoGain,1.0},{Punch,3.0},{Body,4.0},{Mass,3.5},{Growl,2.0},{Drive,1.8},{Resonance,2.0},{Smooth,2.2},{Output,-0.5}},
                      "", postNolly),
        factoryPreset("Post-Amp", "Smooth Wall",
                      "A broad, softened wall of sound with breathing sustain and less abrasive high-frequency texture.",
                      {{AutoGain,1.0},{Body,3.0},{Mass,2.0},{Growl,2.5},{Drive,3.0},{Bloom,3.0},{Sag,2.0},{Compression,3.5},{Texture,2.0},{Smooth,4.0},{Air,1.0},{Output,-0.8}},
                      "", postNolly),

        // POST-AMP: album / artist inspired finishing.
        factoryPreset("Post-Amp", "Thornhill - BODIES",
                      "Dreamy depth, distorted low-mid motion and sharp heavy/soft contrast without losing the rhythmic pulse.",
                      {{AutoGain,1.0},{Punch,2.0},{Body,3.8},{Mass,2.2},{Growl,2.3},{Drive,2.6},{Fuzz,0.8},{Bloom,3.4},{Sag,2.2},{Compression,2.4},{Texture,3.2},{Smooth,4.0},{Presence,1.8},{Air,2.4},{ParallelDry,7.0}},
                      "Thornhill - BODIES", postNolly),
        factoryPreset("Post-Amp", "ERRA - silence outlives the earth",
                      "Slick technical definition, spiky articulation and a polished modern midrange for rapid riff changes.",
                      {{AutoGain,1.0},{Tight,2.0},{Punch,2.5},{Body,1.6},{Growl,1.8},{Drive,2.3},{Grind,2.3},{Bite,3.5},{Presence,3.0},{Air,2.5},{Smooth,1.5},{Dynamics,2.0},{Attack,3.3},{ParallelDry,8.0}},
                      "ERRA - silence outlives the earth", postNolly),
        factoryPreset("Post-Amp", "Loathe - A Stranger to You",
                      "Warm baritone mass and digital-maximalist texture, balancing crushing density with a hazy melodic surface.",
                      {{AutoGain,1.0},{Punch,2.8},{Body,4.2},{Mass,3.2},{Growl,3.0},{Drive,3.0},{Grind,2.4},{Fuzz,0.8},{Bloom,3.2},{Compression,3.5},{Texture,4.2},{Smooth,4.4},{Air,1.8},{ParallelDry,5.0},{Output,-0.5}},
                      "Loathe - A Stranger to You", postNolly),
        factoryPreset("Post-Amp", "Spiritbox - Tsunami Sea",
                      "Refined crushing weight, technical precision and a compressed atmospheric sheen.",
                      {{AutoGain,1.0},{Tight,2.0},{Punch,3.2},{Body,2.8},{Mass,2.0},{Growl,2.0},{Drive,2.4},{Grind,2.1},{Bite,2.8},{Presence,2.5},{Air,3.0},{Smooth,3.0},{Bloom,2.8},{Dynamics,2.5},{Compression,4.8},{Attack,2.5},{ParallelDry,8.0}},
                      "Spiritbox - Tsunami Sea", postNolly),
        factoryPreset("Post-Amp", "Deftones - private music",
                      "Thick sustained chords, polished punch and a soft-focus haze around a heavy, menacing core.",
                      {{AutoGain,1.0},{Punch,2.0},{Body,4.5},{Mass,3.5},{Growl,2.2},{Drive,3.4},{Grind,1.0},{Bloom,3.6},{Sag,2.8},{Compression,2.8},{Texture,2.0},{Smooth,4.8},{Air,2.8},{ParallelDry,10.0},{Output,-0.5}},
                      "Deftones - private music", postNolly),

        // CREATIVE: intentionally less conventional combinations.
        factoryPreset("Creative", "Living Fuzz",
                      "A moving fuzz texture with useful note shape instead of pure noise.",
                      {{Drive,3.2},{Fuzz,7.0},{Growl,2.8},{HarmonicBias,3.6},{Bloom,2.6},{Dynamics,2.4},{Texture,4.0},{Smooth,2.8},{Attack,1.8},{Output,-2.2}}),
        factoryPreset("Creative", "Crushed Bloom",
                      "Destroyed sustain with audible motion and deliberately excessive compression.",
                      {{Drive,7.4},{Grind,5.2},{Fuzz,3.6},{Bloom,7.0},{Sag,5.0},{Compression,7.2},{Dynamics,4.2},{Texture,4.4},{Smooth,3.8},{Output,-3.0}}),
        factoryPreset("Creative", "Glass Teeth",
                      "Bright, cutting articulation that stays controlled at the top.",
                      {{Drive,4.2},{Grind,3.8},{Bite,6.0},{Presence,5.2},{Air,3.4},{Smooth,2.5},{Attack,4.4},{ParallelDry,10.0},{Output,-1.5}}),
        factoryPreset("Creative", "Synth Growl",
                      "Harmonic movement for basses, drones and monophonic synthesizers.",
                      {{Body,3.2},{Mass,4.0},{Growl,7.0},{Drive,5.2},{Grind,2.7},{HarmonicBias,4.5},{Bloom,2.8},{Dynamics,3.0},{Texture,3.8},{Smooth,2.2},{Output,-2.0}}),
        factoryPreset("Creative", "Parallel Attack",
                      "Dense wet body with a clean transient edge mixed back in parallel.",
                      {{Tight,3.0},{Punch,3.8},{Drive,6.2},{Growl,2.6},{Compression,4.0},{Attack,4.2},{ParallelDry,22.0},{Presence,2.4},{Output,-1.2}}),
        factoryPreset("Creative", "Velvet Violence",
                      "Heavy saturation with softened edges and a breathing tail.",
                      {{Drive,7.0},{Growl,3.6},{Fuzz,1.8},{Bloom,3.8},{Sag,3.4},{Compression,4.6},{Smooth,4.2},{Texture,2.5},{Air,1.2},{Output,-2.4}}),
        factoryPreset("Creative", "Color x2",
                      "A deliberately exaggerated character stack using x2 COLOR without changing Drive itself.",
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
    preset.category = "User";
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

std::string PresetManager::currentCategory() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() ? presets_[currentIndex_].category : std::string{};
}

std::string PresetManager::currentInspiredBy() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() ? presets_[currentIndex_].inspiredBy : std::string{};
}

std::string PresetManager::currentReferenceChain() const {
    std::lock_guard lock(mutex_);
    return currentIndex_ < presets_.size() ? presets_[currentIndex_].referenceChain : std::string{};
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

std::vector<PresetInfo> PresetManager::presetInfos() const {
    std::lock_guard lock(mutex_);
    std::vector<PresetInfo> infos;
    infos.reserve(presets_.size());
    for (size_t i = 0; i < presets_.size(); ++i)
        infos.push_back(PresetInfo{i, presets_[i].name, presets_[i].category, presets_[i].factory});
    return infos;
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
           << "  \"version\": 3,\n"
           << "  \"name\": \"" << escapeJson(preset.name) << "\",\n"
           << "  \"author\": \"" << escapeJson(preset.author) << "\",\n"
           << "  \"description\": \"" << escapeJson(preset.description) << "\",\n"
           << "  \"category\": \"" << escapeJson(preset.category) << "\",\n"
           << "  \"inspiredBy\": \"" << escapeJson(preset.inspiredBy) << "\",\n"
           << "  \"referenceChain\": \"" << escapeJson(preset.referenceChain) << "\",\n"
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
    preset.category = parseStringField(text, "category");
    preset.inspiredBy = parseStringField(text, "inspiredBy");
    preset.referenceChain = parseStringField(text, "referenceChain");
    if (preset.category.empty()) preset.category = "User";
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
