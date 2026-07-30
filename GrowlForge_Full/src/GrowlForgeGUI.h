#pragma once

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
 p.AddArc(r.X,r.Y,d,d,180,90);
 p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
 p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);
 p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);
 p.CloseFigure();
}

void fillRounded(Graphics&g,const RectF&r,float radius,const Color&fill,const Color&stroke){
 GraphicsPath path;makeRoundedRect(path,r,radius);
 SolidBrush b(fill);g.FillPath(&b,&path);
 Pen p(stroke,1.0f);g.DrawPath(&p,&path);
}

void drawText(Graphics&g,const wchar_t*text,const RectF&r,float size,const Color&c,
              FontStyle style=FontStyleRegular,StringAlignment align=StringAlignmentCenter,
              StringAlignment lineAlign=StringAlignmentCenter){
 FontFamily family(L"Segoe UI");
 Font font(&family,size,style,UnitPixel);
 SolidBrush b(c);
 StringFormat f;f.SetAlignment(align);f.SetLineAlignment(lineAlign);f.SetTrimming(StringTrimmingEllipsisCharacter);
 g.DrawString(text,-1,&font,r,&f,&b);
}

void drawSection(Graphics&g,const RectF&r,const wchar_t*title,bool orange){
 fillRounded(g,r,8.0f,colorPanel(),colorLine());
 const Color accent=orange?colorOrange():colorTeal();
 RectF tr(r.X+18,r.Y+9,r.Width-36,28);
 drawText(g,title,tr,15.0f,accent,FontStyleBold);
 Pen line(accent,1.0f);
 g.DrawLine(&line,r.X+18,r.Y+24,r.X+75,r.Y+24);
 g.DrawLine(&line,r.GetRight()-75,r.Y+24,r.GetRight()-18,r.Y+24);
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
 const double value=(id<kParamCount)?s->p[id].load():0.0;
 if(id==AutoGain||id==X2){std::swprintf(out,n,L"%ls",value>=0.5?L"ON":L"OFF");return;}
 if(id==Input||id==Output||id==Ceiling||id==AutoGainCorrection){std::swprintf(out,n,L"%.1f dB",value);return;}
 if(id==ParallelDry){std::swprintf(out,n,L"%.1f %%",value);return;}
 std::swprintf(out,n,L"%.1f",value);
}

void drawKnob(Graphics&g,GuiState*ui,const KnobDef&k){
 const bool hovered=(ui->hover==k.id||ui->drag==k.id);
 const float n=normalizeParam(k.id,ui->owner->p[k.id].load());
 const Color accent=k.orange?colorOrange():colorTeal();
 const Color dim=k.orange?colorOrangeDim():colorTealDim();
 const float r=k.radius;
 const float start=135.0f,sweep=270.0f;

 Pen baseArc(Color(255,48,53,57),3.0f);
 Pen valueArc(accent,hovered?3.4f:2.7f);
 g.DrawArc(&baseArc,k.x-r-6,k.y-r-6,2*(r+6),2*(r+6),start,sweep);
 if(n>0.001f)g.DrawArc(&valueArc,k.x-r-6,k.y-r-6,2*(r+6),2*(r+6),start,sweep*n);

 const int tickCount=21;
 for(int i=0;i<tickCount;++i){
  const float t=(float)i/(tickCount-1);
  const float angle=(start+sweep*t)*(float)kPi/180.0f;
  const float ro=r+11.0f,ri=r+7.0f;
  Pen tick(t<=n+0.001f?accent:dim,t<=n+0.001f?1.4f:0.8f);
  g.DrawLine(&tick,k.x+std::cos(angle)*ri,k.y+std::sin(angle)*ri,
                  k.x+std::cos(angle)*ro,k.y+std::sin(angle)*ro);
 }

 RectF outer(k.x-r,k.y-r,2*r,2*r);
 GraphicsPath knobPath;knobPath.AddEllipse(outer);PathGradientBrush pg(&knobPath);
 Color center(255,50,53,56),surround(255,16,18,20);
 pg.SetCenterColor(center);INT count=1;pg.SetSurroundColors(&surround,&count);
 g.FillEllipse(&pg,outer);
 Pen edge(hovered?accent:Color(255,88,92,96),hovered?2.0f:1.3f);g.DrawEllipse(&edge,outer);
 RectF inner(k.x-r*0.78f,k.y-r*0.78f,2*r*0.78f,2*r*0.78f);
 LinearGradientBrush innerBrush(inner,Color(255,48,50,52),Color(255,19,21,23),LinearGradientModeVertical);
 g.FillEllipse(&innerBrush,inner);Pen innerEdge(Color(255,12,13,14),1.0f);g.DrawEllipse(&innerEdge,inner);

 const float pointerAngle=(start+sweep*n)*(float)kPi/180.0f;
 Pen pointer(Color(255,244,244,242),std::max(2.0f,r*0.055f));pointer.SetStartCap(LineCapRound);pointer.SetEndCap(LineCapRound);
 g.DrawLine(&pointer,k.x,k.y,k.x+std::cos(pointerAngle)*r*0.61f,k.y+std::sin(pointerAngle)*r*0.61f);

 RectF lr(k.x-r*1.7f,k.y+r+8.0f,r*3.4f,25.0f);
 drawText(g,k.label,lr,k.id==Drive?16.0f:13.5f,k.id==Drive?accent:colorText(),k.id==Drive?FontStyleBold:FontStyleRegular);

 if(k.activity!=kNoActivity){
  const float v=(float)clamp(ui->owner->p[k.activity].load()/100.0,0.0,1.0);
  const float w=54.0f,h=5.0f,x=k.x-w*0.5f,y=k.y+r+34.0f;
  const int seg=10;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;
  for(int i=0;i<seg;++i){
   const bool on=(float)(i+1)/seg<=v+0.001f;
   SolidBrush b(on?accent:Color(255,42,47,50));g.FillRectangle(&b,x+i*(sw+gap),y,sw,h);
  }
 }
}

void drawToggle(Graphics&g,const RectF&r,bool on,const wchar_t*label,bool orange,bool hovered){
 const Color accent=orange?colorOrange():colorTeal();
 drawText(g,label,RectF(r.X-12,r.Y-24,r.Width+24,20),13.0f,colorText());
 fillRounded(g,r,7.0f,on?Color(255,35,50,52):Color(255,26,29,32),hovered?accent:colorLine());
 const float knob=r.Height-10.0f;
 const float x=on?r.GetRight()-knob-5.0f:r.X+5.0f;
 SolidBrush glow(on?accent:Color(255,93,98,101));g.FillEllipse(&glow,x,r.Y+5.0f,knob,knob);
 drawText(g,on?L"ON":L"OFF",RectF(r.X,r.GetBottom()+3,r.Width,16),10.5f,on?accent:colorMuted(),FontStyleBold);
}

void drawSaturationMeter(Graphics&g,GuiState*ui){
 const float x=405,y=418,w=390,h=13;
 const int count=40;const float gap=2.0f;const float sw=(w-gap*(count-1))/count;
 const float n=(float)clamp(ui->owner->p[MeterSaturation].load()/100.0,0.0,1.0);
 drawText(g,L"SATURATION ACTIVITY",RectF(x,y-25,w,20),12.0f,colorOrange(),FontStyleBold);
 for(int i=0;i<count;++i){
  const float t=(float)(i+1)/count;
  Color c=Color(255,43,35,27);
  if(t<=n){
   if(t<0.72f)c=Color(255,225,100+(BYTE)(55*t),22);
   else c=Color(255,255,178+(BYTE)(50*(t-0.72f)/0.28f),40);
  }
  SolidBrush b(c);g.FillRectangle(&b,x+i*(sw+gap),y,sw,13);
 }
 Pen border(Color(255,57,59,61),1.0f);g.DrawRectangle(&border,x-3,y-3,w+6,19);
}

float meterNormalized(float peak){
 const float db=20.0f*std::log10(std::max(peak,1.0e-5f));
 return (float)clamp((db+60.0f)/60.0f,0.0,1.12);
}

void drawStereoMeter(Graphics&g,GuiState*ui,float x,float y,float w,bool output){
 drawText(g,output?L"OUTPUT METER":L"INPUT METER",RectF(x,y-25,w,20),12.0f,colorMuted(),FontStyleBold);
 const int seg=48;const float gap=2.0f;const float sw=(w-gap*(seg-1))/seg;const float h=8.0f;
 for(int chn=0;chn<2;++chn){
  const float peak=output?ui->owner->guiOutputPeak[chn].load():ui->owner->guiInputPeak[chn].load();
  const float n=meterNormalized(peak);
  drawText(g,chn==0?L"L":L"R",RectF(x-26,y+chn*17-4,18,16),11.0f,colorText(),FontStyleBold);
  for(int i=0;i<seg;++i){
   const float t=(float)(i+1)/seg;
   Color active=t<0.72f?Color(255,75,215,54):(t<0.9f?Color(255,248,177,33):Color(255,241,61,28));
   SolidBrush b(t<=n?active:Color(255,35,39,41));g.FillRectangle(&b,x+i*(sw+gap),y+chn*17,sw,h);
  }
 }
 const wchar_t* marks[]={L"-60",L"-48",L"-36",L"-24",L"-12",L"-6",L"0"};
 const float positions[]={0,0.2f,0.4f,0.6f,0.8f,0.9f,1.0f};
 for(int i=0;i<7;++i)drawText(g,marks[i],RectF(x+w*positions[i]-18,y+35,36,14),9.5f,colorMuted());
}

void drawUi(Graphics&g,GuiState*ui){
 g.SetSmoothingMode(SmoothingModeAntiAlias);
 g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
 SolidBrush bg(colorBg());g.FillRectangle(&bg,0,0,kDesignWidth,kDesignHeight);

 fillRounded(g,RectF(8,8,1184,72),9,colorPanel2(),colorLine());
 drawText(g,L"GROWLFORGE",RectF(30,19,280,42),31.0f,colorText(),FontStyleBold,StringAlignmentNear);
 drawText(g,L"2.0",RectF(278,26,70,26),17.0f,colorOrange(),FontStyleBold,StringAlignmentNear);

 clap_id active=ui->drag!=CLAP_INVALID_ID?ui->drag:(ui->hover!=CLAP_INVALID_ID?ui->hover:Drive);
 wchar_t value[64]{};formatParam(ui->owner,active,value,64);
 fillRounded(g,RectF(430,15,340,57),7,Color(255,8,10,12),Color(255,56,59,62));
 drawText(g,labelFor(active),RectF(445,19,310,20),12.5f,active==Drive?colorOrange():colorTeal(),FontStyleBold);
 drawText(g,value,RectF(445,35,310,32),24.0f,colorText(),FontStyleBold);

 const bool x2=ui->owner->p[X2].load()>=0.5;
 RectF x2r(1032,22,142,43);fillRounded(g,x2r,7,x2?Color(255,65,38,15):Color(255,24,27,29),
                                        (ui->hover==X2||x2)?colorOrange():colorLine());
 drawText(g,L"×2 COLOR",x2r,16.0f,x2?colorOrange():colorText(),FontStyleBold);

 drawSection(g,RectF(10,90,330,360),L"INPUT & FEEL",false);
 drawSection(g,RectF(348,90,504,360),L"DISTORTION CORE",true);
 drawSection(g,RectF(860,90,330,360),L"MOTION & DYNAMICS",false);
 drawSection(g,RectF(10,458,650,170),L"TONE & TEXTURE",false);
 drawSection(g,RectF(668,458,522,170),L"ROUTING & OUTPUT",false);

 for(const auto&k:kKnobs)drawKnob(g,ui,k);
 drawSaturationMeter(g,ui);

 RectF ag(881,510,82,38);drawToggle(g,ag,ui->owner->p[AutoGain].load()>=0.5,L"Auto-Gain",false,ui->hover==AutoGain);
 wchar_t correction[64]{};std::swprintf(correction,64,L"COMMIT %+.1f dB",ui->owner->p[AutoGainCorrection].load());
 RectF commit(873,572,98,25);fillRounded(g,commit,5,Color(255,25,29,31),ui->hover==ApplyAutoGain?colorTeal():colorLine());
 drawText(g,correction,commit,9.5f,ui->owner->p[AutoGain].load()>=0.5?colorTeal():colorMuted(),FontStyleBold);

 fillRounded(g,RectF(10,636,1180,76),7,colorPanel2(),colorLine());
 drawStereoMeter(g,ui,52,668,420,false);
 drawStereoMeter(g,ui,728,668,420,true);
 drawText(g,L"GROWLFORGE 2.0  •  CLAP",RectF(485,660,230,26),10.5f,Color(255,71,76,80),FontStyleBold);
}

PointF logicalPoint(GuiState*ui,int px,int py){
 RECT r{};GetClientRect(ui->hwnd,&r);
 const float w=(float)std::max(1L,r.right-r.left),h=(float)std::max(1L,r.bottom-r.top);
 return PointF(px*kDesignWidth/w,py*kDesignHeight/h);
}

const KnobDef* knobAt(float x,float y){
 for(const auto&k:kKnobs){const float dx=x-k.x,dy=y-k.y;if(dx*dx+dy*dy<=(k.radius+12)*(k.radius+12))return &k;}
 return nullptr;
}

bool inRect(float x,float y,const RectF&r){return x>=r.X&&x<=r.GetRight()&&y>=r.Y&&y<=r.GetBottom();}

clap_id controlAt(float x,float y){
 if(const auto*k=knobAt(x,y))return k->id;
 if(inRect(x,y,RectF(1032,22,142,43)))return X2;
 if(inRect(x,y,RectF(881,510,82,38)))return AutoGain;
 if(inRect(x,y,RectF(873,572,98,25)))return ApplyAutoGain;
 return CLAP_INVALID_ID;
}

void invalidate(GuiState*ui){if(ui&&ui->hwnd)InvalidateRect(ui->hwnd,nullptr,FALSE);}

void toggleParam(GuiState*ui,clap_id id){
 ui->owner->beginGuiGesture(id);
 ui->owner->setGuiParameter(id,ui->owner->p[id].load()>=0.5?0.0:1.0);
 ui->owner->endGuiGesture(id);
 invalidate(ui);
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
  case WM_TIMER:invalidate(ui);return 0;
  case WM_SIZE:ui->width=LOWORD(lp);ui->height=HIWORD(lp);invalidate(ui);return 0;
  case WM_SETCURSOR:{
   POINT p{};GetCursorPos(&p);ScreenToClient(hwnd,&p);auto l=logicalPoint(ui,p.x,p.y);
   SetCursor(LoadCursorW(nullptr,controlAt(l.X,l.Y)!=CLAP_INVALID_ID?IDC_HAND:IDC_ARROW));return TRUE;
  }
  case WM_MOUSEMOVE:{
   auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));
   if(ui->drag!=CLAP_INVALID_ID){
    const auto&d=defs[ui->drag];const bool fine=(GetKeyState(VK_SHIFT)&0x8000)!=0;
    const double delta=(ui->dragStartY-GET_Y_LPARAM(lp))/(fine?1500.0:180.0);
    const double value=ui->dragStartValue+delta*(d.max-d.min);
    ui->owner->setGuiParameter(ui->drag,value);ui->hover=ui->drag;invalidate(ui);return 0;
   }
   const clap_id h=controlAt(l.X,l.Y);if(h!=ui->hover){ui->hover=h;invalidate(ui);}return 0;
  }
  case WM_MOUSELEAVE:if(ui->drag==CLAP_INVALID_ID){ui->hover=CLAP_INVALID_ID;invalidate(ui);}return 0;
  case WM_LBUTTONDOWN:{
   SetFocus(hwnd);auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));const clap_id id=controlAt(l.X,l.Y);
   ui->hover=id;
   if(id==X2||id==AutoGain){toggleParam(ui,id);return 0;}
   if(id==ApplyAutoGain){ui->owner->setGuiParameter(ApplyAutoGain,1.0);invalidate(ui);return 0;}
   if(id!=CLAP_INVALID_ID){ui->drag=id;ui->dragStartY=GET_Y_LPARAM(lp);ui->dragStartValue=ui->owner->p[id].load();
    ui->owner->beginGuiGesture(id);SetCapture(hwnd);invalidate(ui);return 0;}
   return 0;
  }
  case WM_LBUTTONUP:
   if(ui->drag!=CLAP_INVALID_ID){ui->owner->endGuiGesture(ui->drag);ui->drag=CLAP_INVALID_ID;ReleaseCapture();invalidate(ui);}return 0;
  case WM_LBUTTONDBLCLK:{
   auto l=logicalPoint(ui,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));const clap_id id=controlAt(l.X,l.Y);
   if(id!=CLAP_INVALID_ID&&id!=ApplyAutoGain){ui->owner->beginGuiGesture(id);ui->owner->setGuiParameter(id,defs[id].def);ui->owner->endGuiGesture(id);invalidate(ui);}return 0;
  }
  case WM_MOUSEWHEEL:{
   POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&p);auto l=logicalPoint(ui,p.x,p.y);const clap_id id=controlAt(l.X,l.Y);
   if(id!=CLAP_INVALID_ID&&id!=X2&&id!=AutoGain&&id!=ApplyAutoGain){
    const double step=((GetKeyState(VK_SHIFT)&0x8000)!=0)?0.1:((defs[id].max-defs[id].min)>24.0?1.0:0.2);
    const double value=ui->owner->p[id].load()+(GET_WHEEL_DELTA_WPARAM(wp)>0?step:-step);
    ui->owner->beginGuiGesture(id);ui->owner->setGuiParameter(id,value);ui->owner->endGuiGesture(id);ui->hover=id;invalidate(ui);
   }return 0;
  }
  case WM_PAINT:{
   PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT cr{};GetClientRect(hwnd,&cr);
   const int w=std::max(1L,cr.right-cr.left),h=std::max(1L,cr.bottom-cr.top);
   HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,w,h);HGDIOBJ old=SelectObject(mem,bmp);
   {Graphics g(mem);g.ScaleTransform(w/kDesignWidth,h/kDesignHeight);drawUi(g,ui);}
   BitBlt(dc,0,0,w,h,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(hwnd,&ps);return 0;
  }
  case WM_DESTROY:KillTimer(hwnd,1);ui->hwnd=nullptr;return 0;
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
 if(!ui->hwnd)return false;SetTimer(ui->hwnd,1,33,nullptr);return true;
}

} // namespace gfui

bool growlForgeGuiGlobalInit(){return gfui::globalInit();}
void growlForgeGuiGlobalShutdown(){gfui::globalShutdown();}

void destroyGrowlForgeGui(GrowlForge*s){
 if(!s||!s->guiState)return;auto*ui=static_cast<gfui::GuiState*>(s->guiState);
 if(ui->hwnd)DestroyWindow(ui->hwnd);delete ui;s->guiState=nullptr;
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
 ui->scale=clamp(scale,0.75,2.0);ui->width=(uint32_t)std::lround(gfui::kBaseWidth*ui->scale);ui->height=(uint32_t)std::lround(gfui::kBaseHeight*ui->scale);return true;
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
 if(ui->hwnd)SetWindowPos(ui->hwnd,nullptr,0,0,(int)w,(int)h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);return true;
}
bool guiSetParent(const clap_plugin_t*p,const clap_window_t*w){
 auto*s=self(p);if(!s->guiState||!w||!w->api||std::strcmp(w->api,CLAP_WINDOW_API_WIN32)||!w->win32)return false;
 return gfui::createWindow(static_cast<gfui::GuiState*>(s->guiState),static_cast<HWND>(w->win32));
}
bool guiSetTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void guiSuggestTitle(const clap_plugin_t*,const char*){}
bool guiShow(const clap_plugin_t*p){auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);if(!ui->hwnd)return false;ShowWindow(ui->hwnd,SW_SHOW);UpdateWindow(ui->hwnd);ui->shown=true;return true;}
bool guiHide(const clap_plugin_t*p){auto*s=self(p);if(!s->guiState)return false;auto*ui=static_cast<gfui::GuiState*>(s->guiState);if(!ui->hwnd)return false;ShowWindow(ui->hwnd,SW_HIDE);ui->shown=false;return true;}

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
