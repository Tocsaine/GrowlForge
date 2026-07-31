#include "GrowlForgeGUI.h"
#include "../plugin/GrowlForgePlugin.h"
#include "../parameters/ParameterDefinitions.h"
#include "../common/Math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#endif

namespace growlforge {

#ifdef _WIN32

namespace gfui {
using namespace Gdiplus;

constexpr float kDesignWidth = 1200.0f;
constexpr float kDesignHeight = 720.0f;
constexpr uint32_t kBaseWidth = 1200;
constexpr uint32_t kBaseHeight = 720;
constexpr uint32_t kMinWidth = 900;
constexpr uint32_t kMinHeight = 540;
constexpr uint32_t kMaxWidth = 2000;
constexpr uint32_t kMaxHeight = 1200;
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kAnimationIntervalMs = 33;
constexpr wchar_t kWindowClass[] = L"GrowlForge2PluginWindow";

struct KnobDef {
 clap_id id;
 const wchar_t* label;
 float x, y, radius;
 bool orange;
 clap_id activity;
};

constexpr clap_id kNoActivity = CLAP_INVALID_ID;

static const KnobDef kKnobs[] = {
 {Input,L"Input",70,178,31,false,kNoActivity},
 {Gate,L"Gate",175,178,31,false,kNoActivity},
 {Tight,L"Tight",280,178,31,false,kNoActivity},
 {Punch,L"Punch",70,329,31,false,kNoActivity},
 {Body,L"Body",175,329,31,false,kNoActivity},
 {Mass,L"Mass",280,329,31,false,kNoActivity},

 {Growl,L"Growl",425,180,35,true,kNoActivity},
 {Drive,L"Drive",600,248,79,true,kNoActivity},
 {Grind,L"Grind",775,180,35,true,kNoActivity},
 {Fuzz,L"Fuzz",442,354,35,true,kNoActivity},
 {HarmonicBias,L"Harmonic Bias",758,354,35,true,kNoActivity},

 {Bloom,L"Bloom",920,178,31,false,MeterBloom},
 {Sag,L"Sag",1025,178,31,false,MeterSag},
 {Dynamics,L"Dynamics",1130,178,31,false,kNoActivity},
 {Compression,L"Compression",920,329,31,false,MeterCompression},
 {Attack,L"Attack",1025,329,31,false,MeterAttack},
 {Resonance,L"Resonance",1130,329,31,false,kNoActivity},

 {Bite,L"Bite",70,538,29,false,kNoActivity},
 {Presence,L"Presence",175,538,29,false,kNoActivity},
 {Air,L"Air",280,538,29,false,kNoActivity},
 {Smooth,L"Smooth",385,538,29,false,kNoActivity},
 {Texture,L"Texture",490,538,29,false,kNoActivity},
 {Focus,L"Focus",595,538,29,false,kNoActivity},

 {PreCab,L"Pre-Cab",722,538,29,false,kNoActivity},
 {ParallelDry,L"Parallel Dry",817,538,29,false,kNoActivity},
 {Output,L"Output",1034,532,42,false,kNoActivity},
 {Ceiling,L"Ceiling",1138,538,29,false,kNoActivity},
};

struct GdiSurface {
 HDC dc = nullptr;
 HBITMAP bitmap = nullptr;
 HGDIOBJ oldBitmap = nullptr;
 int width = 0;
 int height = 0;
};

struct KnobBodyCache {
 float radius = 0.0f;
 bool orange = false;
 float scale = 1.0f;
 float logicalSide = 0.0f;
 std::unique_ptr<Bitmap> bitmap;
};

struct GuiState {
 GrowlForge* owner = nullptr;
 HWND hwnd = nullptr;
 HWND parent = nullptr;
 uint32_t width = kBaseWidth;
 uint32_t height = kBaseHeight;
 double scale = 1.0;
 clap_id hover = CLAP_INVALID_ID;
 clap_id drag = CLAP_INVALID_ID;
 int dragStartY = 0;
 double dragStartValue = 0.0;
 bool shown = false;
 bool staticDirty = true;
 bool trackingMouse = false;

 GdiSurface staticLayer;
 GdiSurface backBuffer;
 std::vector<KnobBodyCache> knobBodies;

 std::unique_ptr<FontFamily> fontFamily;
 std::unordered_map<int,std::unique_ptr<Font>> fonts;
 std::unique_ptr<StringFormat> formatCenter;
 std::unique_ptr<StringFormat> formatNear;
 std::unique_ptr<StringFormat> formatFar;
 std::unique_ptr<SolidBrush> scratchBrush;
 std::unique_ptr<Pen> scratchPen;

 std::array<double,kParamCount> lastParamValues{};
 clap_id lastDisplayId = CLAP_INVALID_ID;
 double lastDisplayValue = std::numeric_limits<double>::quiet_NaN();

 GuiState(){
  lastParamValues.fill(std::numeric_limits<double>::quiet_NaN());
 }
};

static ULONG_PTR gGdiToken = 0;
static HINSTANCE gModule = nullptr;
static bool gInitialized = false;
static int gModuleAnchor = 0;

Color colorBg(){return Color(255,12,14,17);}
Color colorPanel(){return Color(255,22,25,29);}
Color colorPanel2(){return Color(255,17,20,23);}
Color colorLine(){return Color(255,62,68,73);}
Color colorText(){return Color(255,220,224,226);}
Color colorMuted(){return Color(255,125,132,138);}
Color colorOrange(){return Color(255,255,143,32);}
Color colorOrangeDim(){return Color(255,117,66,22);}
Color colorTeal(){return Color(255,58,208,220);}
Color colorTealDim(){return Color(255,27,99,106);}

void makeRoundedRect(GraphicsPath& p,const RectF& r,float radius){
 const float d=radius*2.0f;
 p.AddArc(r.X,r.Y,d,d,180.0f,90.0f);
 p.AddArc(r.GetRight()-d,r.Y,d,d,270.0f,90.0f);
 p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0.0f,90.0f);
 p.AddArc(r.X,r.GetBottom()-d,d,d,90.0f,90.0f);
 p.CloseFigure();
}

void destroySurface(GdiSurface& s){
 if(s.dc&&s.oldBitmap)SelectObject(s.dc,s.oldBitmap);
 if(s.bitmap)DeleteObject(s.bitmap);
 if(s.dc)DeleteDC(s.dc);
 s={};
}

bool ensureSurface(GdiSurface& s,HDC reference,int width,int height){
 if(s.dc&&s.bitmap&&s.width==width&&s.height==height)return true;
 destroySurface(s);
 s.dc=CreateCompatibleDC(reference);
 if(!s.dc)return false;
 s.bitmap=CreateCompatibleBitmap(reference,width,height);
 if(!s.bitmap){destroySurface(s);return false;}
 s.oldBitmap=SelectObject(s.dc,s.bitmap);
 s.width=width;s.height=height;
 return true;
}

void destroyRenderResources(GuiState* ui){
 if(!ui)return;
 destroySurface(ui->staticLayer);
 destroySurface(ui->backBuffer);
 ui->knobBodies.clear();
 ui->fonts.clear();
 ui->fontFamily.reset();
 ui->formatCenter.reset();
 ui->formatNear.reset();
 ui->formatFar.reset();
 ui->scratchBrush.reset();
 ui->scratchPen.reset();
}

void ensureTextResources(GuiState* ui){
 if(ui->fontFamily)return;
 ui->fontFamily=std::make_unique<FontFamily>(L"Segoe UI");
 ui->formatCenter=std::make_unique<StringFormat>();
 ui->formatNear=std::make_unique<StringFormat>();
 ui->formatFar=std::make_unique<StringFormat>();
 for(auto* f:{ui->formatCenter.get(),ui->formatNear.get(),ui->formatFar.get()}){
  f->SetLineAlignment(StringAlignmentCenter);
  f->SetTrimming(StringTrimmingEllipsisCharacter);
  f->SetFormatFlags(StringFormatFlagsNoWrap);
 }
 ui->formatCenter->SetAlignment(StringAlignmentCenter);
 ui->formatNear->SetAlignment(StringAlignmentNear);
 ui->formatFar->SetAlignment(StringAlignmentFar);
 ui->scratchBrush=std::make_unique<SolidBrush>(colorText());
 ui->scratchPen=std::make_unique<Pen>(colorLine(),1.0f);
}

Font* getFont(GuiState* ui,float size,FontStyle style){
 ensureTextResources(ui);
 const int key=(int)std::lround(size*10.0f)*16+(int)style;
 auto it=ui->fonts.find(key);
 if(it!=ui->fonts.end())return it->second.get();
 auto font=std::make_unique<Font>(ui->fontFamily.get(),size,style,UnitPixel);
 Font* result=font.get();
 ui->fonts.emplace(key,std::move(font));
 return result;
}

SolidBrush* setBrush(GuiState* ui,const Color& c){
 ensureTextResources(ui);ui->scratchBrush->SetColor(c);return ui->scratchBrush.get();
}

Pen* setPen(GuiState* ui,const Color& c,float width=1.0f){
 ensureTextResources(ui);
 ui->scratchPen->SetColor(c);ui->scratchPen->SetWidth(width);
 ui->scratchPen->SetStartCap(LineCapFlat);ui->scratchPen->SetEndCap(LineCapFlat);
 return ui->scratchPen.get();
}

void fillRounded(Graphics&g,GuiState*ui,const RectF&r,float radius,const Color&fill,const Color&stroke){
 GraphicsPath path;makeRoundedRect(path,r,radius);
 g.FillPath(setBrush(ui,fill),&path);
 g.DrawPath(setPen(ui,stroke,1.0f),&path);
}

void drawText(Graphics&g,GuiState*ui,const wchar_t*text,const RectF&r,float size,const Color&c,
              FontStyle style=FontStyleRegular,StringAlignment align=StringAlignmentCenter){
 ensureTextResources(ui);
 StringFormat* format=ui->formatCenter.get();
 if(align==StringAlignmentNear)format=ui->formatNear.get();
 else if(align==StringAlignmentFar)format=ui->formatFar.get();
 g.DrawString(text,-1,getFont(ui,size,style),r,format,setBrush(ui,c));
}

void drawSection(Graphics&g,GuiState*ui,const RectF&r,const wchar_t*title,bool orange){
 fillRounded(g,ui,r,8.0f,colorPanel(),colorLine());
 const Color accent=orange?colorOrange():colorTeal();
 drawText(g,ui,title,RectF(r.X+18,r.Y+9,r.Width-36,28),15.0f,accent,FontStyleBold);
 g.DrawLine(setPen(ui,accent,1.0f),r.X+18,r.Y+24,r.X+75,r.Y+24);
 g.DrawLine(setPen(ui,accent,1.0f),r.GetRight()-75,r.Y+24,r.GetRight()-18,r.Y+24);
}

float normalizeParam(clap_id id,double value){
 const auto&d=defs[id];
 const double range=d.max-d.min;
 if(range<=0.0)return 0.0f;
 return (float)clamp((value-d.min)/range,0.0,1.0);
}

const wchar_t* labelFor(clap_id id){
 for(const auto&k:kKnobs)if(k.id==id)return k.label;
 if(id==AutoGain)return L"Auto-Gain";
 if(id==X2)return L"×2 Color";
 if(id==ApplyAutoGain)return L"Commit Auto-Gain";
 return L"Drive";
}

void formatParam(GrowlForge*s,clap_id id,wchar_t*out,size_t n){
 if(!out||n==0)return;
 const double value=(id<kParamCount)?s->parameters.values[id].load():0.0;
 if(id==AutoGain||id==X2){std::swprintf(out,n,L"%ls",value>=0.5?L"ON":L"OFF");return;}
 if(id==Input||id==Output||id==Ceiling||id==AutoGainCorrection){std::swprintf(out,n,L"%.1f dB",value);return;}
 if(id==ParallelDry){std::swprintf(out,n,L"%.1f %%",value);return;}
 std::swprintf(out,n,L"%.1f",value);
}

const KnobBodyCache* getKnobBody(GuiState* ui,float radius,bool orange,float renderScale){
 for(const auto&entry:ui->knobBodies){
  if(entry.radius==radius&&entry.orange==orange&&std::abs(entry.scale-renderScale)<0.001f)return &entry;
 }
 KnobBodyCache entry;
 entry.radius=radius;entry.orange=orange;entry.scale=renderScale;
 entry.logicalSide=std::ceil((radius+15.0f)*2.0f+4.0f);
 const int pixelSide=std::max(1,(int)std::lround(entry.logicalSide*renderScale));
 entry.bitmap=std::make_unique<Bitmap>(pixelSide,pixelSide,PixelFormat32bppPARGB);
 if(!entry.bitmap||entry.bitmap->GetLastStatus()!=Ok)return nullptr;
 {
  Graphics g(entry.bitmap.get());
  g.Clear(Color(0,0,0,0));
  g.SetSmoothingMode(SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
  g.ScaleTransform(renderScale,renderScale);
  const float c=entry.logicalSide*0.5f;
  const float r=radius;
  const float start=135.0f,sweep=270.0f;
  const Color dim=orange?colorOrangeDim():colorTealDim();
  Pen baseArc(Color(255,48,53,57),3.0f);
  g.DrawArc(&baseArc,RectF(c-r-6.0f,c-r-6.0f,2.0f*(r+6.0f),2.0f*(r+6.0f)),start,sweep);
  const int tickCount=21;
  for(int i=0;i<tickCount;++i){
   const float t=(float)i/(tickCount-1);
   const float angle=(start+sweep*t)*(float)kPi/180.0f;
   const float ro=r+11.0f,ri=r+7.0f;
   Pen tick(dim,0.8f);
   g.DrawLine(&tick,c+std::cos(angle)*ri,c+std::sin(angle)*ri,
                    c+std::cos(angle)*ro,c+std::sin(angle)*ro);
  }
  RectF outer(c-r,c-r,2.0f*r,2.0f*r);
  GraphicsPath knobPath;knobPath.AddEllipse(outer);PathGradientBrush pg(&knobPath);
  Color center(255,50,53,56),surround(255,16,18,20);
  pg.SetCenterColor(center);INT count=1;pg.SetSurroundColors(&surround,&count);
  g.FillEllipse(&pg,outer);
  Pen edge(Color(255,88,92,96),1.3f);g.DrawEllipse(&edge,outer);
  RectF inner(c-r*0.78f,c-r*0.78f,2.0f*r*0.78f,2.0f*r*0.78f);
  LinearGradientBrush innerBrush(inner,Color(255,48,50,52),Color(255,19,21,23),LinearGradientModeVertical);
  g.FillEllipse(&innerBrush,inner);Pen innerEdge(Color(255,12,13,14),1.0f);g.DrawEllipse(&innerEdge,inner);
 }
 ui->knobBodies.emplace_back(std::move(entry));
 return &ui->knobBodies.back();
}

void drawKnobStatic(Graphics&g,GuiState*ui,const KnobDef&k,float renderScale){
 if(const auto*body=getKnobBody(ui,k.radius,k.orange,renderScale)){
  const float side=body->logicalSide;
  g.DrawImage(body->bitmap.get(),RectF(k.x-side*0.5f,k.y-side*0.5f,side,side));
 }
 const Color accent=k.orange?colorOrange():colorTeal();
 RectF lr(k.x-k.radius*1.7f,k.y+k.radius+8.0f,k.radius*3.4f,25.0f);
 drawText(g,ui,k.label,lr,k.id==Drive?16.0f:13.5f,k.id==Drive?accent:colorText(),k.id==Drive?FontStyleBold:FontStyleRegular);
 if(k.activity!=kNoActivity){
  const float w=54.0f,h=5.0f,x=k.x-w*0.5f,y=k.y+k.radius+34.0f;
  const int seg=10;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;
  for(int i=0;i<seg;++i)g.FillRectangle(setBrush(ui,Color(255,42,47,50)),RectF(x+i*(sw+gap),y,sw,h));
 }
}

void drawKnobDynamic(Graphics&g,GuiState*ui,const KnobDef&k){
 const bool hovered=(ui->hover==k.id||ui->drag==k.id);
 const float n=normalizeParam(k.id,ui->owner->parameters.values[k.id].load());
 const Color accent=k.orange?colorOrange():colorTeal();
 const float r=k.radius;
 const float start=135.0f,sweep=270.0f;
 if(n>0.001f){
  Pen valueArc(accent,hovered?3.4f:2.7f);
  g.DrawArc(&valueArc,RectF(k.x-r-6.0f,k.y-r-6.0f,2.0f*(r+6.0f),2.0f*(r+6.0f)),start,sweep*n);
 }
 const int tickCount=21;
 for(int i=0;i<tickCount;++i){
  const float t=(float)i/(tickCount-1);
  if(t>n+0.001f)continue;
  const float angle=(start+sweep*t)*(float)kPi/180.0f;
  const float ro=r+11.0f,ri=r+7.0f;
  Pen tick(accent,1.4f);
  g.DrawLine(&tick,k.x+std::cos(angle)*ri,k.y+std::sin(angle)*ri,
                  k.x+std::cos(angle)*ro,k.y+std::sin(angle)*ro);
 }
 if(hovered){Pen edge(accent,2.0f);g.DrawEllipse(&edge,RectF(k.x-r,k.y-r,2.0f*r,2.0f*r));}
 const float pointerAngle=(start+sweep*n)*(float)kPi/180.0f;
 Pen pointer(Color(255,244,244,242),std::max(2.0f,r*0.055f));
 pointer.SetStartCap(LineCapRound);pointer.SetEndCap(LineCapRound);
 g.DrawLine(&pointer,k.x,k.y,k.x+std::cos(pointerAngle)*r*0.61f,k.y+std::sin(pointerAngle)*r*0.61f);
}

void drawKnobActivityDynamic(Graphics&g,GuiState*ui,const KnobDef&k){
 if(k.activity==kNoActivity)return;
 const Color accent=k.orange?colorOrange():colorTeal();
 const float v=(float)clamp(ui->owner->parameters.values[k.activity].load()/100.0,0.0,1.0);
 const float w=54.0f,h=5.0f,x=k.x-w*0.5f,y=k.y+k.radius+34.0f;
 const int seg=10;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;
 for(int i=0;i<seg;++i){
  if((float)(i+1)/seg>v+0.001f)break;
  g.FillRectangle(setBrush(ui,accent),RectF(x+i*(sw+gap),y,sw,h));
 }
}

void drawToggleStatic(Graphics&g,GuiState*ui,const RectF&r,const wchar_t*label){
 drawText(g,ui,label,RectF(r.X-12.0f,r.Y-24.0f,r.Width+24.0f,20.0f),13.0f,colorText());
}

void drawToggleDynamic(Graphics&g,GuiState*ui,const RectF&r,bool on,bool orange,bool hovered){
 const Color accent=orange?colorOrange():colorTeal();
 fillRounded(g,ui,r,7.0f,on?Color(255,35,50,52):Color(255,26,29,32),hovered?accent:colorLine());
 const float knob=r.Height-10.0f;
 const float x=on?r.GetRight()-knob-5.0f:r.X+5.0f;
 g.FillEllipse(setBrush(ui,on?accent:Color(255,93,98,101)),RectF(x,r.Y+5.0f,knob,knob));
 drawText(g,ui,on?L"ON":L"OFF",RectF(r.X,r.GetBottom()+3.0f,r.Width,16.0f),10.5f,on?accent:colorMuted(),FontStyleBold);
}

void drawSaturationStatic(Graphics&g,GuiState*ui){
 const float x=405.0f,y=418.0f,w=390.0f;
 const int count=40;const float gap=2.0f;const float sw=(w-gap*(count-1))/count;
 drawText(g,ui,L"SATURATION ACTIVITY",RectF(x,y-25.0f,w,20.0f),12.0f,colorOrange(),FontStyleBold);
 for(int i=0;i<count;++i)g.FillRectangle(setBrush(ui,Color(255,43,35,27)),RectF(x+i*(sw+gap),y,sw,13.0f));
 g.DrawRectangle(setPen(ui,Color(255,57,59,61),1.0f),RectF(x-3.0f,y-3.0f,w+6.0f,19.0f));
}

void drawSaturationDynamic(Graphics&g,GuiState*ui){
 const float x=405.0f,y=418.0f,w=390.0f;
 const int count=40;const float gap=2.0f;const float sw=(w-gap*(count-1))/count;
 const float n=(float)clamp(ui->owner->parameters.values[MeterSaturation].load()/100.0,0.0,1.0);
 for(int i=0;i<count;++i){
  const float t=(float)(i+1)/count;
  if(t>n)break;
  Color c=t<0.72f?Color(255,225,100+(BYTE)(55*t),22):Color(255,255,178+(BYTE)(50*(t-0.72f)/0.28f),40);
  g.FillRectangle(setBrush(ui,c),RectF(x+i*(sw+gap),y,sw,13.0f));
 }
}

float meterNormalized(float peak){
 const float db=20.0f*std::log10(std::max(peak,1.0e-5f));
 return (float)clamp((db+60.0f)/60.0f,0.0,1.12);
}

void drawStereoMeterStatic(Graphics&g,GuiState*ui,float x,float y,float w,bool output){
 drawText(g,ui,output?L"OUTPUT METER":L"INPUT METER",RectF(x,y-25.0f,w,20.0f),12.0f,colorMuted(),FontStyleBold);
 const int seg=48;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;const float h=8.0f;
 for(int chn=0;chn<2;++chn){
  drawText(g,ui,chn==0?L"L":L"R",RectF(x-26.0f,y+chn*17.0f-4.0f,18.0f,16.0f),11.0f,colorText(),FontStyleBold);
  for(int i=0;i<seg;++i)g.FillRectangle(setBrush(ui,Color(255,35,39,41)),RectF(x+i*(sw+gap),y+chn*17.0f,sw,h));
 }
 const wchar_t* marks[]={L"-60",L"-48",L"-36",L"-24",L"-12",L"-6",L"0"};
 const float positions[]={0.0f,0.2f,0.4f,0.6f,0.8f,0.9f,1.0f};
 const float labelY=y+34.0f;
 for(int i=0;i<7;++i){
  if(i==0)drawText(g,ui,marks[i],RectF(x,labelY,40.0f,11.0f),9.5f,colorMuted(),FontStyleRegular,StringAlignmentNear);
  else if(i==6)drawText(g,ui,marks[i],RectF(x+w-40.0f,labelY,40.0f,11.0f),9.5f,colorMuted(),FontStyleRegular,StringAlignmentFar);
  else drawText(g,ui,marks[i],RectF(x+w*positions[i]-18.0f,labelY,36.0f,11.0f),9.5f,colorMuted());
 }
}

void drawStereoMeterDynamic(Graphics&g,GuiState*ui,float x,float y,float w,bool output){
 const int seg=48;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;const float h=8.0f;
 for(int chn=0;chn<2;++chn){
  const float peak=output?ui->owner->guiOutputPeak[chn].load():ui->owner->guiInputPeak[chn].load();
  const float n=meterNormalized(peak);
  for(int i=0;i<seg;++i){
   const float t=(float)(i+1)/seg;
   if(t>n)break;
   const Color active=t<0.72f?Color(255,75,215,54):(t<0.9f?Color(255,248,177,33):Color(255,241,61,28));
   g.FillRectangle(setBrush(ui,active),RectF(x+i*(sw+gap),y+chn*17.0f,sw,h));
  }
 }
}

void drawStaticUi(Graphics&g,GuiState*ui,float renderScale){
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
 g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
 g.FillRectangle(setBrush(ui,colorBg()),RectF(0.0f,0.0f,kDesignWidth,kDesignHeight));

 fillRounded(g,ui,RectF(8.0f,8.0f,1184.0f,72.0f),9.0f,colorPanel2(),colorLine());
 drawText(g,ui,L"GROWLFORGE",RectF(30.0f,19.0f,280.0f,42.0f),31.0f,colorText(),FontStyleBold,StringAlignmentNear);
 drawText(g,ui,L"2.0",RectF(278.0f,26.0f,70.0f,26.0f),17.0f,colorOrange(),FontStyleBold,StringAlignmentNear);
 fillRounded(g,ui,RectF(430.0f,15.0f,340.0f,57.0f),7.0f,Color(255,8,10,12),Color(255,56,59,62));

 drawSection(g,ui,RectF(10.0f,90.0f,330.0f,360.0f),L"INPUT & FEEL",false);
 drawSection(g,ui,RectF(348.0f,90.0f,504.0f,360.0f),L"DISTORTION CORE",true);
 drawSection(g,ui,RectF(860.0f,90.0f,330.0f,360.0f),L"MOTION & DYNAMICS",false);
 drawSection(g,ui,RectF(10.0f,458.0f,650.0f,170.0f),L"TONE & TEXTURE",false);
 drawSection(g,ui,RectF(668.0f,458.0f,522.0f,170.0f),L"ROUTING & OUTPUT",false);

 for(const auto&k:kKnobs)drawKnobStatic(g,ui,k,renderScale);
 drawSaturationStatic(g,ui);

 const RectF ag(881.0f,510.0f,82.0f,38.0f);
 drawToggleStatic(g,ui,ag,L"Auto-Gain");

 fillRounded(g,ui,RectF(10.0f,634.0f,1180.0f,78.0f),7.0f,colorPanel2(),colorLine());
 drawStereoMeterStatic(g,ui,52.0f,665.0f,420.0f,false);
 drawStereoMeterStatic(g,ui,728.0f,665.0f,420.0f,true);
 drawText(g,ui,L"GROWLFORGE 2.0  •  CLAP",RectF(485.0f,653.0f,230.0f,26.0f),10.5f,Color(255,71,76,80),FontStyleBold);
}

clap_id activeDisplayId(const GuiState*ui){
 return ui->drag!=CLAP_INVALID_ID?ui->drag:(ui->hover!=CLAP_INVALID_ID?ui->hover:Drive);
}

RectF knobCircleRect(const KnobDef&k);
RectF knobActivityRect(const KnobDef&k);
RectF knobDynamicRect(const KnobDef&k);

bool rectIntersects(const RectF&a,const RectF&b){
 return a.X<b.GetRight()&&a.GetRight()>b.X&&a.Y<b.GetBottom()&&a.GetBottom()>b.Y;
}

void drawDynamicUi(Graphics&g,GuiState*ui,const RectF&dirty){
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
 g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

 const RectF displayRect(428.0f,13.0f,344.0f,61.0f);
 if(rectIntersects(dirty,displayRect)){
  const clap_id active=activeDisplayId(ui);
  wchar_t value[64]{};formatParam(ui->owner,active,value,64);
  drawText(g,ui,labelFor(active),RectF(445.0f,19.0f,310.0f,20.0f),12.5f,active==Drive?colorOrange():colorTeal(),FontStyleBold);
  drawText(g,ui,value,RectF(445.0f,35.0f,310.0f,32.0f),24.0f,colorText(),FontStyleBold);
 }

 const RectF x2r(1032.0f,22.0f,142.0f,43.0f);
 if(rectIntersects(dirty,x2r)){
  const bool x2=ui->owner->parameters.values[X2].load()>=0.5;
  fillRounded(g,ui,x2r,7.0f,x2?Color(255,65,38,15):Color(255,24,27,29),(ui->hover==X2||x2)?colorOrange():colorLine());
  drawText(g,ui,L"×2 COLOR",x2r,16.0f,x2?colorOrange():colorText(),FontStyleBold);
 }

 for(const auto&k:kKnobs){
  if(rectIntersects(dirty,knobCircleRect(k)))drawKnobDynamic(g,ui,k);
  if(k.activity!=kNoActivity&&rectIntersects(dirty,knobActivityRect(k)))drawKnobActivityDynamic(g,ui,k);
 }
 const RectF saturationRect(401.0f,414.0f,398.0f,21.0f);
 if(rectIntersects(dirty,saturationRect))drawSaturationDynamic(g,ui);

 const RectF autoGainRect(868.0f,506.0f,108.0f,64.0f);
 if(rectIntersects(dirty,autoGainRect)){
  const RectF ag(881.0f,510.0f,82.0f,38.0f);
  drawToggleDynamic(g,ui,ag,ui->owner->parameters.values[AutoGain].load()>=0.5,false,ui->hover==AutoGain);
 }
 const RectF commitRect(870.0f,569.0f,104.0f,31.0f);
 if(rectIntersects(dirty,commitRect)){
  wchar_t correction[64]{};std::swprintf(correction,64,L"COMMIT %+.1f dB",ui->owner->parameters.values[AutoGainCorrection].load());
  const RectF commit(873.0f,572.0f,98.0f,25.0f);
  fillRounded(g,ui,commit,5.0f,Color(255,25,29,31),ui->hover==ApplyAutoGain?colorTeal():colorLine());
  drawText(g,ui,correction,commit,9.5f,ui->owner->parameters.values[AutoGain].load()>=0.5?colorTeal():colorMuted(),FontStyleBold);
 }

 const RectF inputMeterRect(48.0f,660.0f,428.0f,31.0f);
 const RectF outputMeterRect(724.0f,660.0f,428.0f,31.0f);
 if(rectIntersects(dirty,inputMeterRect))drawStereoMeterDynamic(g,ui,52.0f,665.0f,420.0f,false);
 if(rectIntersects(dirty,outputMeterRect))drawStereoMeterDynamic(g,ui,728.0f,665.0f,420.0f,true);
}

PointF logicalPoint(GuiState*ui,int px,int py){
 RECT r{};GetClientRect(ui->hwnd,&r);
 const float w=(float)std::max(1L,r.right-r.left),h=(float)std::max(1L,r.bottom-r.top);
 return PointF(px*kDesignWidth/w,py*kDesignHeight/h);
}

RECT logicalToPixelRect(GuiState*ui,const RectF&r,float padding=2.0f){
 RECT client{};GetClientRect(ui->hwnd,&client);
 const float sx=(float)std::max(1L,client.right-client.left)/kDesignWidth;
 const float sy=(float)std::max(1L,client.bottom-client.top)/kDesignHeight;
 RECT out{};
 out.left=(LONG)std::floor((r.X-padding)*sx);
 out.top=(LONG)std::floor((r.Y-padding)*sy);
 out.right=(LONG)std::ceil((r.GetRight()+padding)*sx);
 out.bottom=(LONG)std::ceil((r.GetBottom()+padding)*sy);
 out.left=std::max<LONG>(0,out.left);out.top=std::max<LONG>(0,out.top);
 out.right=std::min<LONG>(client.right,out.right);out.bottom=std::min<LONG>(client.bottom,out.bottom);
 return out;
}

void invalidateLogical(GuiState*ui,const RectF&r,float padding=2.0f){
 if(!ui||!ui->hwnd)return;RECT pr=logicalToPixelRect(ui,r,padding);InvalidateRect(ui->hwnd,&pr,FALSE);
}

RectF knobCircleRect(const KnobDef&k){
 const float left=k.x-k.radius-14.0f;
 const float top=k.y-k.radius-14.0f;
 const float side=2.0f*(k.radius+14.0f);
 return RectF(left,top,side,side);
}

RectF knobActivityRect(const KnobDef&k){
 return RectF(k.x-29.0f,k.y+k.radius+31.0f,58.0f,10.0f);
}

RectF knobDynamicRect(const KnobDef&k){
 const RectF circle=knobCircleRect(k);
 if(k.activity==kNoActivity)return circle;
 const RectF activity=knobActivityRect(k);
 const float left=std::min(circle.X,activity.X);
 const float top=std::min(circle.Y,activity.Y);
 const float right=std::max(circle.GetRight(),activity.GetRight());
 const float bottom=std::max(circle.GetBottom(),activity.GetBottom());
 return RectF(left,top,right-left,bottom-top);
}

const KnobDef* findKnob(clap_id id){for(const auto&k:kKnobs)if(k.id==id)return &k;return nullptr;}

void invalidateDisplay(GuiState*ui){invalidateLogical(ui,RectF(428.0f,13.0f,344.0f,61.0f),1.0f);}
void paintLogicalNow(GuiState*ui,const RectF&r,float padding=1.0f){
 invalidateLogical(ui,r,padding);
 if(ui&&ui->hwnd)UpdateWindow(ui->hwnd);
}

void invalidateControl(GuiState*ui,clap_id id){
 if(const auto*k=findKnob(id)){invalidateLogical(ui,knobDynamicRect(*k),2.0f);return;}
 if(id==X2){invalidateLogical(ui,RectF(1029.0f,19.0f,148.0f,49.0f),2.0f);return;}
 if(id==AutoGain){invalidateLogical(ui,RectF(868.0f,506.0f,108.0f,64.0f),2.0f);invalidateLogical(ui,RectF(870.0f,569.0f,104.0f,31.0f),2.0f);return;}
 if(id==ApplyAutoGain||id==AutoGainCorrection){invalidateLogical(ui,RectF(870.0f,569.0f,104.0f,31.0f),2.0f);return;}
}

const KnobDef* knobAt(float x,float y){
 for(const auto&k:kKnobs){const float dx=x-k.x,dy=y-k.y;if(dx*dx+dy*dy<=(k.radius+12.0f)*(k.radius+12.0f))return &k;}
 return nullptr;
}

bool inRect(float x,float y,const RectF&r){return x>=r.X&&x<=r.GetRight()&&y>=r.Y&&y<=r.GetBottom();}

clap_id controlAt(float x,float y){
 if(const auto*k=knobAt(x,y))return k->id;
 if(inRect(x,y,RectF(1032.0f,22.0f,142.0f,43.0f)))return X2;
 if(inRect(x,y,RectF(881.0f,510.0f,82.0f,38.0f)))return AutoGain;
 if(inRect(x,y,RectF(873.0f,572.0f,98.0f,25.0f)))return ApplyAutoGain;
 return CLAP_INVALID_ID;
}

void invalidateFull(GuiState*ui){if(ui&&ui->hwnd)InvalidateRect(ui->hwnd,nullptr,FALSE);}

void invalidateHoverTransition(GuiState*ui,clap_id oldId,clap_id newId){
 if(oldId!=CLAP_INVALID_ID)invalidateControl(ui,oldId);
 if(newId!=CLAP_INVALID_ID)invalidateControl(ui,newId);
 invalidateDisplay(ui);
}

void resetSnapshots(GuiState*ui){
 ui->lastParamValues.fill(std::numeric_limits<double>::quiet_NaN());
 ui->lastDisplayId=CLAP_INVALID_ID;
 ui->lastDisplayValue=std::numeric_limits<double>::quiet_NaN();
}

void timerTick(GuiState*ui){
 if(!ui||!ui->shown)return;

 // Keep the 30 Hz animation paints small and separate so Windows does not
 // coalesce distant meter/activity rectangles into one large repaint.
 paintLogicalNow(ui,RectF(48.0f,660.0f,1104.0f,31.0f),1.0f);
 paintLogicalNow(ui,RectF(401.0f,414.0f,398.0f,21.0f),1.0f);
 paintLogicalNow(ui,RectF(888.0f,239.0f,168.0f,15.0f),1.0f);
 paintLogicalNow(ui,RectF(888.0f,390.0f,168.0f,15.0f),1.0f);

 bool parameterPaintPending=false;
 for(const auto&k:kKnobs){
  const double value=ui->owner->parameters.values[k.id].load();
  if(!std::isfinite(ui->lastParamValues[k.id])||std::abs(value-ui->lastParamValues[k.id])>1.0e-7){
   invalidateControl(ui,k.id);ui->lastParamValues[k.id]=value;parameterPaintPending=true;
  }
 }
 for(const clap_id id:{X2,AutoGain,AutoGainCorrection}){
  const double value=ui->owner->parameters.values[id].load();
  if(!std::isfinite(ui->lastParamValues[id])||std::abs(value-ui->lastParamValues[id])>1.0e-7){
   invalidateControl(ui,id);ui->lastParamValues[id]=value;parameterPaintPending=true;
  }
 }
 const clap_id displayId=activeDisplayId(ui);
 const double displayValue=ui->owner->parameters.values[displayId].load();
 if(displayId!=ui->lastDisplayId||!std::isfinite(ui->lastDisplayValue)||std::abs(displayValue-ui->lastDisplayValue)>1.0e-7){
  invalidateDisplay(ui);ui->lastDisplayId=displayId;ui->lastDisplayValue=displayValue;parameterPaintPending=true;
 }
 if(parameterPaintPending)UpdateWindow(ui->hwnd);
}

void toggleParam(GuiState*ui,clap_id id){
 ui->owner->beginGuiGesture(id);
 ui->owner->setGuiParameter(id,ui->owner->parameters.values[id].load()>=0.5?0.0:1.0);
 ui->owner->endGuiGesture(id);
 invalidateControl(ui,id);invalidateDisplay(ui);
}

bool ensureRenderSurfaces(GuiState*ui,HDC reference,int width,int height){
 const bool resized=ui->staticLayer.width!=width||ui->staticLayer.height!=height||ui->backBuffer.width!=width||ui->backBuffer.height!=height;
 if(!ensureSurface(ui->staticLayer,reference,width,height))return false;
 if(!ensureSurface(ui->backBuffer,reference,width,height))return false;
 if(resized){ui->staticDirty=true;ui->knobBodies.clear();}
 return true;
}

void renderStaticLayer(GuiState*ui){
 if(!ui||!ui->staticLayer.dc)return;
 Graphics g(ui->staticLayer.dc);
 const float sx=(float)ui->staticLayer.width/kDesignWidth;
 const float sy=(float)ui->staticLayer.height/kDesignHeight;
 g.ScaleTransform(sx,sy);
 drawStaticUi(g,ui,std::max(sx,sy));
 ui->staticDirty=false;
}

LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 GuiState*ui=reinterpret_cast<GuiState*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
 if(msg==WM_NCCREATE){
  auto*cs=reinterpret_cast<CREATESTRUCTW*>(lp);ui=static_cast<GuiState*>(cs->lpCreateParams);
  SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ui));ui->hwnd=hwnd;
 }
 if(!ui)return DefWindowProcW(hwnd,msg,wp,lp);
 switch(msg){
  case WM_ERASEBKGND:return 1;
  case WM_TIMER:if(wp==kAnimationTimer)timerTick(ui);return 0;
  case WM_SIZE:
   ui->width=LOWORD(lp);ui->height=HIWORD(lp);ui->staticDirty=true;ui->knobBodies.clear();resetSnapshots(ui);invalidateFull(ui);return 0;
  case WM_SETCURSOR:{
   POINT p{};GetCursorPos(&p);ScreenToClient(hwnd,&p);auto l=logicalPoint(ui,p.x,p.y);
   SetCursor(LoadCursorW(nullptr,controlAt(l.X,l.Y)!=CLAP_INVALID_ID?IDC_HAND:IDC_ARROW));return TRUE;
  }
  case WM_MOUSEMOVE:{
   if(!ui->trackingMouse){TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hwnd,0};TrackMouseEvent(&tme);ui->trackingMouse=true;}
   auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));
   if(ui->drag!=CLAP_INVALID_ID){
    const auto&d=defs[ui->drag];const bool fine=(GetKeyState(VK_SHIFT)&0x8000)!=0;
    const double delta=(ui->dragStartY-GET_Y_LPARAM(lp))/(fine?1500.0:180.0);
    const double value=ui->dragStartValue+delta*(d.max-d.min);
    ui->owner->setGuiParameter(ui->drag,value);ui->hover=ui->drag;
    invalidateControl(ui,ui->drag);invalidateDisplay(ui);UpdateWindow(hwnd);return 0;
   }
   const clap_id h=controlAt(l.X,l.Y);
   if(h!=ui->hover){const clap_id old=ui->hover;ui->hover=h;invalidateHoverTransition(ui,old,h);}return 0;
  }
  case WM_MOUSELEAVE:
   ui->trackingMouse=false;
   if(ui->drag==CLAP_INVALID_ID&&ui->hover!=CLAP_INVALID_ID){const clap_id old=ui->hover;ui->hover=CLAP_INVALID_ID;invalidateHoverTransition(ui,old,CLAP_INVALID_ID);}return 0;
  case WM_LBUTTONDOWN:{
   SetFocus(hwnd);auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));const clap_id id=controlAt(l.X,l.Y);
   const clap_id old=ui->hover;ui->hover=id;if(old!=id)invalidateHoverTransition(ui,old,id);
   if(id==X2||id==AutoGain){toggleParam(ui,id);return 0;}
   if(id==ApplyAutoGain){ui->owner->setGuiParameter(ApplyAutoGain,1.0);invalidateControl(ui,ApplyAutoGain);invalidateDisplay(ui);return 0;}
   if(id!=CLAP_INVALID_ID){ui->drag=id;ui->dragStartY=GET_Y_LPARAM(lp);ui->dragStartValue=ui->owner->parameters.values[id].load();
    ui->owner->beginGuiGesture(id);SetCapture(hwnd);invalidateControl(ui,id);invalidateDisplay(ui);return 0;}
   return 0;
  }
  case WM_LBUTTONUP:
   if(ui->drag!=CLAP_INVALID_ID){const clap_id id=ui->drag;ui->owner->endGuiGesture(id);ui->drag=CLAP_INVALID_ID;ReleaseCapture();invalidateControl(ui,id);invalidateDisplay(ui);}return 0;
  case WM_LBUTTONDBLCLK:{
   auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));const clap_id id=controlAt(l.X,l.Y);
   if(id!=CLAP_INVALID_ID&&id!=ApplyAutoGain){ui->owner->beginGuiGesture(id);ui->owner->setGuiParameter(id,defs[id].def);ui->owner->endGuiGesture(id);invalidateControl(ui,id);invalidateDisplay(ui);}return 0;
  }
  case WM_MOUSEWHEEL:{
   POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&p);auto l=logicalPoint(ui,p.x,p.y);const clap_id id=controlAt(l.X,l.Y);
   if(id!=CLAP_INVALID_ID&&id!=X2&&id!=AutoGain&&id!=ApplyAutoGain){
    const double step=((GetKeyState(VK_SHIFT)&0x8000)!=0)?0.1:((defs[id].max-defs[id].min)>24.0?1.0:0.2);
    const double value=ui->owner->parameters.values[id].load()+(GET_WHEEL_DELTA_WPARAM(wp)>0?step:-step);
    ui->owner->beginGuiGesture(id);ui->owner->setGuiParameter(id,value);ui->owner->endGuiGesture(id);
    const clap_id old=ui->hover;ui->hover=id;if(old!=id)invalidateHoverTransition(ui,old,id);
    invalidateControl(ui,id);invalidateDisplay(ui);UpdateWindow(hwnd);
   }return 0;
  }
  case WM_PAINT:{
   PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT cr{};GetClientRect(hwnd,&cr);
   const int w=std::max(1L,cr.right-cr.left),h=std::max(1L,cr.bottom-cr.top);
   if(ensureRenderSurfaces(ui,dc,w,h)){
    if(ui->staticDirty)renderStaticLayer(ui);
    RECT dirty=ps.rcPaint;
    if(dirty.right<=dirty.left||dirty.bottom<=dirty.top)dirty=cr;
    const int dw=dirty.right-dirty.left,dh=dirty.bottom-dirty.top;
    BitBlt(ui->backBuffer.dc,dirty.left,dirty.top,dw,dh,ui->staticLayer.dc,dirty.left,dirty.top,SRCCOPY);
    {
     Graphics g(ui->backBuffer.dc);
     const float sx=(float)w/kDesignWidth,sy=(float)h/kDesignHeight;
     g.ScaleTransform(sx,sy);
     const RectF logicalDirty(dirty.left/sx,dirty.top/sy,dw/sx,dh/sy);
     g.SetClip(logicalDirty,CombineModeReplace);
     drawDynamicUi(g,ui,logicalDirty);
    }
    BitBlt(dc,dirty.left,dirty.top,dw,dh,ui->backBuffer.dc,dirty.left,dirty.top,SRCCOPY);
   }
   EndPaint(hwnd,&ps);return 0;
  }
  case WM_DESTROY:
   KillTimer(hwnd,kAnimationTimer);destroyRenderResources(ui);ui->hwnd=nullptr;return 0;
 }
 return DefWindowProcW(hwnd,msg,wp,lp);
}

bool globalInit(){
 if(gInitialized)return true;
 GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&gModuleAnchor),&gModule);
 GdiplusStartupInput input;if(GdiplusStartup(&gGdiToken,&input,nullptr)!=Ok)return false;
 WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_DBLCLKS;wc.lpfnWndProc=windowProc;wc.hInstance=gModule;
 wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=kWindowClass;wc.hbrBackground=nullptr;
 if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){GdiplusShutdown(gGdiToken);gGdiToken=0;return false;}
 gInitialized=true;return true;
}

void globalShutdown(){
 if(!gInitialized)return;
 UnregisterClassW(kWindowClass,gModule);
 if(gGdiToken)GdiplusShutdown(gGdiToken);
 gGdiToken=0;gInitialized=false;
}

bool createWindow(GuiState*ui,HWND parent){
 if(!ui||!parent)return false;if(ui->hwnd)return true;ui->parent=parent;
 ui->hwnd=CreateWindowExW(0,kWindowClass,L"GrowlForge 2.0",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
                          0,0,(int)ui->width,(int)ui->height,parent,nullptr,gModule,ui);
 return ui->hwnd!=nullptr;
}

} // namespace gfui

bool growlForgeGuiGlobalInit(){return gfui::globalInit();}
void growlForgeGuiGlobalShutdown(){gfui::globalShutdown();}

void destroyGrowlForgeGui(GrowlForge*s){
 if(!s||!s->guiState)return;auto*ui=static_cast<gfui::GuiState*>(s->guiState);
 if(ui->hwnd)DestroyWindow(ui->hwnd);else gfui::destroyRenderResources(ui);delete ui;s->guiState=nullptr;
}

bool guiIsApiSupported(const clap_plugin_t*,const char*api,bool floating){return !floating&&api&&!std::strcmp(api,CLAP_WINDOW_API_WIN32);}
bool guiPreferred(const clap_plugin_t*,const char**api,bool*floating){if(!api||!floating)return false;*api=CLAP_WINDOW_API_WIN32;*floating=false;return true;}
bool guiCreate(const clap_plugin_t*p,const char*api,bool floating){
 auto*s=self(p);if(s->guiState||!growlForgeGuiGlobalInit()||!guiIsApiSupported(p,api,floating))return false;
 auto*ui=new gfui::GuiState();ui->owner=s;s->guiState=ui;return true;
}
void guiDestroy(const clap_plugin_t*p){destroyGrowlForgeGui(self(p));}
bool guiSetScale(const clap_plugin_t*p,double scale){
 auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);
 ui->scale=clamp(scale,0.75,2.0);ui->width=(uint32_t)std::lround(gfui::kBaseWidth*ui->scale);ui->height=(uint32_t)std::lround(gfui::kBaseHeight*ui->scale);
 ui->staticDirty=true;ui->knobBodies.clear();gfui::resetSnapshots(ui);return true;
}
bool guiGetSize(const clap_plugin_t*p,uint32_t*w,uint32_t*h){if(!w||!h)return false;auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);*w=ui->width;*h=ui->height;return true;}
bool guiCanResize(const clap_plugin_t*){return true;}
bool guiResizeHints(const clap_plugin_t*,clap_gui_resize_hints_t*h){if(!h)return false;h->can_resize_horizontally=true;h->can_resize_vertically=true;h->preserve_aspect_ratio=true;h->aspect_ratio_width=5;h->aspect_ratio_height=3;return true;}
bool guiAdjustSize(const clap_plugin_t*,uint32_t*w,uint32_t*h){
 if(!w||!h)return false;double width=clamp((double)*w,(double)gfui::kMinWidth,(double)gfui::kMaxWidth);double height=width*3.0/5.0;
 if(height>*h){height=clamp((double)*h,(double)gfui::kMinHeight,(double)gfui::kMaxHeight);width=height*5.0/3.0;}
 width=clamp(width,(double)gfui::kMinWidth,(double)gfui::kMaxWidth);height=width*3.0/5.0;*w=(uint32_t)std::lround(width);*h=(uint32_t)std::lround(height);return true;
}
bool guiSetSize(const clap_plugin_t*p,uint32_t w,uint32_t h){
 auto*s=self(p);if(!s->guiState)return false;guiAdjustSize(p,&w,&h);auto*ui=static_cast<gfui::GuiState*>(s->guiState);ui->width=w;ui->height=h;
 ui->staticDirty=true;ui->knobBodies.clear();gfui::resetSnapshots(ui);
 if(ui->hwnd)SetWindowPos(ui->hwnd,nullptr,0,0,(int)w,(int)h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);return true;
}
bool guiSetParent(const clap_plugin_t*p,const clap_window_t*w){
 auto*s=self(p);if(!s->guiState||!w||!w->api||std::strcmp(w->api,CLAP_WINDOW_API_WIN32)||!w->win32)return false;
 return gfui::createWindow(static_cast<gfui::GuiState*>(s->guiState),static_cast<HWND>(w->win32));
}
bool guiSetTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void guiSuggestTitle(const clap_plugin_t*,const char*){}
bool guiShow(const clap_plugin_t*p){
 auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);if(!ui->hwnd)return false;
 ui->shown=true;gfui::resetSnapshots(ui);SetTimer(ui->hwnd,gfui::kAnimationTimer,gfui::kAnimationIntervalMs,nullptr);
 ShowWindow(ui->hwnd,SW_SHOW);gfui::invalidateFull(ui);UpdateWindow(ui->hwnd);return true;
}
bool guiHide(const clap_plugin_t*p){
 auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);if(!ui->hwnd)return false;
 KillTimer(ui->hwnd,gfui::kAnimationTimer);ShowWindow(ui->hwnd,SW_HIDE);ui->shown=false;return true;
}

const clap_plugin_gui_t guiExt{guiIsApiSupported,guiPreferred,guiCreate,guiDestroy,guiSetScale,guiGetSize,guiCanResize,guiResizeHints,guiAdjustSize,guiSetSize,guiSetParent,guiSetTransient,guiSuggestTitle,guiShow,guiHide};

#else

bool growlForgeGuiGlobalInit(){return true;}
void growlForgeGuiGlobalShutdown(){}
void destroyGrowlForgeGui(GrowlForge*s){if(s)s->guiState=nullptr;}
bool guiIsApiSupported(const clap_plugin_t*,const char*,bool){return false;}
bool guiPreferred(const clap_plugin_t*,const char**,bool*){return false;}
bool guiCreate(const clap_plugin_t*,const char*,bool){return false;}
void guiDestroy(const clap_plugin_t*){}
bool guiSetScale(const clap_plugin_t*,double){return false;}
bool guiGetSize(const clap_plugin_t*,uint32_t*,uint32_t*){return false;}
bool guiCanResize(const clap_plugin_t*){return false;}
bool guiResizeHints(const clap_plugin_t*,clap_gui_resize_hints_t*){return false;}
bool guiAdjustSize(const clap_plugin_t*,uint32_t*,uint32_t*){return false;}
bool guiSetSize(const clap_plugin_t*,uint32_t,uint32_t){return false;}
bool guiSetParent(const clap_plugin_t*,const clap_window_t*){return false;}
bool guiSetTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void guiSuggestTitle(const clap_plugin_t*,const char*){}
bool guiShow(const clap_plugin_t*){return false;}
bool guiHide(const clap_plugin_t*){return false;}
const clap_plugin_gui_t guiExt{guiIsApiSupported,guiPreferred,guiCreate,guiDestroy,guiSetScale,guiGetSize,guiCanResize,guiResizeHints,guiAdjustSize,guiSetSize,guiSetParent,guiSetTransient,guiSuggestTitle,guiShow,guiHide};

#endif

} // namespace growlforge
