#include <clap/clap.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kParamCount = 18;
constexpr int kOversample = 4;

enum ParamId : clap_id {
 Input=0, Gate, Tight, Punch, Body, Mass, Growl, Drive, Grind, Fuzz,
 Bite, Presence, Air, Smooth, PreCab, Mix, Output, Ceiling
};

struct ParamDef {
 clap_id id;
 const char* name;
 const char* module;
 double min, max, def;
 const char* unit;
};

constexpr std::array<ParamDef,kParamCount> defs{{
 {Input,"Input Trim","Gain",-24,12,-6," dB"},
 {Gate,"Gate","Dynamics",-90,-20,-65," dB"},
 {Tight,"Tight","Low End",0,10,5.5,""},
 {Punch,"Punch","Low Mids",0,10,5.0,""},
 {Body,"Body","Low Mids",0,10,5.0,""},
 {Mass,"Mass","Low End",0,10,4.5,""},
 {Growl,"Growl","Mids",0,10,5.5,""},
 {Drive,"Drive","Saturation",0,10,2.5,""},
 {Grind,"Grind","Saturation",0,10,3.0,""},
 {Fuzz,"Fuzz","Saturation",0,10,1.5,""},
 {Bite,"Bite","High Mids",0,10,4.0,""},
 {Presence,"Presence","Highs",0,10,4.0,""},
 {Air,"Air","Highs",0,10,3.0,""},
 {Smooth,"Smooth","Anti-Alias",0,10,6.0,""},
 {PreCab,"Pre-Cab Filter","Anti-Alias",0,10,2.5,""},
 {Mix,"Mix","Output",0,100,100," %"},
 {Output,"Output","Output",-24,6,0," dB"},
 {Ceiling,"Ceiling","Output",-12,0,-1," dB"}
}};

inline double dbToGain(double db){ return std::pow(10.0,db/20.0); }
inline double clamp(double x,double a,double b){ return std::max(a,std::min(b,x)); }
inline float zap(float x){ return std::abs(x)<1.0e-20f?0.0f:x; }

struct OnePole {
 float z=0.0f;
 float a=0.0f;
 void setLowpass(double hz,double sr){
  hz=clamp(hz,10.0,sr*0.45);
  a=(float)std::exp(-2.0*kPi*hz/sr);
 }
 void reset(){z=0.0f;}
 float lp(float x){z=(1.0f-a)*x+a*z;return zap(z);}
 float hp(float x){return x-lp(x);}
};

struct ChannelDSP {
 OnePole inputHP;
 OnePole low180;
 OnePole low650;
 OnePole low1600;
 OnePole presenceLP;
 OnePole airLP;
 std::array<OnePole,4> antiAlias;
 std::array<OnePole,2> postLP;
 float gateEnv=0.0f;
 float previousShaped=0.0f;

 void reset(){
  inputHP.reset(); low180.reset(); low650.reset(); low1600.reset();
  presenceLP.reset(); airLP.reset();
  for(auto&f:antiAlias)f.reset();
  for(auto&f:postLP)f.reset();
  gateEnv=0.0f;
  previousShaped=0.0f;
 }
};

struct GrowlForge {
 clap_plugin_t plugin{};
 const clap_host_t* host=nullptr;
 std::array<std::atomic<double>,kParamCount> p{};
 std::array<ChannelDSP,2> ch{};
 double sampleRate=48000.0;

 explicit GrowlForge(const clap_host_t* h):host(h){
  for(size_t i=0;i<p.size();++i)p[i]=defs[i].def;
 }

 void configure(){
  const double tight=p[Tight].load();
  const double smooth=p[Smooth].load();
  const double preCab=p[PreCab].load();

  const double hpHz=38.0+24.0*tight;
  const double osRate=sampleRate*kOversample;

  // Internal oversampling filter. Smooth raises damping without turning the
  // plug-in into a dull low-pass by itself.
  const double aaCut=clamp(
    sampleRate*(0.44-0.020*smooth),
    9000.0,
    std::min(19000.0,sampleRate*0.44)
  );

  // Pre-cab filter is intentionally mild at low values because this plug-in
  // is designed to sit before a separate cabinet or IR loader.
  const double postCut=clamp(
    20500.0-1250.0*preCab-430.0*smooth,
    5500.0,
    std::min(19500.0,sampleRate*0.45)
  );

  for(auto&c:ch){
   c.inputHP.setLowpass(hpHz,sampleRate);
   c.low180.setLowpass(180.0,sampleRate);
   c.low650.setLowpass(650.0,sampleRate);
   c.low1600.setLowpass(1600.0,sampleRate);
   c.presenceLP.setLowpass(2600.0,sampleRate);
   c.airLP.setLowpass(6500.0,sampleRate);
   for(auto&f:c.antiAlias)f.setLowpass(aaCut,osRate);
   for(auto&f:c.postLP)f.setLowpass(postCut,sampleRate);
  }
 }

 float nonlinearOversampled(float x, ChannelDSP& c){
  const double drive=p[Drive].load()/10.0;
  const double grind=p[Grind].load()/10.0;
  const double fuzz=p[Fuzz].load()/10.0;

  const double pre=1.0+13.0*drive+7.0*grind;
  const double asymPos=1.0+0.34*grind;
  const double asymNeg=1.0-0.22*grind;
  const double fuzzGain=5.0+58.0*fuzz;

  float out=0.0f;
  const float start=c.previousShaped;
  for(int i=1;i<=kOversample;++i){
   const float t=(float)i/(float)kOversample;
   const float u=start+(x-start)*t;

   const double mainSat=std::tanh(u*pre*(u>=0.0f?asymPos:asymNeg));
   const double fuzzSat=std::tanh(u*fuzzGain);
   const double fuzzMix=0.52*fuzz;
   double y=mainSat*(1.0-fuzzMix)+fuzzSat*fuzzMix;

   // Drive compensation keeps this usable after an already loud amp stage.
   const double makeup=1.0/std::sqrt(1.0+0.62*(pre-1.0));
   y*=makeup;

   float filtered=(float)y;
   for(auto&f:c.antiAlias)filtered=f.lp(filtered);
   out=filtered;
  }
  c.previousShaped=x;
  return out;
 }

 float processSample(float in,int ci){
  auto&c=ch[ci];

  const double inputGain=dbToGain(p[Input].load());
  const double outputGain=dbToGain(p[Output].load());
  const double ceilingGain=dbToGain(p[Ceiling].load());

  const double tight=p[Tight].load()/10.0;
  const double punch=p[Punch].load()/10.0;
  const double body=p[Body].load()/10.0;
  const double mass=p[Mass].load()/10.0;
  const double growl=p[Growl].load()/10.0;
  const double bite=p[Bite].load()/10.0;
  const double presence=p[Presence].load()/10.0;
  const double air=p[Air].load()/10.0;
  const double wet=p[Mix].load()/100.0;
  const double threshold=dbToGain(p[Gate].load());

  const float dry=in;
  float x=(float)(in*inputGain);

  const float ax=std::abs(x);
  const float attack=(float)std::exp(-1.0/(0.0012*sampleRate));
  const float release=(float)std::exp(-1.0/(0.070*sampleRate));
  c.gateEnv=ax>c.gateEnv
    ? attack*c.gateEnv+(1.0f-attack)*ax
    : release*c.gateEnv+(1.0f-release)*ax;

  const float gateGain=(float)clamp(
    (c.gateEnv-threshold*0.30)/(threshold*0.70),
    0.0,1.0
  );

  x=c.inputHP.hp(x)*gateGain;

  const float low=c.low180.lp(x);
  const float lowMid=c.low650.lp(x)-low;
  const float growlBand=c.low1600.lp(x)-c.low650.z;
  const float high=x-c.low1600.z;

  // Independent tone dimensions:
  // Tight reduces loose lows, Mass restores controlled deep weight,
  // Punch and Body shape different low-mid regions, Growl adds focused mids.
  const float lowGain=(float)(0.38+1.30*mass-0.55*tight);
  const float punchGain=(float)(0.42+1.45*punch);
  const float bodyGain=(float)(0.45+1.30*body);
  const float growlGain=(float)(0.32+1.85*growl);
  const float highGain=(float)(0.60+0.75*bite);

  float shaped=
    low*lowGain+
    lowMid*(0.68f*punchGain+0.62f*bodyGain)+
    growlBand*growlGain+
    high*highGain;

  float y=nonlinearOversampled(shaped,c);

  const float presenceBand=y-c.presenceLP.lp(y);
  const float airBand=y-c.airLP.lp(y);

  // Bite emphasizes upper mids before the final smoothing, Presence is wider,
  // and Air is deliberately restrained to avoid reintroducing fizz.
  y += presenceBand*(float)(-0.22+0.78*presence+0.58*bite);
  y += airBand*(float)(-0.12+0.42*air);

  for(auto&f:c.postLP)y=f.lp(y);

  y=(float)((dry*(1.0-wet)+y*wet)*outputGain);

  // Smooth bounded ceiling. Below the ceiling it is nearly transparent.
  const double normalized=y/std::max(ceilingGain,1.0e-6);
  const double limited=std::tanh(normalized*1.35)/std::tanh(1.35);
  y=(float)(limited*ceilingGain);

  return (float)clamp(y,-1.05,1.05);
 }
};

GrowlForge* self(const clap_plugin_t* p){
 return static_cast<GrowlForge*>(p->plugin_data);
}

void handleEvents(GrowlForge* s,const clap_input_events_t* ev){
 if(!ev||!ev->size||!ev->get)return;
 for(uint32_t i=0;i<ev->size(ev);++i){
  auto*h=ev->get(ev,i);
  if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID||h->type!=CLAP_EVENT_PARAM_VALUE)continue;
  auto*v=reinterpret_cast<const clap_event_param_value_t*>(h);
  if(v->param_id<kParamCount){
   s->p[v->param_id]=clamp(v->value,defs[v->param_id].min,defs[v->param_id].max);
  }
 }
 s->configure();
}

bool plugInit(const clap_plugin_t*){return true;}
void plugDestroy(const clap_plugin_t*p){delete self(p);}
bool plugActivate(const clap_plugin_t*p,double sr,uint32_t,uint32_t){
 auto*s=self(p);
 if(sr<=1000.0)return false;
 s->sampleRate=sr;
 for(auto&c:s->ch)c.reset();
 s->configure();
 return true;
}
void plugDeactivate(const clap_plugin_t*){}
bool plugStart(const clap_plugin_t*){return true;}
void plugStop(const clap_plugin_t*){}
void plugReset(const clap_plugin_t*p){for(auto&c:self(p)->ch)c.reset();}

clap_process_status plugProcess(const clap_plugin_t*p,const clap_process_t*pr){
 if(!pr)return CLAP_PROCESS_ERROR;
 auto*s=self(p);
 handleEvents(s,pr->in_events);
 if(pr->audio_inputs_count<1||pr->audio_outputs_count<1)return CLAP_PROCESS_CONTINUE;
 auto&in=pr->audio_inputs[0];
 auto&out=pr->audio_outputs[0];
 const uint32_t channels=std::min({in.channel_count,out.channel_count,2u});
 for(uint32_t c=0;c<channels;++c){
  if(!in.data32||!out.data32||!in.data32[c]||!out.data32[c])continue;
  for(uint32_t n=0;n<pr->frames_count;++n){
   out.data32[c][n]=s->processSample(in.data32[c][n],(int)c);
  }
 }
 return CLAP_PROCESS_CONTINUE;
}

uint32_t audioCount(const clap_plugin_t*,bool){return 1;}
bool audioGet(const clap_plugin_t*,uint32_t index,bool isInput,clap_audio_port_info_t*info){
 if(index||!info)return false;
 std::memset(info,0,sizeof(*info));
 info->id=isInput?0:1;
 std::snprintf(info->name,sizeof(info->name),"%s",isInput?"Stereo Input":"Stereo Output");
 info->flags=CLAP_AUDIO_PORT_IS_MAIN;
 info->channel_count=2;
 info->port_type=CLAP_PORT_STEREO;
 info->in_place_pair=isInput?1:0;
 return true;
}
const clap_plugin_audio_ports_t audioExt{audioCount,audioGet};

uint32_t paramCount(const clap_plugin_t*){return kParamCount;}
bool paramInfo(const clap_plugin_t*,uint32_t i,clap_param_info_t*o){
 if(i>=kParamCount||!o)return false;
 auto&d=defs[i];
 std::memset(o,0,sizeof(*o));
 o->id=d.id;
 o->flags=CLAP_PARAM_IS_AUTOMATABLE;
 o->min_value=d.min;
 o->max_value=d.max;
 o->default_value=d.def;
 std::snprintf(o->name,sizeof(o->name),"%s",d.name);
 std::snprintf(o->module,sizeof(o->module),"%s",d.module);
 return true;
}
bool paramValue(const clap_plugin_t*p,clap_id id,double*v){
 if(id>=kParamCount||!v)return false;
 *v=self(p)->p[id];
 return true;
}
bool valueText(const clap_plugin_t*,clap_id id,double v,char*dst,uint32_t n){
 if(id>=kParamCount||!dst||!n)return false;
 std::snprintf(dst,n,"%.2f%s",v,defs[id].unit);
 return true;
}
bool textValue(const clap_plugin_t*,clap_id id,const char*t,double*v){
 if(id>=kParamCount||!t||!v)return false;
 char*e=nullptr;
 const double x=std::strtod(t,&e);
 if(e==t)return false;
 *v=clamp(x,defs[id].min,defs[id].max);
 return true;
}
void paramFlush(const clap_plugin_t*p,const clap_input_events_t*in,const clap_output_events_t*){
 handleEvents(self(p),in);
}
const clap_plugin_params_t paramsExt{
 paramCount,paramInfo,paramValue,valueText,textValue,paramFlush
};

struct StateBlob{
 uint32_t magic=0x47465247;
 uint32_t version=2;
 double values[kParamCount]{};
};
bool stateSave(const clap_plugin_t*p,const clap_ostream_t*s){
 if(!s||!s->write)return false;
 StateBlob b;
 for(size_t i=0;i<kParamCount;++i)b.values[i]=self(p)->p[i];
 return s->write(s,&b,sizeof(b))==(int64_t)sizeof(b);
}
bool stateLoad(const clap_plugin_t*p,const clap_istream_t*s){
 if(!s||!s->read)return false;
 StateBlob b;
 if(s->read(s,&b,sizeof(b))!=(int64_t)sizeof(b)||b.magic!=0x47465247||b.version!=2)return false;
 for(size_t i=0;i<kParamCount;++i){
  self(p)->p[i]=clamp(b.values[i],defs[i].min,defs[i].max);
 }
 self(p)->configure();
 return true;
}
const clap_plugin_state_t stateExt{stateSave,stateLoad};

const void* plugExtension(const clap_plugin_t*,const char*id){
 if(!id)return nullptr;
 if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audioExt;
 if(!std::strcmp(id,CLAP_EXT_PARAMS))return &paramsExt;
 if(!std::strcmp(id,CLAP_EXT_STATE))return &stateExt;
 return nullptr;
}
void plugMain(const clap_plugin_t*){}

const char* features[]={
 CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
 CLAP_PLUGIN_FEATURE_DISTORTION,
 CLAP_PLUGIN_FEATURE_STEREO,
 nullptr
};

const clap_plugin_descriptor_t desc{
 CLAP_VERSION,
 "audio.growlforge.effect",
 "GrowlForge",
 "OpenAI / User Project",
 "",
 "",
 "",
 "1.1.1",
 "Post-amp guitar tone sculptor with high-impact controls, oversampled saturation and controlled output.",
 features
};

uint32_t factoryCount(const clap_plugin_factory_t*){return 1;}
const clap_plugin_descriptor_t* factoryDesc(const clap_plugin_factory_t*,uint32_t i){
 return i==0?&desc:nullptr;
}
const clap_plugin_t* factoryCreate(
 const clap_plugin_factory_t*,const clap_host_t*h,const char*id
){
 if(!id||std::strcmp(id,desc.id))return nullptr;
 auto*s=new GrowlForge(h);
 s->plugin={
  &desc,s,plugInit,plugDestroy,plugActivate,plugDeactivate,
  plugStart,plugStop,plugReset,plugProcess,plugExtension,plugMain
 };
 return &s->plugin;
}
const clap_plugin_factory_t factory{factoryCount,factoryDesc,factoryCreate};

bool entryInit(const char*){return true;}
void entryDeinit(){}
const void* entryFactory(const char*id){
 return id&&!std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr;
}
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
 CLAP_VERSION,entryInit,entryDeinit,entryFactory
};
