#include "GrowlForgeDSP.h"
#include "GateEngine.h"

namespace growlforge {

void ChannelDSP::reset() {
    tightHP.reset();low110.reset();low180.reset();low650.reset();low1600.reset();
    presenceLP.reset();airLP.reset();fuzzLow.reset();driveSubsonic.reset();
    for(auto&f:antiAlias)f.reset();
    for(auto&f:postLP)f.reset();
    gateEnv=previousInput=0;
    fastEnv=slowEnv=sagEnv=attackMemory=attackEnv=compEnv=driveFastEnv=driveSlowEnv=0;
    meterSat=meterBloom=meterComp=meterSag=meterAttack=0;
    dryRms2=wetRms2=1e-8;autoGain=1.0;
}

GrowlForgeDSP::GrowlForgeDSP(ParameterStore& parameters) : parameters_(parameters) {}

void GrowlForgeDSP::setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

void GrowlForgeDSP::reset() { for (auto& channel : channels_) channel.reset(); }

bool GrowlForgeDSP::enhancersZero() const{
  for(clap_id id=Tight;id<=PreCab;++id)if(parameters_.values[id].load()>1e-9)return false;
  return true;
 }

bool GrowlForgeDSP::additionsZero() const{
  for(clap_id id=Bloom;id<=HarmonicBias;++id)if(parameters_.values[id].load()>1e-9)return false;
  return true;
 }

bool GrowlForgeDSP::x2Enabled() const{return parameters_.values[X2].load()>=0.5;}

double GrowlForgeDSP::color(double v) const{return x2Enabled()?2.0*v:v;}

void GrowlForgeDSP::configure(){
  double tight=color(parameters_.values[Tight].load()/10.0),smooth=color(parameters_.values[Smooth].load()/10.0),preCab=parameters_.values[PreCab].load()/10.0;
  double osRate=sampleRate_*kOversample;
  double hpHz=45.0+115.0*tight;
  double aaCut=clamp(sampleRate_*(0.46-0.23*smooth),5200.0,std::min(20500.0,sampleRate_*0.46));
  double openCut=std::min(21000.0,sampleRate_*0.46),closedCut=2600.0;
  double postCut=openCut*std::pow(closedCut/openCut,preCab);
  for(auto&c:channels_){
   c.tightHP.setLowpass(hpHz,sampleRate_);c.low110.setLowpass(110,sampleRate_);c.low180.setLowpass(180,sampleRate_);
   c.low650.setLowpass(650,sampleRate_);c.low1600.setLowpass(1600,sampleRate_);
   c.presenceLP.setLowpass(2600,sampleRate_);c.airLP.setLowpass(6500,sampleRate_);
   c.driveSubsonic.setHighpass(20.0,sampleRate_);
   for(auto&f:c.antiAlias)f.setLowpass(aaCut,osRate);
   c.fuzzLow.setLowpass(260,osRate);for(auto&f:c.postLP)f.setLowpass(postCut,sampleRate_);
  }
 }

float GrowlForgeDSP::processSample(float in,int ci){
  auto&c=channels_[ci];
  double inputDb=parameters_.values[Input].load(),outputDb=parameters_.values[Output].load(),gate=parameters_.values[Gate].load()/10.0;
  double tight=color(parameters_.values[Tight].load()/10.0),punch=color(parameters_.values[Punch].load()/10.0),body=color(parameters_.values[Body].load()/10.0);
  double mass=color(parameters_.values[Mass].load()/10.0),growl=color(parameters_.values[Growl].load()/10.0),bite=color(parameters_.values[Bite].load()/10.0);
  double presence=color(parameters_.values[Presence].load()/10.0),air=color(parameters_.values[Air].load()/10.0),smooth=color(parameters_.values[Smooth].load()/10.0);
  double preCab=parameters_.values[PreCab].load()/10.0,parallel=parameters_.values[ParallelDry].load()/100.0;
  float original=in,x=(float)(in*dbToGain(inputDb));

  if(gate>0){
   x=GateEngine::process(x,gate,c.gateEnv,sampleRate_);
  }

  if(enhancersZero()&&gate==0&&inputDb==0&&outputDb==0&&parameters_.values[Ceiling].load()==0&&
     parameters_.values[AutoGain].load()<0.5&&parallel==0&&additionsZero())return original;

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

  double ceiling=parameters_.values[Ceiling].load();
  if(ceiling<0){
   double cg=dbToGain(ceiling),n=y/std::max(cg,1e-6);
   y=(float)(std::tanh(n*1.35)/std::tanh(1.35)*cg);
  }
  parameters_.values[MeterSaturation]=clamp(0.5*(channels_[0].meterSat+channels_[1].meterSat),0.0,100.0);
  parameters_.values[MeterBloom]=clamp(0.5*(channels_[0].meterBloom+channels_[1].meterBloom),0.0,100.0);
  parameters_.values[MeterCompression]=clamp(0.5*(channels_[0].meterComp+channels_[1].meterComp),0.0,100.0);
  parameters_.values[MeterSag]=clamp(0.5*(channels_[0].meterSag+channels_[1].meterSag),0.0,100.0);
  parameters_.values[MeterAttack]=clamp(0.5*(channels_[0].meterAttack+channels_[1].meterAttack),0.0,100.0);
  return (float)clamp(y,-4.0,4.0);
 }

} // namespace growlforge
