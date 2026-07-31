#include <clap/clap.h>
#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
const void* hostExt(const clap_host_t*, const char*) { return nullptr; }
void noop(const clap_host_t*) {}
struct InEvents {
  clap_input_events_t iface{}; std::vector<clap_event_param_value_t> e;
  InEvents(){ iface.ctx=this; iface.size=[](const clap_input_events_t*l){return (uint32_t)static_cast<const InEvents*>(l->ctx)->e.size();}; iface.get=[](const clap_input_events_t*l,uint32_t i)->const clap_event_header_t*{auto*s=static_cast<const InEvents*>(l->ctx);return i<s->e.size()?&s->e[i].header:nullptr;}; }
  void add(clap_id id,double v){clap_event_param_value_t x{};x.header.size=sizeof(x);x.header.space_id=CLAP_CORE_EVENT_SPACE_ID;x.header.type=CLAP_EVENT_PARAM_VALUE;x.param_id=id;x.note_id=x.port_index=x.channel=x.key=-1;x.value=v;e.push_back(x);}
};
struct OutEvents {clap_output_events_t iface{};OutEvents(){iface.ctx=this;iface.try_push=[](const clap_output_events_t*,const clap_event_header_t*){return true;};}};
}
int main(int argc,char**argv){
 if(argc!=2)return 2;void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){std::puts(dlerror());return 3;}auto*entry=(const clap_plugin_entry_t*)dlsym(lib,"clap_entry");if(!entry||!entry->init(argv[1]))return 4;auto*factory=(const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);auto*d=factory->get_plugin_descriptor(factory,0);
 clap_host_t h{};h.clap_version=CLAP_VERSION;h.name="FeatureTest";h.vendor="GF";h.url="";h.version="1";h.get_extension=hostExt;h.request_restart=noop;h.request_process=noop;h.request_callback=noop;
 auto*p=factory->create_plugin(factory,&h,d->id);if(!p||!p->init(p)||!p->activate(p,48000,1,128)||!p->start_processing(p))return 5;auto*params=(const clap_plugin_params_t*)p->get_extension(p,CLAP_EXT_PARAMS);
 constexpr int N=128;std::array<std::array<float,N>,2> in{},out{};float*ip[2]{in[0].data(),in[1].data()},*op[2]{out[0].data(),out[1].data()};clap_audio_buffer_t ib{ip,nullptr,2,0,0},ob{op,nullptr,2,0,0};OutEvents oe;uint64_t sample=0;
 auto block=[&](InEvents&ie){clap_process_t pr{};pr.frames_count=N;pr.steady_time=sample;pr.audio_inputs=&ib;pr.audio_outputs=&ob;pr.audio_inputs_count=pr.audio_outputs_count=1;pr.in_events=&ie.iface;pr.out_events=&oe.iface;auto st=p->process(p,&pr);sample+=N;return st!=CLAP_PROCESS_ERROR;};
 // User Output must survive Auto-Gain enable.
 {InEvents e;e.add(16,3.0);e.add(18,1.0);for(int i=0;i<N;++i)in[0][i]=in[1][i]=0.1f; if(!block(e))return 6;double v=0;params->get_value(p,16,&v);if(std::abs(v-3.0)>1e-6){std::printf("Output reset: %f\n",v);return 7;}}
 // Drive and Auto-Gain continue to run behind bypass; output converges to dry.
 {InEvents first;first.add(7,8.0);first.add(36,1.0);double err2=0,dry2=0;for(int b=0;b<500;++b){for(int i=0;i<N;++i){float x=0.18f*std::sin(2*3.141592653589793*220.0*(sample+i)/48000.0);in[0][i]=in[1][i]=x;}InEvents empty;if(!block(b==0?first:empty))return 8;if(b>100){for(int i=0;i<N;++i){double e=out[0][i]-in[0][i];err2+=e*e;dry2+=in[0][i]*in[0][i];}}}double corr=0;params->get_value(p,19,&corr);double rel=std::sqrt(err2/std::max(dry2,1e-20));if(rel>1e-5){std::printf("Bypass mismatch %g\n",rel);return 9;}if(std::abs(corr)<0.05){std::printf("AutoGain did not measure in bypass %f\n",corr);return 10;}}
 // Stereo-linked gate: a strong left channel keeps a quiet right channel open.
 {InEvents e;e.add(36,0.0);e.add(18,0.0);e.add(7,0.0);e.add(16,0.0);e.add(1,10.0);double r2=0;for(int b=0;b<120;++b){for(int i=0;i<N;++i){double t=(sample+i)/48000.0;in[0][i]=0.12f*std::sin(2*3.141592653589793*110*t);in[1][i]=0.004f*std::sin(2*3.141592653589793*110*t);}InEvents empty;if(!block(b==0?e:empty))return 11;if(b>20)for(float y:out[1])r2+=y*y;}double rms=std::sqrt(r2/((120-21)*N));if(rms<0.0015){std::printf("Gate not stereo linked %g\n",rms);return 12;}}
 p->stop_processing(p);p->deactivate(p);p->destroy(p);entry->deinit();dlclose(lib);std::puts("feature validation: ok");return 0;
}
