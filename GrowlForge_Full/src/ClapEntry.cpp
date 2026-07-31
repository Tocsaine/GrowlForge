#include <clap/clap.h>
#include "plugin/PluginFactory.h"
#include "gui/GrowlForgeGUI.h"
#include <cstring>

namespace growlforge {

namespace {

bool entryInit(const char*) { return growlForgeGuiGlobalInit(); }
void entryDeinit() { growlForgeGuiGlobalShutdown(); }
const void* entryFactory(const char* id) {
    return id && !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? pluginFactory() : nullptr;
}

} // namespace

} // namespace growlforge

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    CLAP_VERSION,
    growlforge::entryInit,
    growlforge::entryDeinit,
    growlforge::entryFactory
};
