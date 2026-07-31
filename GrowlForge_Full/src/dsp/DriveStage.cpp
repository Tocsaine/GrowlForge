#include "GrowlForgeDSP.h"

namespace growlforge {

float GrowlForgeDSP::nonlinear(float x,float low,float growlBand,float high,ChannelDSP&c){
  double mass=color(parameters_.values[Mass].load()/10.0),growl=color(parameters_.values[Growl].load()/10.0);
  // Drive keeps a predictable 0..10 amount and is deliberately excluded
  // from x2. Its voicing is identical whether x2 is off or on.
  const double drive=clamp(parameters_.values[Drive].load()/10.0,0.0,1.0);
  double grind=color(parameters_.values[Grind].load()/10.0),fuzz=color(parameters_.values[Fuzz].load()/10.0),bite=color(parameters_.values[Bite].load()/10.0);
  double harmonicBias=color(parameters_.values[HarmonicBias].load()/10.0);
  double focus=parameters_.values[Focus].load()/10.0;
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
   const float fastA=(float)std::exp(-1.0/(0.0018*sampleRate_));
   const float fastR=(float)std::exp(-1.0/(0.030*sampleRate_));
   const float slowA=(float)std::exp(-1.0/(0.025*sampleRate_));
   const float slowR=(float)std::exp(-1.0/(0.180*sampleRate_));
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

} // namespace growlforge
