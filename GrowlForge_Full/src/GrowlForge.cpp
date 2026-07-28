#include <clap/clap.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kParamCount = 10;

enum ParamId : clap_id { Input=0, Drive, Fuzz, Growl, Tight, Presence, Cab, Gate, Mix, Output };
struct ParamDef { clap_id id; const char* name; const char* module; double min, max, def; const char* unit; };
constexpr std::array<ParamDef,kParamCount> defs{{
 {Input,"Input","Gain",-24,24,0," dB"}, {Drive,"Drive","Amp",0,10,4.8,""},
 {Fuzz,"Fuzz","Amp",0,10,3.2,""}, {Growl,"Growl","Tone",0,10,6.3,""},
 {Tight,"Tight","Tone",0,10,6.8,""}, {Presence,"Presence","Tone",0,10,5.4,""},
 {Cab,"Cab Filter","Cab",0,10,7.0,""}, {Gate,"Gate","Dynamics",-90,-20,-58," dB"},
 {Mix,"Mix","Output",0,100,100," %"}, {Output,"Output","Output",-24,12,-5," dB"}
}};

inline double dbToGain(double db){ return std::pow(10.0,db/20.0); }
inline double clamp(double x,double a,double b){ return std::max(a,std::min(b,x)); }
inline float zap(float x){ return std::abs(x)<1.0e-20f?0.0f:x; }

struct OnePole {
 float z=0, a=0;
 void lowpass(double hz,double sr){ a=(float)std::exp(-2.0*kPi*hz/sr); }
 void reset(){z=0;}
 float lp(float x){ z=(1-a)*x+a*z; return zap(z); }
 float hp(float x){ return x-lp(x); }
};

struct ChannelDSP {
 OnePole inputHP, postLP, bassSplit, presenceLP;
 float gateEnv=0;
 void reset(){inputHP.reset();postLP.reset();bassSplit.reset();presenceLP.reset();gateEnv=0;}
};

struct GrowlForge {
 clap_plugin_t plugin{};
 const clap_host_t* host=nullptr;
 std::array<std::atomic<double>,kParamCount> p{};
 std::array<ChannelDSP,2> ch{};
 double sampleRate=48000;

 explicit GrowlForge(const clap_host_t* h):host(h){ for(size_t i=0;i<p.size();++i)p[i]=defs[i].def; }
 void configure(){
  double tight=p[Tight].load(); double cab=p[Cab].load();
  double hp=55.0+20.0*tight; double lp=12000.0-650.0*cab;
  for(auto &c:ch){c.inputHP.lowpass(hp,sampleRate);c.postLP.lowpass(clamp(lp,3500,14000),sampleRate);c.bassSplit.lowpass(420,sampleRate);c.presenceLP.lowpass(2300,sampleRate);}
 }
 float processSample(float in,int ci){
  auto &c=ch[ci];
  const double inGain=dbToGain(p[Input]); const double outGain=dbToGain(p[Output]);
  const double drive=p[Drive]/10.0, fuzz=p[Fuzz]/10.0, growl=p[Growl]/10.0, presence=p[Presence]/10.0;
  const double wet=p[Mix]/100.0, threshold=dbToGain(p[Gate]);
  float dry=in; float x=(float)(in*inGain);
  float ax=std::abs(x); float attack=(float)std::exp(-1.0/(0.0015*sampleRate)); float release=(float)std::exp(-1.0/(0.055*sampleRate));
  c.gateEnv = ax>c.gateEnv ? attack*c.gateEnv+(1-attack)*ax : release*c.gateEnv+(1-release)*ax;
  float gateGain=(float)clamp((c.gateEnv-threshold*0.35)/(threshold*0.65),0.0,1.0);
  x=c.inputHP.hp(x)*gateGain;
  float low=c.bassSplit.lp(x); float body=x-low;
  x=(float)(low*(0.78+0.30*growl)+body*(1.0+0.55*growl));
  double pre=1.0+22.0*drive;
  double asym = std::tanh(x*pre*(x>=0?1.08:0.88));
  double fuzzed = std::tanh(x*(6.0+46.0*fuzz));
  double shaped = asym*(1.0-0.42*fuzz)+fuzzed*(0.42*fuzz);
  shaped += 0.075*growl*std::tanh((x-low)*15.0);
  float presBand=(float)shaped-c.presenceLP.lp((float)shaped);
  float y=(float)shaped+presBand*(float)(-0.10+0.42*presence);
  y=c.postLP.lp(y);
  y=(float)((dry*(1.0-wet)+y*wet)*outGain);
  return (float)clamp(y,-1.2,1.2);
 }
};

GrowlForge* self(const clap_plugin_t* p){return static_cast<GrowlForge*>(p->plugin_data);} 
void handleEvents(GrowlForge* s,const clap_input_events_t* ev){
 if(!ev||!ev->size||!ev->get)return;
 for(uint32_t i=0;i<ev->size(ev);++i){auto*h=ev->get(ev,i);if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID||h->type!=CLAP_EVENT_PARAM_VALUE)continue;
  auto*v=reinterpret_cast<const clap_event_param_value_t*>(h); if(v->param_id<kParamCount)s->p[v->param_id]=clamp(v->value,defs[v->param_id].min,defs[v->param_id].max);
 }
 s->configure();
}

bool plugInit(const clap_plugin_t*){return true;}
void plugDestroy(const clap_plugin_t*p){delete self(p);}
bool plugActivate(const clap_plugin_t*p,double sr,uint32_t,uint32_t){auto*s=self(p);s->sampleRate=sr;for(auto&c:s->ch)c.reset();s->configure();return sr>1000;}
void plugDeactivate(const clap_plugin_t*){}
bool plugStart(const clap_plugin_t*){return true;}
void plugStop(const clap_plugin_t*){}
void plugReset(const clap_plugin_t*p){for(auto&c:self(p)->ch)c.reset();}
clap_process_status plugProcess(const clap_plugin_t*p,const clap_process_t*pr){
 auto*s=self(p);handleEvents(s,pr->in_events); if(!pr||pr->audio_inputs_count<1||pr->audio_outputs_count<1)return CLAP_PROCESS_CONTINUE;
 auto&in=pr->audio_inputs[0];auto&out=pr->audio_outputs[0];uint32_t channels=std::min({in.channel_count,out.channel_count,2u});
 for(uint32_t c=0;c<channels;++c){if(!in.data32||!out.data32||!in.data32[c]||!out.data32[c])continue;for(uint32_t n=0;n<pr->frames_count;++n)out.data32[c][n]=s->processSample(in.data32[c][n],(int)c);}
 return CLAP_PROCESS_CONTINUE;
}

uint32_t audioCount(const clap_plugin_t*,bool){return 1;}
bool audioGet(const clap_plugin_t*,uint32_t index,bool isInput,clap_audio_port_info_t*info){if(index||!info)return false;std::memset(info,0,sizeof(*info));info->id=isInput?0:1;std::snprintf(info->name,sizeof(info->name),"%s",isInput?"Stereo Input":"Stereo Output");info->flags=CLAP_AUDIO_PORT_IS_MAIN;info->channel_count=2;info->port_type=CLAP_PORT_STEREO;info->in_place_pair=isInput?1:0;return true;}
const clap_plugin_audio_ports_t audioExt{audioCount,audioGet};

uint32_t paramCount(const clap_plugin_t*){return kParamCount;}
bool paramInfo(const clap_plugin_t*,uint32_t i,clap_param_info_t*o){if(i>=kParamCount||!o)return false;auto&d=defs[i];std::memset(o,0,sizeof(*o));o->id=d.id;o->flags=CLAP_PARAM_IS_AUTOMATABLE;o->min_value=d.min;o->max_value=d.max;o->default_value=d.def;std::snprintf(o->name,sizeof(o->name),"%s",d.name);std::snprintf(o->module,sizeof(o->module),"%s",d.module);return true;}
bool paramValue(const clap_plugin_t*p,clap_id id,double*v){if(id>=kParamCount||!v)return false;*v=self(p)->p[id];return true;}
bool valueText(const clap_plugin_t*,clap_id id,double v,char*dst,uint32_t n){if(id>=kParamCount||!dst||!n)return false;std::snprintf(dst,n,"%.2f%s",v,defs[id].unit);return true;}
bool textValue(const clap_plugin_t*,clap_id id,const char*t,double*v){if(id>=kParamCount||!t||!v)return false;char*e=nullptr;double x=std::strtod(t,&e);if(e==t)return false;*v=clamp(x,defs[id].min,defs[id].max);return true;}
void paramFlush(const clap_plugin_t*p,const clap_input_events_t*in,const clap_output_events_t*){handleEvents(self(p),in);}
const clap_plugin_params_t paramsExt{paramCount,paramInfo,paramValue,valueText,textValue,paramFlush};

struct StateBlob{uint32_t magic=0x47465247,version=1;double values[kParamCount]{};};
bool stateSave(const clap_plugin_t*p,const clap_ostream_t*s){if(!s||!s->write)return false;StateBlob b;for(size_t i=0;i<kParamCount;++i)b.values[i]=self(p)->p[i];return s->write(s,&b,sizeof(b))==(int64_t)sizeof(b);}
bool stateLoad(const clap_plugin_t*p,const clap_istream_t*s){if(!s||!s->read)return false;StateBlob b;if(s->read(s,&b,sizeof(b))!=(int64_t)sizeof(b)||b.magic!=0x47465247)return false;for(size_t i=0;i<kParamCount;++i)self(p)->p[i]=clamp(b.values[i],defs[i].min,defs[i].max);self(p)->configure();return true;}
const clap_plugin_state_t stateExt{stateSave,stateLoad};

const void* plugExtension(const clap_plugin_t*,const char*id){if(!id)return nullptr;if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audioExt;if(!std::strcmp(id,CLAP_EXT_PARAMS))return &paramsExt;if(!std::strcmp(id,CLAP_EXT_STATE))return &stateExt;return nullptr;}
void plugMain(const clap_plugin_t*){}

const char* features[]={CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,CLAP_PLUGIN_FEATURE_DISTORTION,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t desc{CLAP_VERSION,"audio.growlforge.effect","GrowlForge","OpenAI / User Project","","","","1.0.0","Tight modern high-gain growl, restrained fuzz and cabinet-style filtering.",features};
uint32_t factoryCount(const clap_plugin_factory_t*){return 1;}
const clap_plugin_descriptor_t* factoryDesc(const clap_plugin_factory_t*,uint32_t i){return i==0?&desc:nullptr;}
const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*,const clap_host_t*h,const char*id){if(!id||std::strcmp(id,desc.id))return nullptr;auto*s=new GrowlForge(h);s->plugin={&desc,s,plugInit,plugDestroy,plugActivate,plugDeactivate,plugStart,plugStop,plugReset,plugProcess,plugExtension,plugMain};return &s->plugin;}
const clap_plugin_factory_t factory{factoryCount,factoryDesc,factoryCreate};
bool entryInit(const char*){return true;} void entryDeinit(){} const void* entryFactory(const char*id){return id&&!std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr;}
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION,entryInit,entryDeinit,entryFactory};
