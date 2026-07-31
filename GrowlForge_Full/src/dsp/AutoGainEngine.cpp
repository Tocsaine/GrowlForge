#include "GrowlForgeDSP.h"

namespace growlforge {

double GrowlForgeDSP::currentAutoGainDb() const{
  const double average=0.5*(channels_[0].autoGain+channels_[1].autoGain);
  return clamp(gainToDb(average),-12.0,12.0);
 }

void GrowlForgeDSP::resetAutoGainMeasurement(){
  for(auto&c:channels_){
   c.dryRms2=1.0e-8;
   c.wetRms2=1.0e-8;
   c.autoGain=1.0;
  }
 }

void GrowlForgeDSP::applyCurrentAutoGain(){
  // Auto-Gain is measured before Output. Commit the current absolute
  // correction to Output once, then clear all measurement history.
  const double correction=quantize01(currentAutoGainDb());
  parameters_.values[Output]=quantize01(clamp(correction,defs[Output].min,defs[Output].max));
  parameters_.values[AutoGain]=0.0;
  parameters_.values[ApplyAutoGain]=0.0;
  resetAutoGainMeasurement();
 }

float GrowlForgeDSP::applyAutoGain(float dry,float wet,ChannelDSP&c){
  if(parameters_.values[AutoGain].load()<0.5){c.autoGain+=(1-c.autoGain)*0.002;return wet;}
  double rc=std::exp(-1.0/(0.300*sampleRate_));
  c.dryRms2=rc*c.dryRms2+(1-rc)*dry*dry;c.wetRms2=rc*c.wetRms2+(1-rc)*wet*wet;
  double target=std::sqrt((c.dryRms2+1e-8)/(c.wetRms2+1e-8));
  target=clamp(target,dbToGain(-12),dbToGain(12));
  double gc=std::exp(-1.0/(0.180*sampleRate_));
  c.autoGain=gc*c.autoGain+(1-gc)*target;
  return (float)(wet*c.autoGain);
 }

} // namespace growlforge
