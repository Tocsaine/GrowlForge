#include "GrowlForgeDSP.h"

namespace growlforge {

float GrowlForgeDSP::applyNewEffects(float dry,float wet,float low,float growlBand,float high,ChannelDSP&c){
  const double bloom=color(parameters_.values[Bloom].load()/10.0);
  const double sag=color(parameters_.values[Sag].load()/10.0);
  const double dynamics=color(parameters_.values[Dynamics].load()/10.0);
  const double texture=color(parameters_.values[Texture].load()/10.0);
  const double attack=color(parameters_.values[Attack].load()/10.0);
  const double resonance=color(parameters_.values[Resonance].load()/10.0);
  const double compression=color(parameters_.values[Compression].load()/10.0);

  if(bloom+sag+dynamics+texture+attack+resonance+compression<=0.0){
   c.meterBloom*=0.995f;c.meterComp*=0.995f;c.meterSag*=0.995f;c.meterAttack*=0.995f;
   return wet;
  }

  const float level=std::abs(dry);
  const float fastA=(float)std::exp(-1.0/(0.004*sampleRate_));
  const float fastR=(float)std::exp(-1.0/(0.045*sampleRate_));
  const float slowA=(float)std::exp(-1.0/(0.040*sampleRate_));
  const float slowR=(float)std::exp(-1.0/(0.320*sampleRate_));

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
   const float sagAttack=(float)std::exp(-1.0/(0.012*sampleRate_));
   const float sagRelease=(float)std::exp(-1.0/(0.260*sampleRate_));
   c.sagEnv=level>c.sagEnv?sagAttack*c.sagEnv+(1-sagAttack)*level:
                           sagRelease*c.sagEnv+(1-sagRelease)*level;
   const double demand=clamp((c.sagEnv-0.025f)/0.30f,0.0,1.0);
   const double sagGain=1.0-sag*(0.06+0.24*demand*demand);
   const double recoveryWarmth=sag*0.10*(1.0-demand);
   y=(float)(y*sagGain+std::tanh((y+0.20f*low)*1.8)*recoveryWarmth);
   const float mt=(float)(100.0*clamp(sag*demand,0.0,1.0));c.meterSag+=0.02f*(mt-c.meterSag);
  }

  if(compression>0.0){
   const float compA=(float)std::exp(-1.0/(0.010*sampleRate_));
   const float compR=(float)std::exp(-1.0/(0.180*sampleRate_));
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
   const float attackCoeff=(float)std::exp(-1.0/(0.0015*sampleRate_));
   const float releaseCoeff=(float)std::exp(-1.0/(0.038*sampleRate_));
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

  if(bloom<=0)c.meterBloom*=0.995f;
  if(compression<=0)c.meterComp*=0.995f;
  if(sag<=0)c.meterSag*=0.995f;
  if(attack<=0)c.meterAttack*=0.995f;
  c.attackMemory=wet;
  return y;
 }

} // namespace growlforge
