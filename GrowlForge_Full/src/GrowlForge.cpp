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
constexpr uint32_t kParamCount = 21;
constexpr int kOversample = 4;

enum ParamId : clap_id {
 Input=0, Gate, Tight, Punch, Body, Mass, Growl, Drive, Grind, Fuzz,
 Bite, Presence, Air, Smooth, PreCab, ParallelDry, Output, Ceiling, AutoGain,
 AutoGainCorrection, ApplyAutoGain
};

struct ParamDef {
 clap_id id; const char* name; const char* module;
 double min,max,def; const char* unit; uint32_t flags;
};

constexpr uint32_t kAuto = CLAP_PARAM_IS_AUTOMATABLE;
constexpr uint32_t kToggle = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
constexpr uint32_t kReadOnly = CLAP_PARAM_IS_READONLY;

constexpr std::array<ParamDef,kParamCount> defs{{
 {Input,"Input Trim","Gain",-12,12,0," dB",kAuto},
 {Gate,"Gate","Dynamics",0,10,0,"",kAuto},
 {Tight,"Tight","Enhancers",0,10,0,"",kAuto},
 {Punch,"Punch","Enhancers",0,10,0,"",kAuto},
 {Body,"Body","Enhancers",0,10,0,"",kAuto},
 {Mass,"Mass","Enhancers",0,10,0,"",kAuto},
 {Growl,"Growl","Enhancers",0,10,0,"",kAuto},
 {Drive,"Drive","Enhancers",0,10,0,"",kAuto},
 {Grind,"Grind","Enhancers",0,10,0,"",kAuto},
 {Fuzz,"Fuzz","Enhancers",0,10,0,"",kAuto},
 {Bite,"Bite","Enhancers",0,10,0,"",kAuto},
 {Presence,"Presence","Enhancers",0,10,0,"",kAuto},
 {Air,"Air","Enhancers",0,10,0,"",kAuto},
 {Smooth,"Smooth","Enhancers",0,10,0,"",kAuto},
 {PreCab,"Pre-Cab Filter","Enhancers",0,10,0,"",kAuto},
 {ParallelDry,"Parallel Dry","Output",0,100,0," %",kAuto},
 {Output,"Output","Output",-12,12,0," dB",kAuto},
 {Ceiling,"Ceiling","Output",-12,0,0," dB",kAuto},
 {AutoGain,"Auto-Gain","Output",0,1,0,"",kToggle},
 {AutoGainCorrection,"Auto-Gain Correction","Output",-12,12,0," dB",kReadOnly},
 {ApplyAutoGain,"Apply Auto-Gain","Output",0,1,0,"",kToggle}
}};

inline double dbToGain(double db){return std::pow(10.0,db/20.0);}
inline double clamp(double x,double a,double b){return std::max(a,std::min(b,x));}
inline double gainToDb(double gain){return 20.0*std::log10(std::max(gain,1.0e-9));}
inline double quantize01(double value){return std::round(value*10.0)/10.0;}
inline float zap(float x){return std::abs(x)<1e-20f?0.0f:x;}

struct OnePole{
 float z=0,a=0;
 void setLowpass(double hz,double sr){hz=clamp(hz,10.0,sr*0.45);a=(float)std::exp(-2.0*kPi*hz/sr);}
 void reset(){z=0;}
 float lp(float x){z=(1-a)*x+a*z;return zap(z);}
 float hp(float x){return x-lp(x);}
};

struct ChannelDSP{
 OnePole tightHP,low180,low650,low1600,presenceLP,airLP,fuzzLow;
 std::array<OnePole,4> antiAlias;
 std::array<OnePole,2> postLP;
 float gateEnv=0,previousInput=0;
 double dryRms2=1e-8,wetRms2=1e-8,autoGain=1.0;
 void reset(){
  tightHP.reset();low180.reset();low650.reset();low1600.reset();
  presenceLP.reset();airLP.reset();fuzzLow.reset();
  for(auto&f:antiAlias)f.reset();for(auto&f:postLP)f.reset();
  gateEnv=previousInput=0;dryRms2=wetRms2=1e-8;autoGain=1.0;
 }
};

struct GrowlForge{
 clap_plugin_t plugin{}; const clap_host_t* host=nullptr;
 std::array<std::atomic<double>,kParamCount> p{};
 std::array<ChannelDSP,2> ch{}; double sampleRate=48000.0;

 explicit GrowlForge(const clap_host_t*h):host(h){for(size_t i=0;i<p.size();++i)p[i]=defs[i].def;}

 bool enhancersZero()const{
  for(clap_id id=Tight;id<=PreCab;++id)if(p[id].load()>1e-9)return false;
  return true;
 }


 double currentAutoGainDb() const {
  const double average=0.5*(ch[0].autoGain+ch[1].autoGain);
  return clamp(gainToDb(average),-12.0,12.0);
 }

 void resetAutoGainMeasurement(){
  for(auto&c:ch){
   c.dryRms2=1.0e-8;
   c.wetRms2=1.0e-8;
   c.autoGain=1.0;
  }
 }

 void applyCurrentAutoGain(){
  // Auto-Gain is measured before Output. Commit the current absolute
  // correction to Output once, then clear all measurement history.
  const double correction=quantize01(currentAutoGainDb());
  p[Output]=quantize01(clamp(correction,defs[Output].min,defs[Output].max));
  p[AutoGain]=0.0;
  p[ApplyAutoGain]=0.0;
  resetAutoGainMeasurement();
 }

 void configure(){
  double tight=p[Tight].load()/10.0,smooth=p[Smooth].load()/10.0,preCab=p[PreCab].load()/10.0;
  double osRate=sampleRate*kOversample;
  double hpHz=45.0+115.0*tight;
  double aaCut=clamp(sampleRate*(0.46-0.23*smooth),5200.0,std::min(20500.0,sampleRate*0.46));
  double openCut=std::min(21000.0,sampleRate*0.46),closedCut=2600.0;
  double postCut=openCut*std::pow(closedCut/openCut,preCab);
  for(auto&c:ch){
   c.tightHP.setLowpass(hpHz,sampleRate);c.low180.setLowpass(180,sampleRate);
   c.low650.setLowpass(650,sampleRate);c.low1600.setLowpass(1600,sampleRate);
   c.presenceLP.setLowpass(2600,sampleRate);c.airLP.setLowpass(6500,sampleRate);
   for(auto&f:c.antiAlias)f.setLowpass(aaCut,osRate);
   c.fuzzLow.setLowpass(260,osRate);for(auto&f:c.postLP)f.setLowpass(postCut,sampleRate);
  }
 }

 float nonlinear(float x,float low,float growlBand,float high,ChannelDSP&c){
  double mass=p[Mass].load()/10.0,growl=p[Growl].load()/10.0,drive=p[Drive].load()/10.0;
  double grind=p[Grind].load()/10.0,fuzz=p[Fuzz].load()/10.0,bite=p[Bite].load()/10.0;
  if(mass+growl+drive+grind+fuzz+bite<=0){c.previousInput=x;return x;}

  double massH=1.65*mass*low*std::abs((double)low);
  double growlH=3.10*growl*growlBand*std::abs((double)growlBand);
  double biteH=1.45*bite*high*high*high;
  float target=(float)(x+massH+growlH+biteH);

  double pre=1+16*drive+9*grind+2.5*growl+1.5*bite;
  double pos=1+0.42*grind+0.18*growl,neg=1-0.28*grind,fuzzGain=5+58*fuzz;
  float out=0,start=c.previousInput;

  for(int i=1;i<=kOversample;++i){
   float t=(float)i/kOversample,u=start+(target-start)*t;
   double amount=clamp(0.82*drive+0.62*grind+0.32*growl+0.22*bite+0.18*mass,0.0,1.0);
   double sat=std::tanh(u*pre*(u>=0?pos:neg));
   double main=u*(1-amount)+sat*amount;

   float fl=c.fuzzLow.lp(u),fs=u-0.72f*fl;
   double sustain=clamp((std::abs((double)u)-0.015)/0.18,0.0,1.0);
   double fm=fuzz*(0.12+0.34*sustain);
   double intelligent=0.86*std::tanh(fs*fuzzGain)+0.14*std::tanh(fl*(1+4*drive));
   double y=(main*(1-fm)+intelligent*fm)/std::sqrt(1+0.50*(pre-1)*amount);

   float filtered=(float)y;for(auto&f:c.antiAlias)filtered=f.lp(filtered);out=filtered;
  }
  c.previousInput=target;return out;
 }

 float applyAutoGain(float dry,float wet,ChannelDSP&c){
  if(p[AutoGain].load()<0.5){c.autoGain+=(1-c.autoGain)*0.002;return wet;}
  double rc=std::exp(-1.0/(0.300*sampleRate));
  c.dryRms2=rc*c.dryRms2+(1-rc)*dry*dry;c.wetRms2=rc*c.wetRms2+(1-rc)*wet*wet;
  double target=std::sqrt((c.dryRms2+1e-8)/(c.wetRms2+1e-8));
  target=clamp(target,dbToGain(-12),dbToGain(12));
  double gc=std::exp(-1.0/(0.180*sampleRate));
  c.autoGain=gc*c.autoGain+(1-gc)*target;
  return (float)(wet*c.autoGain);
 }

 float processSample(float in,int ci){
  auto&c=ch[ci];
  double inputDb=p[Input].load(),outputDb=p[Output].load(),gate=p[Gate].load()/10.0;
  double tight=p[Tight].load()/10.0,punch=p[Punch].load()/10.0,body=p[Body].load()/10.0;
  double mass=p[Mass].load()/10.0,growl=p[Growl].load()/10.0,bite=p[Bite].load()/10.0;
  double presence=p[Presence].load()/10.0,air=p[Air].load()/10.0,smooth=p[Smooth].load()/10.0;
  double preCab=p[PreCab].load()/10.0,parallel=p[ParallelDry].load()/100.0;
  float original=in,x=(float)(in*dbToGain(inputDb));

  if(gate>0){
   double th=dbToGain(-90+58*gate);float ax=std::abs(x);
   float a=(float)std::exp(-1.0/(0.0012*sampleRate)),r=(float)std::exp(-1.0/(0.070*sampleRate));
   c.gateEnv=ax>c.gateEnv?a*c.gateEnv+(1-a)*ax:r*c.gateEnv+(1-r)*ax;
   x*=(float)clamp((c.gateEnv-th*0.30)/(th*0.70),0.0,1.0);
  }

  if(enhancersZero()&&gate==0&&inputDb==0&&outputDb==0&&p[Ceiling].load()==0&&
     p[AutoGain].load()<0.5&&parallel==0)return original;

  if(tight>0){float hp=c.tightHP.hp(x);x=x*(float)(1-tight)+hp*(float)tight;}

  float low=c.low180.lp(x),l650=c.low650.lp(x),l1600=c.low1600.lp(x);
  float lowMid=l650-low,growlBand=l1600-l650,high=x-l1600;
  float shaped=
   low*(float)(1+0.95*mass)+
   lowMid*(float)(0.58*(1+1.25*punch)+0.42*(1+1.18*body))+
   growlBand*(float)(1+1.65*growl)+
   high*(float)(1+0.70*bite);

  float y=nonlinear(shaped,low,growlBand,high,c);
  float presenceBand=y-c.presenceLP.lp(y),airBand=y-c.airLP.lp(y);
  y+=presenceBand*(float)(0.95*presence+0.72*bite);
  y+=airBand*(float)(0.78*air);

  if(smooth>0){y-=airBand*(float)(0.88*smooth);y-=presenceBand*(float)(0.24*smooth);}
  if(preCab>0)for(auto&f:c.postLP)y=f.lp(y);

  y=applyAutoGain(x,y,c);
  y=(float)(y*(1-parallel)+x*parallel);
  y=(float)(y*dbToGain(outputDb));

  double ceiling=p[Ceiling].load();
  if(ceiling<0){
   double cg=dbToGain(ceiling),n=y/std::max(cg,1e-6);
   y=(float)(std::tanh(n*1.35)/std::tanh(1.35)*cg);
  }
  return (float)clamp(y,-4.0,4.0);
 }
};

GrowlForge*self(const clap_plugin_t*p){return static_cast<GrowlForge*>(p->plugin_data);}

void handleEvents(GrowlForge*s,const clap_input_events_t*ev){
 if(!ev||!ev->size||!ev->get)return;
 bool changed=false;
 for(uint32_t i=0;i<ev->size(ev);++i){
  auto*h=ev->get(ev,i);
  if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID||h->type!=CLAP_EVENT_PARAM_VALUE)continue;
  auto*v=reinterpret_cast<const clap_event_param_value_t*>(h);
  if(v->param_id>=kParamCount||v->param_id==AutoGainCorrection)continue;

  double x=clamp(v->value,defs[v->param_id].min,defs[v->param_id].max);
  if(v->param_id==AutoGain||v->param_id==ApplyAutoGain){
   x=x>=0.5?1.0:0.0;
  }else{
   x=quantize01(x);
  }

  if(v->param_id==ApplyAutoGain&&x>=0.5){
   s->applyCurrentAutoGain();
   changed=true;
   continue;
  }

  if(v->param_id==AutoGain){
   const double previous=s->p[AutoGain].load();
   s->p[AutoGain]=x;

   if(previous!=x){
    // Auto-Gain must measure from a neutral output reference.
    // Enabling it clears any previously committed/manual Output gain.
    if(x>=0.5)s->p[Output]=0.0;
    s->resetAutoGainMeasurement();
   }

   changed=true;
   continue;
  }

  s->p[v->param_id]=x;
  changed=true;
 }
 if(changed)s->configure();
}

bool plugInit(const clap_plugin_t*){return true;}
void plugDestroy(const clap_plugin_t*p){delete self(p);}
bool plugActivate(const clap_plugin_t*p,double sr,uint32_t,uint32_t){
 auto*s=self(p);if(sr<=1000)return false;s->sampleRate=sr;for(auto&c:s->ch)c.reset();s->configure();return true;
}
void plugDeactivate(const clap_plugin_t*){} bool plugStart(const clap_plugin_t*){return true;}
void plugStop(const clap_plugin_t*){} void plugReset(const clap_plugin_t*p){for(auto&c:self(p)->ch)c.reset();}

clap_process_status plugProcess(const clap_plugin_t*p,const clap_process_t*pr){
 if(!pr)return CLAP_PROCESS_ERROR;auto*s=self(p);handleEvents(s,pr->in_events);
 if(pr->audio_inputs_count<1||pr->audio_outputs_count<1)return CLAP_PROCESS_CONTINUE;
 auto&in=pr->audio_inputs[0];auto&out=pr->audio_outputs[0];
 uint32_t channels=std::min({in.channel_count,out.channel_count,2u});
 for(uint32_t c=0;c<channels;++c){
  if(!in.data32||!out.data32||!in.data32[c]||!out.data32[c])continue;
  for(uint32_t n=0;n<pr->frames_count;++n)out.data32[c][n]=s->processSample(in.data32[c][n],(int)c);
 }
 return CLAP_PROCESS_CONTINUE;
}

uint32_t audioCount(const clap_plugin_t*,bool){return 1;}
bool audioGet(const clap_plugin_t*,uint32_t i,bool input,clap_audio_port_info_t*o){
 if(i||!o)return false;std::memset(o,0,sizeof(*o));o->id=input?0:1;
 std::snprintf(o->name,sizeof(o->name),"%s",input?"Stereo Input":"Stereo Output");
 o->flags=CLAP_AUDIO_PORT_IS_MAIN;o->channel_count=2;o->port_type=CLAP_PORT_STEREO;o->in_place_pair=input?1:0;return true;
}
const clap_plugin_audio_ports_t audioExt{audioCount,audioGet};

uint32_t paramCount(const clap_plugin_t*){return kParamCount;}
bool paramInfo(const clap_plugin_t*,uint32_t i,clap_param_info_t*o){
 if(i>=kParamCount||!o)return false;auto&d=defs[i];std::memset(o,0,sizeof(*o));
 o->id=d.id;o->flags=d.flags;o->min_value=d.min;o->max_value=d.max;o->default_value=d.def;
 std::snprintf(o->name,sizeof(o->name),"%s",d.name);std::snprintf(o->module,sizeof(o->module),"%s",d.module);return true;
}
bool paramValue(const clap_plugin_t*p,clap_id id,double*v){
 if(id>=kParamCount||!v)return false;
 if(id==AutoGainCorrection){*v=self(p)->currentAutoGainDb();return true;}
 *v=self(p)->p[id];return true;
}
bool valueText(const clap_plugin_t*,clap_id id,double v,char*d,uint32_t n){
 if(id>=kParamCount||!d||!n)return false;
 if(id==AutoGain)std::snprintf(d,n,"%s",v>=0.5?"On":"Off");
 else if(id==ApplyAutoGain)std::snprintf(d,n,"%s",v>=0.5?"Apply":"Ready");
 else std::snprintf(d,n,"%.1f%s",v,defs[id].unit);
 return true;
}
bool textValue(const clap_plugin_t*,clap_id id,const char*t,double*v){
 if(id>=kParamCount||!t||!v||id==AutoGainCorrection)return false;
 if(id==AutoGain||id==ApplyAutoGain){
  *v=(!std::strcmp(t,"On")||!std::strcmp(t,"on")||!std::strcmp(t,"Apply")||
      !std::strcmp(t,"apply")||!std::strcmp(t,"1"))?1.0:0.0;
  return true;
 }
 char*e=nullptr;
 double x=std::strtod(t,&e);
 if(e==t)return false;
 *v=quantize01(clamp(x,defs[id].min,defs[id].max));
 return true;
}
void paramFlush(const clap_plugin_t*p,const clap_input_events_t*i,const clap_output_events_t*){handleEvents(self(p),i);}
const clap_plugin_params_t paramsExt{paramCount,paramInfo,paramValue,valueText,textValue,paramFlush};

struct StateBlob{uint32_t magic=0x47465247,version=6;double values[kParamCount]{};};
bool stateSave(const clap_plugin_t*p,const clap_ostream_t*s){
 if(!s||!s->write)return false;StateBlob b;for(size_t i=0;i<kParamCount;++i)b.values[i]=self(p)->p[i];
 return s->write(s,&b,sizeof(b))==(int64_t)sizeof(b);
}
bool stateLoad(const clap_plugin_t*p,const clap_istream_t*s){
 if(!s||!s->read)return false;StateBlob b;
 if(s->read(s,&b,sizeof(b))!=(int64_t)sizeof(b)||b.magic!=0x47465247||b.version!=6)return false;
 for(size_t i=0;i<kParamCount;++i){
  if(i==AutoGainCorrection||i==ApplyAutoGain){self(p)->p[i]=0.0;continue;}
  double value=clamp(b.values[i],defs[i].min,defs[i].max);
  self(p)->p[i]=(i==AutoGain)?(value>=0.5?1.0:0.0):quantize01(value);
 }
 self(p)->configure();return true;
}
const clap_plugin_state_t stateExt{stateSave,stateLoad};

const void*plugExtension(const clap_plugin_t*,const char*id){
 if(!id)return nullptr;if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audioExt;
 if(!std::strcmp(id,CLAP_EXT_PARAMS))return &paramsExt;if(!std::strcmp(id,CLAP_EXT_STATE))return &stateExt;return nullptr;
}
void plugMain(const clap_plugin_t*){}

const char*features[]={CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,CLAP_PLUGIN_FEATURE_DISTORTION,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t desc{
 CLAP_VERSION,"audio.growlforge.effect","GrowlForge","OpenAI / User Project","","","","1.2.3",
 "Post-amp guitar enhancer with neutral-reference Auto-Gain measurement.",features
};
uint32_t factoryCount(const clap_plugin_factory_t*){return 1;}
const clap_plugin_descriptor_t*factoryDesc(const clap_plugin_factory_t*,uint32_t i){return i==0?&desc:nullptr;}
const clap_plugin_t*factoryCreate(const clap_plugin_factory_t*,const clap_host_t*h,const char*id){
 if(!id||std::strcmp(id,desc.id))return nullptr;auto*s=new GrowlForge(h);
 s->plugin={&desc,s,plugInit,plugDestroy,plugActivate,plugDeactivate,plugStart,plugStop,plugReset,plugProcess,plugExtension,plugMain};
 return &s->plugin;
}
const clap_plugin_factory_t factory{factoryCount,factoryDesc,factoryCreate};
bool entryInit(const char*){return true;}void entryDeinit(){}
const void*entryFactory(const char*id){return id&&!std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr;}
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION,entryInit,entryDeinit,entryFactory};
