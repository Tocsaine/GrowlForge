#pragma once

#include "../parameters/ParameterStore.h"
#include <array>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace growlforge {

struct Preset {
    std::string name;
    std::string author;
    std::string description;
    std::array<double, kParamCount> values{};
    bool factory = false;
    std::filesystem::path path;
};

class PresetManager {
public:
    explicit PresetManager(ParameterStore& parameters);

    void refresh();
    bool selectPrevious();
    bool selectNext();
    bool selectIndex(size_t index);
    bool loadFile(const std::filesystem::path& path);

    bool saveFile(const std::filesystem::path& path, const std::string& requestedName = {});
    bool saveCurrent();
    bool renameCurrent(const std::filesystem::path& newPath, const std::string& requestedName = {});
    bool deleteCurrent();

    std::filesystem::path userPresetDirectory() const;
    std::string currentName() const;
    std::string currentCleanName() const;
    std::string currentDescription() const;
    std::filesystem::path currentPath() const;
    bool currentIsFactory() const;
    bool currentIsUser() const;
    bool isDirty() const;
    size_t presetCount() const;
    size_t currentIndex() const;
    std::vector<std::string> presetNames() const;
    void markDirty();
    void setCurrentNameFromState(const std::string& name);

private:
    static std::array<double, kParamCount> defaultValues();
    static std::vector<Preset> makeFactoryPresets();
    bool applyPreset(const Preset& preset, size_t index);
    bool writePresetFile(const std::filesystem::path& path, const Preset& preset) const;
    Preset captureCurrentPreset(const std::filesystem::path& path, const std::string& name) const;
    static bool parsePreset(const std::string& text, Preset& preset);
    static std::string serializePreset(const Preset& preset);
    static std::string escapeJson(const std::string& value);
    static std::string readTextFile(const std::filesystem::path& path);
    static bool samePath(const std::filesystem::path& a, const std::filesystem::path& b);

    ParameterStore& parameters_;
    mutable std::mutex mutex_;
    std::vector<Preset> presets_;
    size_t currentIndex_ = 0;
    std::string currentName_ = "Init";
    bool dirty_ = false;
};

} // namespace growlforge
