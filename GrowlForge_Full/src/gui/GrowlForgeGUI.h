#pragma once

#include <clap/clap.h>

namespace growlforge {

struct GrowlForge;

bool growlForgeGuiGlobalInit();
void growlForgeGuiGlobalShutdown();
void destroyGrowlForgeGui(GrowlForge* instance);
extern const clap_plugin_gui_t guiExt;

} // namespace growlforge
