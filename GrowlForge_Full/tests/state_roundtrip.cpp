#include <clap/clap.h>
#include <dlfcn.h>
#include <algorithm>
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
        iface.ctx=this;
        iface.size=[](const clap_input_events_t* l){return static_cast<uint32_t>(static_cast<InputEvents*>(l->ctx)->events.size());};
        iface.get=[](const clap_input_events_t* l,uint32_t i)->const clap_event_header_t*{
            auto* s=static_cast<InputEvents*>(l->ctx);return i<s->events.size()?&s->events[i].header:nullptr;};
    }
    void add(clap_id id,double value){
        clap_event_param_value_t e{};e.header.size=sizeof(e);e.header.space_id=CLAP_CORE_EVENT_SPACE_ID;
        e.header.type=CLAP_EVENT_PARAM_VALUE;e.param_id=id;e.note_id=-1;e.port_index=-1;e.channel=-1;e.key=-1;e.value=value;
        events.push_back(e);
    }
};
struct OutputEvents {clap_output_events_t iface{};OutputEvents(){iface.ctx=this;iface.try_push=[](const clap_output_events_t*,const clap_event_header_t*){return true;};}};
struct MemoryWriter {clap_ostream_t iface{};std::vector<uint8_t> bytes;MemoryWriter(){iface.ctx=this;iface.write=[](const clap_ostream_t* s,const void* b,uint64_t n)->int64_t{auto* self=static_cast<MemoryWriter*>(s->ctx);auto* p=static_cast<const uint8_t*>(b);self->bytes.insert(self->bytes.end(),p,p+n);return static_cast<int64_t>(n);};}};
}

int main(int argc,char**argv){
 if(argc!=3){std::fprintf(stderr,"usage: %s plugin output.state\n",argv[0]);return 2;}
 void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){std::fprintf(stderr,"%s\n",dlerror());return 3;}
 auto* entry=reinterpret_cast<const clap_plugin_entry_t*>(dlsym(lib,"clap_entry"));if(!entry||!entry->init(argv[1]))return 4;
 auto* factory=static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));auto* desc=factory->get_plugin_descriptor(factory,0);
 clap_host_t host{};host.clap_version=CLAP_VERSION;host.name="StateHost";host.vendor="GrowlForge";host.url="";host.version="1";host.get_extension=hostGetExtension;host.request_restart=hostNoop;host.request_process=hostNoop;host.request_callback=hostNoop;
 auto* plugin=factory->create_plugin(factory,&host,desc->id);if(!plugin||!plugin->init(plugin))return 5;
 auto* params=static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin,CLAP_EXT_PARAMS));
 auto* state=static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin,CLAP_EXT_STATE));if(!params||!state)return 6;
 InputEvents input;OutputEvents output;
 // Exercise every persisted parameter with non-default values. Read-only and momentary parameters are intentionally skipped.
 for(uint32_t i=0;i<params->count(plugin);++i){clap_param_info_t info{};if(!params->get_info(plugin,i,&info))continue;if(info.flags&CLAP_PARAM_IS_READONLY)continue;if(info.id==20)continue;double n=(i%7+1)/8.0;double v=info.min_value+(info.max_value-info.min_value)*n;input.add(info.id,v);}
 params->flush(plugin,&input.iface,&output.iface);
 MemoryWriter writer;if(!state->save(plugin,&writer.iface))return 7;
 std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(writer.bytes.data()),static_cast<std::streamsize>(writer.bytes.size()));
 plugin->destroy(plugin);entry->deinit();dlclose(lib);return file.good()?0:8;
}
