#pragma once
// Scoped locator pixel-to-viewport conversion. No preference writes or Lua timing.
namespace resolution {
using Screen=void(*)(UEVR_UObjectHandle,const Point*,Point*,bool);
using Viewport=void(*)(UEVR_UObjectHandle,const Point*,Point*,Point*);
inline Screen originalScreen=nullptr;
inline Viewport absoluteViewport=nullptr;
inline bool installed=false;
inline bool allySupported=false;
constexpr uintptr_t screenRva=0x3d42820,viewportRva=0x3d42480;
// LiveOffscreenLocatorWidget::UpdateLayout converts its already clamped point
// here. Native projection, front/behind decisions, animation and angle remain
// with the game. No new hook, UMG setter, retained widget or preference change.
constexpr uintptr_t allyScreenSite=0x72efdf9;
// The same native offscreen update emits its labels through two text helpers:
// FString (multiple contacts) and FText (one named contact). Keep each label's
// authored displacement, font and alignment; move only the shared anchor.
constexpr uintptr_t allyUpdateRva=0x725b130;
constexpr uintptr_t allyStringScreenSite=0x7244eb7,allyTextScreenSite=0x72448b2;
using AllyUpdate=bool(*)(UEVR_UObjectHandle,const void*,const void*,unsigned char,float,const void*,const void*,const void*,unsigned char);
inline AllyUpdate originalAllyUpdate=nullptr;
struct AllyContext {UEVR_UObjectHandle hud=nullptr;bool mapped=false;Point shift{};};
inline thread_local AllyContext* currentAlly=nullptr;
struct AllyScope {
 AllyContext value;AllyContext* prior;
 explicit AllyScope(UEVR_UObjectHandle hud):value{hud,false,{}},prior(currentAlly){currentAlly=&value;}
 ~AllyScope(){currentAlly=prior;}
 AllyScope(const AllyScope&)=delete;AllyScope& operator=(const AllyScope&)=delete;
};
inline bool allyUpdate(UEVR_UObjectHandle hud,const void* a2,const void* a3,unsigned char a4,float a5,const void* a6,const void* a7,const void* a8,unsigned char a9){
 AllyScope scope(hud==currentHUD()?hud:nullptr);
 return originalAllyUpdate?originalAllyUpdate(hud,a2,a3,a4,a5,a6,a7,a8,a9):false;
}
inline bool pointOK(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1e12&&std::abs(p.y)<1e12;}
inline double insetOffscreenX(double x, double width) {
    // Reserve the same fraction of the live UI width at each edge. This moves
    // the native anchor toward the center without rescaling the drawn symbol.
    constexpr double margin_fraction = 0.04;
    return x * (1.0 - 2.0 * margin_fraction) + width * margin_fraction;
}
inline bool mapPoint(UEVR_UObjectHandle owner,Point p,Point& local){
 Point absolute{NAN,NAN},pixel{NAN,NAN};local={NAN,NAN};
 if(!originalScreen||!absoluteViewport)return false;
 originalScreen(owner,&p,&absolute,false);if(!pointOK(absolute))return false;
 // Resolve at use as well as initialization: either optional native module may
 // initialize first. The verified trampoline also bypasses bomb-only scopes.
 auto shared=::ac8_viewport_helper::verifiedOriginal(reinterpret_cast<void*>(absoluteViewport));
 const auto map=shared?reinterpret_cast<Viewport>(shared):absoluteViewport;
 map(owner,&absolute,&pixel,&local);return pointOK(local);
}
inline bool extents(UEVR_UObjectHandle hud,Point& eye,Point& ui){
 if(!live(hud)||!host||!host->vr||!host->vr->get_ui_width||!host->vr->get_ui_height)return false;
 if(!read(reinterpret_cast<unsigned char*>(hud)+0xa40,eye)||!pointOK(eye)||eye.x<=0||eye.y<=0)return false;
 ui={static_cast<double>(host->vr->get_ui_width()),static_cast<double>(host->vr->get_ui_height())};
 return pointOK(ui)&&ui.x>0&&ui.y>0;
}
inline bool allyPoint(UEVR_UObjectHandle owner,Point input,Point& output){
 const auto hud=currentHUD();Point eye{},ui{};
 if(!pointOK(input)||!extents(hud,eye,ui)||!live(owner))return false;
 auto objects=host->sdk->uobject;if(!objects->get_outer)return false;
 const auto level=objects->get_outer(hud);
 if(!live(level)||objects->get_outer(level)!=owner)return false;
 output={insetOffscreenX(input.x*ui.x/eye.x,ui.x),input.y*ui.y/eye.y};
 return pointOK(output);
}
inline void prepare(TargetContext& ctx,unsigned char kind){
 if(!installed||kind>1||!host||!host->vr||!host->vr->get_ui_width||!host->vr->get_ui_height)return;
 const auto hud=reference(ctx.widget,L"CachedLiveHUD");if(!hud||hud!=currentHUD())return;
 Point eye{};if(!read(reinterpret_cast<unsigned char*>(hud)+0xa40,eye)||!pointOK(eye)||eye.x<=0||eye.y<=0)return;
 const Point ui{static_cast<double>(host->vr->get_ui_width()),static_cast<double>(host->vr->get_ui_height())};
 if(!pointOK(ui)||ui.x<=0||ui.y<=0)return;
 // A zero trim needs only extent normalization, not a measured affine basis.
 // Keep live owner/extents validation above; never reuse a previous draw's map.
 const Point configured=delta(ctx.bits,kind);
 if(!originalScreen||!absoluteViewport)return;
 if(configured.x==0&&configured.y==0&&!ctx.anchor.hud){
  ctx.resolution={true,kind,eye,ui,Point{}};return; // zero-trim fast path
 }
 Point origin{},ex{},ey{};
 if(!mapPoint(ctx.widget,{},origin)||!mapPoint(ctx.widget,{64,0},ex)||!mapPoint(ctx.widget,{0,64},ey))return;
 const Point x{(ex.x-origin.x)/64,(ex.y-origin.y)/64},y{(ey.x-origin.x)/64,(ey.y-origin.y)/64};
 const double det=x.x*y.y-x.y*y.x;if(!std::isfinite(det)||std::abs(det)<1e-12)return;
 Point logical=configured;
 if(ctx.anchor.hud){
  const Point pixels{ui.x*(ctx.anchor.u-.5),ui.y*(ctx.anchor.v-.5)};
  logical.x+=x.x*pixels.x+y.x*pixels.y;logical.y+=x.y*pixels.x+y.y*pixels.y;
 }
 const Point trim{(logical.x*y.y-logical.y*y.x)/det,(logical.y*x.x-logical.x*x.y)/det};
 if(!pointOK(trim))return;
 ctx.resolution={true,kind,eye,ui,trim};ctx.anchorMapped=ctx.anchor.hud!=nullptr;
}
inline void screenAt(uintptr_t site,UEVR_UObjectHandle owner,const Point* input,Point* output,bool includeWindow){
 const auto fn=originalScreen;if(!fn)return;
 const auto ctx=currentTarget;Point point{};
 if(allySupported&&site==gameBase+ac8_game_compatibility::rva<allyScreenSite>()&&!includeWindow){
  // Reset even on a failed conversion: never reuse a previous contact's anchor.
  if(currentAlly)currentAlly->mapped=false;
  Point mapped{};if(read(input,point)&&allyPoint(owner,point,mapped)){
   if(currentAlly&&currentAlly->hud&&currentAlly->hud==currentHUD()){
    currentAlly->shift={mapped.x-point.x,mapped.y-point.y};currentAlly->mapped=true;
   }
   return fn(owner,&mapped,output,includeWindow);
  }
 }
 if(allySupported&&currentAlly&&currentAlly->mapped&&currentAlly->hud&&currentAlly->hud==currentHUD()&&!includeWindow&&
    (site==gameBase+ac8_game_compatibility::rva<allyStringScreenSite>()||site==gameBase+ac8_game_compatibility::rva<allyTextScreenSite>())&&read(input,point)&&pointOK(point)){
  // These helpers receive the World, just like the arrow helper. Require the
  // current HUD's own World; no unrelated text or cross-HUD adjustment.
  const auto objects=host->sdk->uobject;
  const auto level=objects->get_outer?objects->get_outer(currentAlly->hud):nullptr;
  if(live(level)&&objects->get_outer(level)==owner){
   const Point mapped{point.x+currentAlly->shift.x,point.y+currentAlly->shift.y};
   if(pointOK(mapped))return fn(owner,&mapped,output,includeWindow);
  }
 }
 const bool shape=site==gameBase+ac8_game_compatibility::rva<rva>()+0x202;
 const bool label=site==gameBase+ac8_game_compatibility::rva<bodyRva+0x542>();
 if(ctx&&ctx->resolution.active&&!includeWindow&&(shape||(label&&ctx->resolution.kind==1))&&read(input,point)&&pointOK(point)){
  const auto& m=ctx->resolution;
  // The native anchor is in the per-eye pixel extent; convert it to the actual
  // current UI surface before the game's own Slate conversion. Saved trims are
  // logical viewport displacements, converted back through the measured basis.
  double x=point.x*m.ui.x/m.eye.x;
  // The target shape and its name use the same edge anchor. The forward
  // locator keeps the full viewport, including its authored offsets.
  if(m.kind==1)x=insetOffscreenX(x,m.ui.x);
  point={x+m.pixelTrim.x,point.y*m.ui.y/m.eye.y+m.pixelTrim.y};
  if(pointOK(point))return fn(owner,&point,output,includeWindow);
 }
 return fn(owner,input,output,includeWindow);
}
__declspec(noinline) inline void screen(UEVR_UObjectHandle owner,const Point* input,Point* output,bool includeWindow){
 return screenAt(reinterpret_cast<uintptr_t>(_ReturnAddress()),owner,input,output,includeWindow);
}
inline Point postDelta(Point configured,bool mapped){
 // The old -950 baseline compensated the old mixed-space center. Keep the
 // file unchanged and preserve custom deviation from that legacy default.
 return mapped?Point{configured.x,configured.y+950}:configured;
}
inline void initialize(unsigned char* base){
 constexpr unsigned char sh[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xc0,0x0,0x0,0x0,0x48,0x8b,0x5,0xc6,0xa3,0xee,0x9,0x48,0x33,0xc4,0x48,0x89,0x84};constexpr unsigned char vh[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xf0,0x0,0x0,0x0,0x48,0x8b,0x5,0x66,0xa7,0xee,0x9,0x48,0x33,0xc4,0x48,0x89,0x84};
 if(!codeMatches(base+ac8_game_compatibility::rva<screenRva>(),sh))return;
 if(!::ac8_viewport_helper::verifiedOriginal(base+ac8_game_compatibility::rva<viewportRva>())&&!codeMatches(base+ac8_game_compatibility::rva<viewportRva>(),vh))return;
 constexpr unsigned char allyCall[]={0xe8,0x27,0x2a,0xa5,0xfc};
 allySupported=codeMatches(base+ac8_game_compatibility::rva<allyScreenSite-5>(),allyCall);
 absoluteViewport=reinterpret_cast<Viewport>(base+ac8_game_compatibility::rva<viewportRva>());
 const int hook=host->functions->register_inline_hook(base+ac8_game_compatibility::rva<screenRva>(),reinterpret_cast<void*>(&screen),reinterpret_cast<void**>(&originalScreen));
 installed=hook>=0&&originalScreen;
 if(installed)::ac8_viewport_helper::publishScreen(base+ac8_game_compatibility::rva<screenRva>(),reinterpret_cast<void*>(originalScreen));
 if(!installed){if(hook>=0)host->functions->unregister_inline_hook(hook);originalScreen=nullptr;absoluteViewport=nullptr;allySupported=false;}
 // Pair arrow admission with the exact native update and both label branches.
 // One stack scope per original update, no retained widget, tick or deadline.
 constexpr unsigned char updateHead[]={0x44,0x88,0x4c,0x24,0x20,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x48,0x8d,0xac,0x24,0x68,0xff,0xff,0xff,0x48,0x81,0xec,0x98,0x01,0,0};
 constexpr unsigned char stringCall[]={0xe8,0x69,0xd9,0xaf,0xfc};
 constexpr unsigned char textCall[]={0xe8,0x6e,0xdf,0xaf,0xfc};
 allySupported=installed&&allySupported&&codeMatches(base+ac8_game_compatibility::rva<allyUpdateRva>(),updateHead)&&
  codeMatches(base+ac8_game_compatibility::rva<allyStringScreenSite-5>(),stringCall)&&codeMatches(base+ac8_game_compatibility::rva<allyTextScreenSite-5>(),textCall);
 if(allySupported){
  const int id=host->functions->register_inline_hook(base+ac8_game_compatibility::rva<allyUpdateRva>(),reinterpret_cast<void*>(&allyUpdate),reinterpret_cast<void**>(&originalAllyUpdate));
  allySupported=id>=0&&originalAllyUpdate;
  if(!allySupported){if(id>=0)host->functions->unregister_inline_hook(id);originalAllyUpdate=nullptr;}
 }
}
} // namespace resolution
