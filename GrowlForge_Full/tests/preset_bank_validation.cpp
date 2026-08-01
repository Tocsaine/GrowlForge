#include "../src/state/PresetManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

int main() {
    using namespace growlforge;
    ParameterStore store(nullptr);
    PresetManager presets(store);
    const auto infos = presets.presetInfos();

    size_t pre = 0, post = 0, creative = 0, utility = 0;
    std::set<std::string> names;
    for (const auto& info : infos) {
        names.insert(info.name);
        if (info.category == "Pre-Amp") ++pre;
        else if (info.category == "Post-Amp") ++post;
        else if (info.category == "Creative") ++creative;
        else if (info.category == "Utility") ++utility;
    }
    if (pre < 5 || post < 5 || creative < 5 || utility < 1) {
        std::cerr << "category count failure: " << pre << '/' << post << '/' << creative << '/' << utility << '\n';
        return 1;
    }

    const char* expected[] = {
        "156/Silence - People Watching",
        "Thornhill - BODIES",
        "ERRA - silence outlives the earth",
        "ERRA - CURE",
        "Loathe - A Stranger to You",
        "thrown - EXCESSIVE GUILT",
        "HLB - Abyssal Weight",
        "Spiritbox - Tsunami Sea",
        "Deftones - private music",
        "Sleep Token - Take Me Back to Eden"
    };
    for (const char* name : expected) {
        if (!names.contains(name)) {
            std::cerr << "missing artist preset: " << name << '\n';
            return 2;
        }
    }

    store.values[Bypass] = 1.0;
    for (const auto& info : infos) {
        if (!presets.selectIndex(info.index)) {
            std::cerr << "could not select preset: " << info.name << '\n';
            return 3;
        }
        if (store.values[Bypass].load() != 1.0) {
            std::cerr << "preset changed bypass: " << info.name << '\n';
            return 4;
        }
        for (clap_id id = 0; id < kParamCount; ++id) {
            const double value = store.values[id].load();
            if (!std::isfinite(value) || value < defs[id].min - 1e-9 || value > defs[id].max + 1e-9) {
                std::cerr << "invalid value in " << info.name << " param " << id << ": " << value << '\n';
                return 5;
            }
        }
    }

    const auto dir = std::filesystem::temp_directory_path() / "GrowlForgePresetBankTest";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto path = dir / "Metadata.gfpreset";
    if (!presets.saveFile(path, "Metadata")) return 6;
    std::ifstream file(path);
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    if (text.find("\"version\": 3") == std::string::npos ||
        text.find("\"category\": \"User\"") == std::string::npos ||
        text.find("\"referenceChain\"") == std::string::npos) {
        std::cerr << "preset metadata missing\n";
        return 7;
    }

    // Version-2 user presets without the new metadata must remain loadable.
    const auto oldPath = dir / "OldV2.gfpreset";
    std::ofstream old(oldPath);
    old << "{\n  \"format\": \"GrowlForgePreset\",\n  \"version\": 2,\n"
           "  \"name\": \"Old V2\",\n  \"author\": \"User\",\n"
           "  \"description\": \"Legacy\",\n  \"parameters\": {\n    \"drive\": 4.2000\n  }\n}\n";
    old.close();
    if (!presets.loadFile(oldPath) || std::abs(store.values[Drive].load() - 4.2) > 1e-9 ||
        presets.currentCategory() != "User") {
        std::cerr << "legacy preset compatibility failure\n";
        return 8;
    }

    std::filesystem::remove_all(dir);
    std::cout << "preset bank validation: ok; pre=" << pre
              << " post=" << post << " creative=" << creative
              << " total=" << infos.size() << '\n';
    return 0;
}
