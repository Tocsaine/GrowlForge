#include <clap/clap.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <cwchar>
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kParamCount = 36;
constexpr int kOversample = 4;

enum ParamId : clap_id {
 Input=0, Gate, Tight, Punch, Body, Mass, Growl, Drive, Grind, Fuzz,
 Bite, Presence, Air, Smooth, PreCab, ParallelDry, Output, Ceiling, AutoGain,
 AutoGainCorrection, ApplyAutoGain,
 Bloom, Sag, Dynamics, Texture, Focus, Attack,
 Resonance, Compression, HarmonicBias, X2,
 MeterSaturation, MeterBloom, MeterCompression, MeterSag, MeterAttack
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
 {ApplyAutoGain,"Apply Auto-Gain","Output",0,1,0,"",kToggle},
 {Bloom,"Bloom","Motion",0,10,0,"",kAuto},
 {Sag,"Sag","Motion",0,10,0,"",kAuto},
 {Dynamics,"Dynamics","Motion",0,10,0,"",kAuto},
 {Texture,"Texture","Character",0,10,0,"",kAuto},
 {Focus,"Focus","Character",0,10,0,"",kAuto},
 {Attack,"Attack","Motion",0,10,0,"",kAuto},
 {Resonance,"Resonance","Enhancers",0,10,0,"",kAuto},
 {Compression,"Compression","Dynamics",0,10,0,"",kAuto},
 {HarmonicBias,"Harmonic Bias","Character",0,10,0,"",kAuto},
 {X2,"x2","Enhancers",0,1,0,"",kToggle},
 {MeterSaturation,"Saturation Activity","Indicator",0,100,0," %",kReadOnly},
 {MeterBloom,"Bloom Activity","Indicator",0,100,0," %",kReadOnly},
 {MeterCompression,"Compression Activity","Indicator",0,100,0," %",kReadOnly},
 {MeterSag,"Sag Activity","Indicator",0,100,0," %",kReadOnly},
 {MeterAttack,"Attack Activity","Indicator",0,100,0," %",kReadOnly}
}};

inline double dbToGain(double db){return std::pow(10.0,db/20.0);}
inline double clamp(double x,double a,double b){return std::max(a,std::min(b,x));}
inline double gainToDb(double gain){return 20.0*std::log10(std::max(gain,1.0e-9));}
inline double quantize01(double value){return std::round(value*10.0)/10.0;}
inline double extremeCurve(double normalized,double start,double amount){
 normalized=clamp(normalized,0.0,1.0);
 if(normalized<=start)return normalized;
 const double t=(normalized-start)/(1.0-start);
 return clamp(normalized+amount*t*t,0.0,1.5);
}
inline float zap(float x){return std::abs(x)<1e-20f?0.0f:x;}

struct OnePole{
 float z=0,a=0;
 void setLowpass(double hz,double sr){hz=clamp(hz,10.0,sr*0.45);a=(float)std::exp(-2.0*kPi*hz/sr);}
 void reset(){z=0;}
 float lp(float x){z=(1-a)*x+a*z;return zap(z);}
 float hp(float x){return x-lp(x);}
};

// Second-order Butterworth high-pass used only by Drive. Asymmetric clipping
// can legitimately create an average offset from an AC input; this removes the
// resulting DC/subsonic energy without touching the audible guitar range.
struct Highpass2{
 double b0=1.0,b1=0.0,b2=0.0,a1=0.0,a2=0.0,z1=0.0,z2=0.0;
 void setHighpass(double hz,double sr){
  hz=clamp(hz,5.0,sr*0.45);
  constexpr double q=0.70710678118654752440;
  const double w0=2.0*kPi*hz/sr;
  const double cw=std::cos(w0),sw=std::sin(w0);
  const double alpha=sw/(2.0*q),a0=1.0+alpha;
  b0=((1.0+cw)*0.5)/a0;b1=(-(1.0+cw))/a0;b2=b0;
  a1=(-2.0*cw)/a0;a2=(1.0-alpha)/a0;
 }
 void reset(){z1=z2=0.0;}
 float process(float x){
  const double y=b0*x+z1;
  z1=b1*x-a1*y+z2;
  z2=b2*x-a2*y;
  if(std::abs(z1)<1.0e-24)z1=0.0;
  if(std::abs(z2)<1.0e-24)z2=0.0;
  return zap((float)y);
 }
};

struct ChannelDSP{
 OnePole tightHP,low110,low180,low650,low1600,presenceLP,airLP,fuzzLow;
 Highpass2 driveSubsonic;
 std::array<OnePole,4> antiAlias;
 std::array<OnePole,2> postLP;
 float gateEnv=0,previousInput=0;
 float fastEnv=0,slowEnv=0,sagEnv=0,attackMemory=0,attackEnv=0,compEnv=0,driveFastEnv=0,driveSlowEnv=0;
 float meterSat=0,meterBloom=0,meterComp=0,meterSag=0,meterAttack=0;
 double dryRms2=1e-8,wetRms2=1e-8,autoGain=1.0;
 void reset(){
  tightHP.reset();low110.reset();low180.reset();low650.reset();low1600.reset();
  presenceLP.reset();airLP.reset();fuzzLow.reset();driveSubsonic.reset();
  for(auto&f:antiAlias)f.reset();for(auto&f:postLP)f.reset();
  gateEnv=previousInput=0;
  fastEnv=slowEnv=sagEnv=attackMemory=attackEnv=compEnv=driveFastEnv=driveSlowEnv=0;
  meterSat=meterBloom=meterComp=meterSag=meterAttack=0;
  dryRms2=wetRms2=1e-8;autoGain=1.0;
 }
};

struct GrowlForge{
 clap_plugin_t plugin{}; const clap_host_t* host=nullptr;
 const clap_host_params_t* hostParams=nullptr;
 std::array<std::atomic<double>,kParamCount> p{};
 std::array<std::atomic<double>,kParamCount> guiPendingValue{};
 std::array<std::atomic<uint8_t>,kParamCount> guiPendingFlags{};
 std::array<std::atomic<float>,2> guiInputPeak{};
 std::array<std::atomic<float>,2> guiOutputPeak{};
 std::atomic<bool> configDirty{false};
 std::atomic<bool> autoGainResetPending{false};
 std::atomic<bool> applyAutoGainPending{false};
 void* guiState=nullptr;
 std::array<ChannelDSP,2> ch{}; double sampleRate=48000.0;

 explicit GrowlForge(const clap_host_t*h):host(h){
  for(size_t i=0;i<p.size();++i){
   p[i]=defs[i].def;guiPendingValue[i]=defs[i].def;guiPendingFlags[i]=0;
  }
  for(auto&v:guiInputPeak)v=0.0f;for(auto&v:guiOutputPeak)v=0.0f;
 }

 void requestParamFlush(){
  if(hostParams&&hostParams->request_flush)hostParams->request_flush(host);
  else if(host&&host->request_process)host->request_process(host);
 }

 void queueGuiFlag(clap_id id,uint8_t flag){
  if(id>=kParamCount)return;guiPendingFlags[id].fetch_or(flag,std::memory_order_release);requestParamFlush();
 }

 void beginGuiGesture(clap_id id){queueGuiFlag(id,1u);}
 void endGuiGesture(clap_id id){queueGuiFlag(id,4u);}

 void setGuiParameter(clap_id id,double value){
  if(id>=kParamCount||id==AutoGainCorrection||id>=MeterSaturation)return;
  if(id==ApplyAutoGain){applyAutoGainPending=true;requestParamFlush();return;}
  value=clamp(value,defs[id].min,defs[id].max);
  if(id==AutoGain||id==X2)value=value>=0.5?1.0:0.0;else value=quantize01(value);
  const double previous=p[id].exchange(value);
  if(id==AutoGain&&previous!=value){
   if(value>=0.5){p[Output]=0.0;guiPendingValue[Output]=0.0;guiPendingFlags[Output].fetch_or(2u,std::memory_order_release);}
   autoGainResetPending=true;
  }
  guiPendingValue[id]=value;guiPendingFlags[id].fetch_or(2u,std::memory_order_release);
  configDirty=true;requestParamFlush();
 }

 bool enhancersZero()const{
  for(clap_id id=Tight;id<=PreCab;++id)if(p[id].load()>1e-9)return false;
  return true;
 }

 bool additionsZero()const{
  for(clap_id id=Bloom;id<=HarmonicBias;++id)if(p[id].load()>1e-9)return false;
  return true;
 }

 bool x2Enabled()const{return p[X2].load()>=0.5;}
 double color(double v)const{return x2Enabled()?2.0*v:v;}

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
  double tight=color(p[Tight].load()/10.0),smooth=color(p[Smooth].load()/10.0),preCab=p[PreCab].load()/10.0;
  double osRate=sampleRate*kOversample;
  double hpHz=45.0+115.0*tight;
  double aaCut=clamp(sampleRate*(0.46-0.23*smooth),5200.0,std::min(20500.0,sampleRate*0.46));
  double openCut=std::min(21000.0,sampleRate*0.46),closedCut=2600.0;
  double postCut=openCut*std::pow(closedCut/openCut,preCab);
  for(auto&c:ch){
   c.tightHP.setLowpass(hpHz,sampleRate);c.low110.setLowpass(110,sampleRate);c.low180.setLowpass(180,sampleRate);
   c.low650.setLowpass(650,sampleRate);c.low1600.setLowpass(1600,sampleRate);
   c.presenceLP.setLowpass(2600,sampleRate);c.airLP.setLowpass(6500,sampleRate);
   c.driveSubsonic.setHighpass(20.0,sampleRate);
   for(auto&f:c.antiAlias)f.setLowpass(aaCut,osRate);
   c.fuzzLow.setLowpass(260,osRate);for(auto&f:c.postLP)f.setLowpass(postCut,sampleRate);
  }
 }

 float nonlinear(float x,float low,float growlBand,float high,ChannelDSP&c){
  double mass=color(p[Mass].load()/10.0),growl=color(p[Growl].load()/10.0);
  // Drive keeps a predictable 0..10 amount and is deliberately excluded
  // from x2. Its voicing is identical whether x2 is off or on.
  const double drive=clamp(p[Drive].load()/10.0,0.0,1.0);
  double grind=color(p[Grind].load()/10.0),fuzz=color(p[Fuzz].load()/10.0),bite=color(p[Bite].load()/10.0);
  double harmonicBias=color(p[HarmonicBias].load()/10.0);
  double focus=p[Focus].load()/10.0;
  if(mass+growl+drive+grind+fuzz+bite+harmonicBias<=0){
   c.previousInput=x;c.driveSubsonic.reset();c.meterSat*=0.995f;return x;
  }

  if(focus>0.0){
   const float focused=0.18f*low+1.42f*growlBand+0.42f*high;
   x=x*(float)(1.0-focus)+focused*(float)focus;
  }

  double massH=1.65*mass*low*std::abs((double)low);
  double growlH=3.10*growl*growlBand*std::abs((double)growlBand);
  double biteH=1.45*bite*high*high*high;
  float target=(float)(x+massH+growlH+biteH);
  if(harmonicBias>0.0){
   const double even=target*std::abs((double)target);
   const double biasMix=clamp(0.34*harmonicBias,0.0,0.68);
   target=(float)(target*(1.0-biasMix)+std::tanh(target+2.4*even)*biasMix);
  }

  // Touch-sensitive Drive stage. The full 0..10 range is deliberately useful:
  // the middle already reaches a clear overdrive, while the upper range blends
  // into a controlled second distortion stage without becoming Fuzz.
  float driveInput=target;
  double driveTransient=0.0;
  if(drive>0.0){
   const float level=std::abs(target);
   const float fastA=(float)std::exp(-1.0/(0.0018*sampleRate));
   const float fastR=(float)std::exp(-1.0/(0.030*sampleRate));
   const float slowA=(float)std::exp(-1.0/(0.025*sampleRate));
   const float slowR=(float)std::exp(-1.0/(0.180*sampleRate));
   c.driveFastEnv=level>c.driveFastEnv?fastA*c.driveFastEnv+(1-fastA)*level:
                                        fastR*c.driveFastEnv+(1-fastR)*level;
   c.driveSlowEnv=level>c.driveSlowEnv?slowA*c.driveSlowEnv+(1-slowA)*level:
                                        slowR*c.driveSlowEnv+(1-slowR)*level;
   driveTransient=clamp((c.driveFastEnv-c.driveSlowEnv)/(c.driveFastEnv+0.006f),0.0,1.0);

   // A broad mid push gives the clipping stage body and teeth. At high Drive a
   // small amount of deep low end is removed before the harder stage so palm
   // mutes stay compact instead of turning into loose clipping.
   const double midEmphasis=drive*(0.15+0.27*drive);
   const double lowCleanup=drive*drive*(0.035+0.055*drive);
   driveInput+=(float)(growlBand*midEmphasis+high*(0.035*drive)-low*lowCleanup);
  }

  // Raise the useful middle of the control without introducing a perceptible
  // step or dead zone. Drive 5 now carries roughly two thirds of the available
  // internal pressure rather than behaving like a weak preamp trim.
  const double driveShape=clamp(drive+0.52*drive*(1.0-drive),0.0,1.0);
  const double touchGain=1.0+driveShape*(0.72+1.05*driveShape)*driveTransient;
  double pre=1+18.5*driveShape+11.0*driveShape*driveShape+9*grind+2.5*growl+1.5*bite;
  double pos=1+0.42*grind+0.18*growl,neg=1-0.28*grind,fuzzGain=5+58*fuzz;
  float out=0,start=c.previousInput;

  // These terms depend on the current parameters/envelope, not on the four
  // oversampling points. Calculate them once per input sample.
  const double amount=clamp(0.96*driveShape+0.62*grind+0.32*growl+0.22*bite+0.18*mass,0.0,1.0);
  const double asymmetry=driveShape*driveShape*(0.12+0.30*driveShape);
  const double characterMix=clamp(driveShape*(0.10+0.40*driveShape),0.0,0.78);
  const double hardStart=clamp((driveShape-0.58)/0.42,0.0,1.0);
  const double hardCurve=hardStart*hardStart*(3.0-2.0*hardStart);
  const double transientRelief=1.0-driveTransient*(0.46+0.16*driveShape);
  const double hardMix=clamp(hardCurve*0.72*transientRelief,0.0,0.88);
  const double hardGain=1.75+2.45*driveShape;
  const double hardAsym=asymmetry*0.82;
  const double grindMix=clamp(grind*0.55,0.0,0.75);
  const double normalization=std::sqrt(1+0.23*(pre-1)*amount);

  // The original asymmetric stages used an intentional bias. Their transfer
  // at zero input was therefore non-zero, which generated a very large DC
  // component. Compute the exact zero-input response of the same transfer once
  // and subtract only that constant. The audible nonlinear curve is otherwise
  // preserved.
  const double biasedZero=asymmetry*0.42;
  const double asymmetricZero=std::tanh(biasedZero*pre*(1.0+0.24*asymmetry));
  const double firstStageZero=asymmetricZero*characterMix;
  const double hardInputZero=firstStageZero*hardGain;
  const double hardStageZero=0.72*std::tanh(hardInputZero+hardAsym)+
                             0.28*std::tanh(hardInputZero*1.85-hardAsym*0.55);
  const double driveSatZero=firstStageZero*(1.0-hardMix)+hardStageZero*hardMix;
  const double satZero=drive>0.0?driveSatZero*(1.0-grindMix):0.0;
  const double mainZero=satZero*amount;

  for(int i=1;i<=kOversample;++i){
   float t=(float)i/kOversample,u=start+(driveInput-start)*t;
   const double driven=u*touchGain;

   // First stage: responsive overdrive with a progressively asymmetric voice.
   const double symmetric=std::tanh(driven*pre);
   const double biased=driven+asymmetry*(0.42+0.58*std::abs(driven));
   const double asymmetric=std::tanh(biased*pre*(driven>=0?1.0+0.24*asymmetry:1.0-0.17*asymmetry));
   const double firstStage=symmetric*(1.0-characterMix)+asymmetric*characterMix;

   // Second stage: fades in above roughly Drive 5.5 and moves the top of the
   // range toward distortion. The first few milliseconds of a pick transient
   // are deliberately fed less strongly into this stage, preserving tactile
   // attack even when the sustained body is heavily clipped.
   const double hardInput=firstStage*hardGain;
   const double hardStage=0.72*std::tanh(hardInput+hardAsym)+0.28*std::tanh(hardInput*1.85-hardAsym*0.55);
   const double driveSat=firstStage*(1.0-hardMix)+hardStage*hardMix;

   // Existing Grind polarity and Fuzz architecture remain untouched. Drive is
   // mixed into the established nonlinear path rather than replacing it.
   const double otherSat=std::tanh(driven*pre*(driven>=0?pos:neg));
   const double sat=drive>0.0?driveSat*(1.0-grindMix)+otherSat*grindMix:otherSat;
   double main=driven*(1-amount)+sat*amount;

   float fl=c.fuzzLow.lp(u),fs=u-0.72f*fl;
   double sustain=clamp((std::abs((double)u)-0.015)/0.18,0.0,1.0);
   double fm=fuzz*(0.12+0.34*sustain);
   double intelligent=0.86*std::tanh(fs*fuzzGain)+0.14*std::tanh(fl*(1+4*driveShape));

   // Drive is excluded from x2, so no x2-specific makeup is needed here.
   double y=((main-mainZero)*(1-fm)+intelligent*fm)/normalization;

   float filtered=(float)y;for(auto&f:c.antiAlias)filtered=f.lp(filtered);out=filtered;
  }

  // Remove any signal-dependent DC and subsonic modulation created by genuine
  // asymmetric clipping. This filter is active only while Drive is non-zero,
  // so Drive=0 keeps strict zero influence and all other controls retain their
  // previous response.
  if(drive>0.0)out=c.driveSubsonic.process(out);else c.driveSubsonic.reset();

  const float satTarget=(float)(100.0*clamp(0.45*drive+0.34*grind+0.28*fuzz+0.18*growl+0.16*harmonicBias,0.0,1.0));
  c.meterSat+=0.02f*(satTarget-c.meterSat);
  c.previousInput=driveInput;return out;
 }


 float applyNewEffects(float dry,float wet,float low,float growlBand,float high,ChannelDSP&c){
  const double bloom=color(p[Bloom].load()/10.0);
  const double sag=color(p[Sag].load()/10.0);
  const double dynamics=color(p[Dynamics].load()/10.0);
  const double texture=color(p[Texture].load()/10.0);
  const double attack=color(p[Attack].load()/10.0);
  const double resonance=color(p[Resonance].load()/10.0);
  const double compression=color(p[Compression].load()/10.0);

  if(bloom+sag+dynamics+texture+attack+resonance+compression<=0.0){
   c.meterBloom*=0.995f;c.meterComp*=0.995f;c.meterSag*=0.995f;c.meterAttack*=0.995f;
   return wet;
  }

  const float level=std::abs(dry);
  const float fastA=(float)std::exp(-1.0/(0.004*sampleRate));
  const float fastR=(float)std::exp(-1.0/(0.045*sampleRate));
  const float slowA=(float)std::exp(-1.0/(0.040*sampleRate));
  const float slowR=(float)std::exp(-1.0/(0.320*sampleRate));

  c.fastEnv=level>c.fastEnv?fastA*c.fastEnv+(1-fastA)*level:
                             fastR*c.fastEnv+(1-fastR)*level;
  c.slowEnv=level>c.slowEnv?slowA*c.slowEnv+(1-slowA)*level:
                             slowR*c.slowEnv+(1-slowR)*level;

  float y=wet;

  if(resonance>0.0){
   const float resonantLow=c.low110.lp(y);
   const double hit=clamp((c.fastEnv-0.018f)/0.22f,0.0,1.0);
   const double amount=clamp(resonance*(0.08+0.22*hit),0.0,0.55);
   y+=(float)(resonantLow*amount);
  }

  if(dynamics>0.0){
   const double playing=clamp((c.fastEnv-0.004f)/0.16f,0.0,1.0);
   const double drive=1.0+dynamics*(0.25+2.25*playing*playing);
   const double saturated=std::tanh(y*drive)/std::max(1.0,drive*0.72);
   const double mix=dynamics*(0.10+0.32*playing);
   y=(float)(y*(1.0-mix)+saturated*mix);
  }

  if(bloom>0.0){
   const double decay=clamp((c.slowEnv-c.fastEnv)/(c.slowEnv+0.008f),0.0,1.0);
   const double active=clamp(c.slowEnv/0.055f,0.0,1.0);
   const double bloomAmount=bloom*decay*active;
   const double harmonic=std::tanh((y+0.32f*growlBand)*3.1);
   y=(float)(y*(1.0-0.34*bloomAmount)+harmonic*(0.34*bloomAmount));
   const float mt=(float)(100.0*clamp(bloomAmount,0.0,1.0));c.meterBloom+=0.025f*(mt-c.meterBloom);
  }

  if(sag>0.0){
   const float sagAttack=(float)std::exp(-1.0/(0.012*sampleRate));
   const float sagRelease=(float)std::exp(-1.0/(0.260*sampleRate));
   c.sagEnv=level>c.sagEnv?sagAttack*c.sagEnv+(1-sagAttack)*level:
                           sagRelease*c.sagEnv+(1-sagRelease)*level;
   const double demand=clamp((c.sagEnv-0.025f)/0.30f,0.0,1.0);
   const double sagGain=1.0-sag*(0.06+0.24*demand*demand);
   const double recoveryWarmth=sag*0.10*(1.0-demand);
   y=(float)(y*sagGain+std::tanh((y+0.20f*low)*1.8)*recoveryWarmth);
   const float mt=(float)(100.0*clamp(sag*demand,0.0,1.0));c.meterSag+=0.02f*(mt-c.meterSag);
  }

  if(compression>0.0){
   const float compA=(float)std::exp(-1.0/(0.010*sampleRate));
   const float compR=(float)std::exp(-1.0/(0.180*sampleRate));
   const float ay=std::abs(y);
   c.compEnv=ay>c.compEnv?compA*c.compEnv+(1-compA)*ay:compR*c.compEnv+(1-compR)*ay;
   const double over=clamp((c.compEnv-0.055f)/0.32f,0.0,1.0);
   const double reduction=compression*(0.05+0.25*over);
   const double makeup=1.0+compression*(0.025+0.075*over);
   const float compressed=(float)(y*(1.0-reduction)*makeup);
   const double glueMix=clamp(compression*(0.22+0.28*compression),0.0,0.78);
   y=(float)(y*(1.0-glueMix)+compressed*glueMix);
   const float mt=(float)(100.0*clamp(compression*over,0.0,1.0));c.meterComp+=0.018f*(mt-c.meterComp);
  }

  if(texture>0.0){
   const float derivative=y-c.attackMemory;
   const double odd=std::tanh(y*(1.8+3.2*texture));
   const double grain=std::tanh((y+0.55f*derivative+0.18f*high)*(3.0+5.0*texture));
   const double mix=texture*(0.05+0.20*texture);
   y=(float)(y*(1.0-mix)+((1.0-0.42*texture)*odd+0.42*texture*grain)*mix);
  }

  if(attack>0.0){
   // Two-part transient enhancement:
   // 1) a fast edge component for pick definition;
   // 2) a short envelope-shaped body lift so the effect is audible
   //    without becoming only a high-frequency click.
   const double transient=clamp((c.fastEnv-c.slowEnv)/(c.fastEnv+0.004f),0.0,1.0);
   const float envTarget=(float)transient;
   const float attackCoeff=(float)std::exp(-1.0/(0.0015*sampleRate));
   const float releaseCoeff=(float)std::exp(-1.0/(0.038*sampleRate));
   c.attackEnv=envTarget>c.attackEnv
      ? attackCoeff*c.attackEnv+(1.0f-attackCoeff)*envTarget
      : releaseCoeff*c.attackEnv+(1.0f-releaseCoeff)*envTarget;

   const float edge=y-c.attackMemory;
   const double edgeGain=attack*(0.90+1.10*attack);
   const double bodyGain=attack*(0.12+0.30*attack);
   const float attackBody=0.55f*y+0.30f*high+0.15f*growlBand;

   y+=(float)(edge*edgeGain*c.attackEnv);
   y+=(float)(attackBody*bodyGain*c.attackEnv);
   const float mt=(float)(100.0*clamp(attack*c.attackEnv,0.0,1.0));c.meterAttack+=0.03f*(mt-c.meterAttack);
  }

  if(bloom<=0)c.meterBloom*=0.995f;if(compression<=0)c.meterComp*=0.995f;
  if(sag<=0)c.meterSag*=0.995f;if(attack<=0)c.meterAttack*=0.995f;
  c.attackMemory=wet;
  return y;
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
  double tight=color(p[Tight].load()/10.0),punch=color(p[Punch].load()/10.0),body=color(p[Body].load()/10.0);
  double mass=color(p[Mass].load()/10.0),growl=color(p[Growl].load()/10.0),bite=color(p[Bite].load()/10.0);
  double presence=color(p[Presence].load()/10.0),air=color(p[Air].load()/10.0),smooth=color(p[Smooth].load()/10.0);
  double preCab=p[PreCab].load()/10.0,parallel=p[ParallelDry].load()/100.0;
  float original=in,x=(float)(in*dbToGain(inputDb));

  if(gate>0){
   double th=dbToGain(-90+58*gate);float ax=std::abs(x);
   float a=(float)std::exp(-1.0/(0.0012*sampleRate)),r=(float)std::exp(-1.0/(0.070*sampleRate));
   c.gateEnv=ax>c.gateEnv?a*c.gateEnv+(1-a)*ax:r*c.gateEnv+(1-r)*ax;
   x*=(float)clamp((c.gateEnv-th*0.30)/(th*0.70),0.0,1.0);
  }

  if(enhancersZero()&&gate==0&&inputDb==0&&outputDb==0&&p[Ceiling].load()==0&&
     p[AutoGain].load()<0.5&&parallel==0&&additionsZero())return original;

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

  y=applyNewEffects(x,y,low,growlBand,high,c);
  y=applyAutoGain(x,y,c);
  y=(float)(y*(1-parallel)+x*parallel);
  y=(float)(y*dbToGain(outputDb));

  double ceiling=p[Ceiling].load();
  if(ceiling<0){
   double cg=dbToGain(ceiling),n=y/std::max(cg,1e-6);
   y=(float)(std::tanh(n*1.35)/std::tanh(1.35)*cg);
  }
  p[MeterSaturation]=clamp(0.5*(ch[0].meterSat+ch[1].meterSat),0.0,100.0);
  p[MeterBloom]=clamp(0.5*(ch[0].meterBloom+ch[1].meterBloom),0.0,100.0);
  p[MeterCompression]=clamp(0.5*(ch[0].meterComp+ch[1].meterComp),0.0,100.0);
  p[MeterSag]=clamp(0.5*(ch[0].meterSag+ch[1].meterSag),0.0,100.0);
  p[MeterAttack]=clamp(0.5*(ch[0].meterAttack+ch[1].meterAttack),0.0,100.0);
  return (float)clamp(y,-4.0,4.0);
 }
};

GrowlForge*self(const clap_plugin_t*p){return static_cast<GrowlForge*>(p->plugin_data);}

#include "GrowlForgeGUI.h"

void handleEvents(GrowlForge*s,const clap_input_events_t*ev){
 if(!ev||!ev->size||!ev->get)return;
 bool changed=false;
 for(uint32_t i=0;i<ev->size(ev);++i){
  auto*h=ev->get(ev,i);
  if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID||h->type!=CLAP_EVENT_PARAM_VALUE)continue;
  auto*v=reinterpret_cast<const clap_event_param_value_t*>(h);
  if(v->param_id>=kParamCount||v->param_id==AutoGainCorrection||v->param_id>=MeterSaturation)continue;

  double x=clamp(v->value,defs[v->param_id].min,defs[v->param_id].max);
  if(v->param_id==AutoGain||v->param_id==ApplyAutoGain||v->param_id==X2){
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

bool pushGuiGesture(const clap_output_events_t*out,uint16_t type,clap_id id){
 if(!out||!out->try_push)return false;
 clap_event_param_gesture_t e{};e.header.size=sizeof(e);e.header.time=0;e.header.space_id=CLAP_CORE_EVENT_SPACE_ID;
 e.header.type=type;e.header.flags=CLAP_EVENT_IS_LIVE;e.param_id=id;
 return out->try_push(out,&e.header);
}

bool pushGuiValue(const clap_output_events_t*out,clap_id id,double value){
 if(!out||!out->try_push)return false;
 clap_event_param_value_t e{};e.header.size=sizeof(e);e.header.time=0;e.header.space_id=CLAP_CORE_EVENT_SPACE_ID;
 e.header.type=CLAP_EVENT_PARAM_VALUE;e.header.flags=CLAP_EVENT_IS_LIVE;e.param_id=id;e.cookie=nullptr;
 e.note_id=-1;e.port_index=-1;e.channel=-1;e.key=-1;e.value=value;
 return out->try_push(out,&e.header);
}

void applyDeferredGuiActions(GrowlForge*s){
 if(s->autoGainResetPending.exchange(false))s->resetAutoGainMeasurement();
 if(s->applyAutoGainPending.exchange(false)){
  s->applyCurrentAutoGain();
  s->guiPendingValue[Output]=s->p[Output].load();
  s->guiPendingValue[AutoGain]=s->p[AutoGain].load();
  s->guiPendingFlags[Output].fetch_or(2u,std::memory_order_release);
  s->guiPendingFlags[AutoGain].fetch_or(2u,std::memory_order_release);
 }
 if(s->configDirty.exchange(false))s->configure();
}

void flushGuiEvents(GrowlForge*s,const clap_output_events_t*out){
 applyDeferredGuiActions(s);
 if(!out||!out->try_push)return;
 for(clap_id id=0;id<kParamCount;++id){
  uint8_t flags=s->guiPendingFlags[id].exchange(0,std::memory_order_acq_rel);
  if(!flags)continue;
  if((flags&1u)&&!pushGuiGesture(out,CLAP_EVENT_PARAM_GESTURE_BEGIN,id)){
   s->guiPendingFlags[id].fetch_or(flags,std::memory_order_release);continue;
  }
  if((flags&2u)&&!pushGuiValue(out,id,s->guiPendingValue[id].load())){
   s->guiPendingFlags[id].fetch_or((uint8_t)(flags&6u),std::memory_order_release);continue;
  }
  if((flags&4u)&&!pushGuiGesture(out,CLAP_EVENT_PARAM_GESTURE_END,id))
   s->guiPendingFlags[id].fetch_or(4u,std::memory_order_release);
 }
}

bool plugInit(const clap_plugin_t*p){
 auto*s=self(p);
 if(s->host&&s->host->get_extension)
  s->hostParams=static_cast<const clap_host_params_t*>(s->host->get_extension(s->host,CLAP_EXT_PARAMS));
 return true;
}
void plugDestroy(const clap_plugin_t*p){auto*s=self(p);destroyGrowlForgeGui(s);delete s;}
bool plugActivate(const clap_plugin_t*p,double sr,uint32_t,uint32_t){
 auto*s=self(p);if(sr<=1000)return false;s->sampleRate=sr;for(auto&c:s->ch)c.reset();s->configure();return true;
}
void plugDeactivate(const clap_plugin_t*){} bool plugStart(const clap_plugin_t*){return true;}
void plugStop(const clap_plugin_t*){} void plugReset(const clap_plugin_t*p){for(auto&c:self(p)->ch)c.reset();}

clap_process_status plugProcess(const clap_plugin_t*p,const clap_process_t*pr){
 if(!pr)return CLAP_PROCESS_ERROR;
 auto*s=self(p);handleEvents(s,pr->in_events);flushGuiEvents(s,pr->out_events);
 if(pr->audio_inputs_count<1||pr->audio_outputs_count<1)return CLAP_PROCESS_CONTINUE;
 auto&in=pr->audio_inputs[0];auto&out=pr->audio_outputs[0];
 const uint32_t channels=std::min({in.channel_count,out.channel_count,2u});
 const float decay=(float)std::exp(-(double)pr->frames_count/std::max(1.0,0.34*s->sampleRate));
 for(uint32_t c=0;c<channels;++c){
  if(!in.data32||!out.data32||!in.data32[c]||!out.data32[c])continue;
  float peakIn=0.0f,peakOut=0.0f;
  for(uint32_t n=0;n<pr->frames_count;++n){
   const float input=in.data32[c][n];
   const float output=s->processSample(input,(int)c);
   out.data32[c][n]=output;peakIn=std::max(peakIn,std::abs(input));peakOut=std::max(peakOut,std::abs(output));
  }
  s->guiInputPeak[c]=std::max(peakIn,s->guiInputPeak[c].load()*decay);
  s->guiOutputPeak[c]=std::max(peakOut,s->guiOutputPeak[c].load()*decay);
 }
 for(uint32_t c=channels;c<2;++c){
  s->guiInputPeak[c]=s->guiInputPeak[c].load()*decay;
  s->guiOutputPeak[c]=s->guiOutputPeak[c].load()*decay;
 }
 s->p[AutoGainCorrection]=s->currentAutoGainDb();
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
 if(id==AutoGain||id==X2)std::snprintf(d,n,"%s",v>=0.5?"On":"Off");
 else if(id==ApplyAutoGain)std::snprintf(d,n,"%s",v>=0.5?"Apply":"Ready");
 else std::snprintf(d,n,"%.1f%s",v,defs[id].unit);
 return true;
}
bool textValue(const clap_plugin_t*,clap_id id,const char*t,double*v){
 if(id>=kParamCount||!t||!v||id==AutoGainCorrection)return false;
 if(id==AutoGain||id==ApplyAutoGain||id==X2){
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
void paramFlush(const clap_plugin_t*p,const clap_input_events_t*i,const clap_output_events_t*out){
 auto*s=self(p);handleEvents(s,i);flushGuiEvents(s,out);
}
const clap_plugin_params_t paramsExt{paramCount,paramInfo,paramValue,valueText,textValue,paramFlush};

struct StateHeader{uint32_t magic=0x47465247,version=10;};
struct StateBlob{uint32_t magic=0x47465247,version=10;double values[kParamCount]{};};

bool stateSave(const clap_plugin_t*p,const clap_ostream_t*s){
 if(!s||!s->write)return false;StateBlob b;
 for(size_t i=0;i<kParamCount;++i)b.values[i]=(i>=MeterSaturation)?0.0:self(p)->p[i].load();
 return s->write(s,&b,sizeof(b))==(int64_t)sizeof(b);
}
bool stateLoad(const clap_plugin_t*p,const clap_istream_t*s){
 if(!s||!s->read)return false;StateHeader h;
 if(s->read(s,&h,sizeof(h))!=(int64_t)sizeof(h)||h.magic!=0x47465247)return false;
 std::array<double,kParamCount> loaded{};for(size_t i=0;i<kParamCount;++i)loaded[i]=defs[i].def;
 if(h.version==10){
  if(s->read(s,loaded.data(),sizeof(double)*kParamCount)!=(int64_t)(sizeof(double)*kParamCount))return false;
 }else if(h.version==9){
  std::array<double,27> old{};if(s->read(s,old.data(),sizeof(old))!=(int64_t)sizeof(old))return false;
  for(size_t i=0;i<old.size();++i)loaded[i]=old[i];
 }else if(h.version==8){
  std::array<double,28> old{};if(s->read(s,old.data(),sizeof(old))!=(int64_t)sizeof(old))return false;
  for(size_t i=0;i<27;++i)loaded[i]=old[i];
 }else if(h.version==7){
  std::array<double,21> old{};if(s->read(s,old.data(),sizeof(old))!=(int64_t)sizeof(old))return false;
  for(size_t i=0;i<old.size();++i)loaded[i]=old[i];
 }else return false;
 for(size_t i=0;i<kParamCount;++i){
  if(i==AutoGainCorrection||i==ApplyAutoGain||i>=MeterSaturation){self(p)->p[i]=0.0;continue;}
  double value=clamp(loaded[i],defs[i].min,defs[i].max);
  self(p)->p[i]=(i==AutoGain||i==X2)?(value>=0.5?1.0:0.0):quantize01(value);
 }
 for(auto&c:self(p)->ch)c.reset();self(p)->configure();return true;
}
const clap_plugin_state_t stateExt{stateSave,stateLoad};

const void*plugExtension(const clap_plugin_t*,const char*id){
 if(!id)return nullptr;if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audioExt;
 if(!std::strcmp(id,CLAP_EXT_PARAMS))return &paramsExt;if(!std::strcmp(id,CLAP_EXT_STATE))return &stateExt;
 if(!std::strcmp(id,CLAP_EXT_GUI))return &guiExt;return nullptr;
}
void plugMain(const clap_plugin_t*){}

const char*features[]={CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,CLAP_PLUGIN_FEATURE_DISTORTION,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t desc{
 CLAP_VERSION,"audio.growlforge.effect","GrowlForge","OpenAI / User Project","","","","2.0.3",
 "Post-amp guitar character processor with a scalable custom interface, live meters and tactile distortion shaping.",features
};
uint32_t factoryCount(const clap_plugin_factory_t*){return 1;}
const clap_plugin_descriptor_t*factoryDesc(const clap_plugin_factory_t*,uint32_t i){return i==0?&desc:nullptr;}
const clap_plugin_t*factoryCreate(const clap_plugin_factory_t*,const clap_host_t*h,const char*id){
 if(!id||std::strcmp(id,desc.id))return nullptr;auto*s=new GrowlForge(h);
 s->plugin={&desc,s,plugInit,plugDestroy,plugActivate,plugDeactivate,plugStart,plugStop,plugReset,plugProcess,plugExtension,plugMain};
 return &s->plugin;
}
const clap_plugin_factory_t factory{factoryCount,factoryDesc,factoryCreate};
bool entryInit(const char*){return growlForgeGuiGlobalInit();}void entryDeinit(){growlForgeGuiGlobalShutdown();}
const void*entryFactory(const char*id){return id&&!std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr;}
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION,entryInit,entryDeinit,entryFactory};
