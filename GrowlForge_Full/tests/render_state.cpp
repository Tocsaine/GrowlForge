#include <clap/clap.h>
#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
const void* hostGetExtension(const clap_host_t*,const char*){return nullptr;} void hostNoop(const clap_host_t*){}
struct EmptyIn{clap_input_events_t iface{};EmptyIn(){iface.ctx=this;iface.size=[](const clap_input_events_t*){return 0u;};iface.get=[](const clap_input_events_t*,uint32_t)->const clap_event_header_t*{return nullptr;};}};
struct EmptyOut{clap_output_events_t iface{};EmptyOut(){iface.ctx=this;iface.try_push=[](const clap_output_events_t*,const clap_event_header_t*){return true;};}};
struct Reader{clap_istream_t iface{};std::vector<uint8_t> bytes;size_t pos=0;Reader(const char*path){std::ifstream f(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});iface.ctx=this;iface.read=[](const clap_istream_t*s,void*b,uint64_t n)->int64_t{auto*self=static_cast<Reader*>(s->ctx);const size_t count=std::min<size_t>(n,self->bytes.size()-self->pos);std::memcpy(b,self->bytes.data()+self->pos,count);self->pos+=count;return static_cast<int64_t>(count);};}};
float sig(uint64_t n,int c){double t=n/48000.0;return(float)(0.22*std::sin(6.283185307179586*(c?131.0:103.0)*t)+0.09*std::sin(6.283185307179586*731.0*t)+(n%3000<40?0.5*std::exp(-(double)(n%3000)/8.0):0.0));}
}
int main(int argc,char**argv){if(argc!=4)return 2;void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib)return 3;auto*entry=(const clap_plugin_entry_t*)dlsym(lib,"clap_entry");if(!entry||!entry->init(argv[1]))return 4;auto*factory=(const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);auto*desc=factory->get_plugin_descriptor(factory,0);clap_host_t host{};host.clap_version=CLAP_VERSION;host.name="LoadHost";host.vendor="GF";host.url="";host.version="1";host.get_extension=hostGetExtension;host.request_restart=hostNoop;host.request_process=hostNoop;host.request_callback=hostNoop;auto*plugin=factory->create_plugin(factory,&host,desc->id);if(!plugin||!plugin->init(plugin))return 5;auto*state=(const clap_plugin_state_t*)plugin->get_extension(plugin,CLAP_EXT_STATE);Reader reader(argv[2]);if(!state||!state->load(plugin,&reader.iface))return 6;if(!plugin->activate(plugin,48000,1,128)||!plugin->start_processing(plugin))return 7;std::array<std::array<float,128>,2> in{},out{};float*ip[2]{in[0].data(),in[1].data()};float*op[2]{out[0].data(),out[1].data()};clap_audio_buffer_t ib{ip,nullptr,2,0,0},ob{op,nullptr,2,0,0};EmptyIn ei;EmptyOut eo;std::ofstream file(argv[3],std::ios::binary);for(uint64_t off=0;off<48000;off+=128){uint32_t frames=(uint32_t)std::min<uint64_t>(128,48000-off);for(int c=0;c<2;++c)for(uint32_t i=0;i<frames;++i)in[c][i]=sig(off+i,c);clap_process_t p{};p.frames_count=frames;p.audio_inputs=&ib;p.audio_outputs=&ob;p.audio_inputs_count=p.audio_outputs_count=1;p.in_events=&ei.iface;p.out_events=&eo.iface;if(plugin->process(plugin,&p)==CLAP_PROCESS_ERROR)return 8;for(uint32_t i=0;i<frames;++i){file.write((char*)&out[0][i],4);file.write((char*)&out[1][i],4);}}plugin->stop_processing(plugin);plugin->deactivate(plugin);plugin->destroy(plugin);entry->deinit();dlclose(lib);return 0;}
