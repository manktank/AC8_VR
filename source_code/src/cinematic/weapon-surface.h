#pragma once
#include "weapon-surface-math.h"
#include "../viewport-helper.h"
#include <array>
#include <cstring>
#include <cstdlib>
// Scoped final native line vertices cover bomb conics, trajectory and SAAM
// segments/ticks alike. No polling callback, retained UObject or saved-data edit.
namespace weapon_surface {
using Object=UEVR_UObjectHandle;using Point=weapon_surface_math::Point;
using Screen=void(*)(Object,const Point*,Point*,bool);
using Viewport=void(*)(Object,const Point*,Point*,Point*);
using Segment=void(*)(Object,const Point*,const Point*,int,const void*,float);
inline const UEVR_PluginInitializeParam* host=nullptr;
inline Segment original=nullptr;
inline bool installed=false;
inline uintptr_t base=0;
struct Snapshot {weapon_surface_math::Surface reference{},current{};bool changed=false,valid=false;double width=0,height=0;};
struct CachedPoint {Point input{},output{};bool valid=false;};
struct Context {bool active=false,pinned=false;Object renderer=nullptr;Point pin{};Snapshot surface{};weapon_surface_math::Basis basis{};weapon_surface_math::Prepared prepared{};std::array<CachedPoint,2> cache{};unsigned next=0;};
inline thread_local Context* current=nullptr;
template<class T>inline bool read(const void* p,T& v){SIZE_T n=0;return p&&ReadProcessMemory(GetCurrentProcess(),p,&v,sizeof(v),&n)&&n==sizeof(v);}
inline bool value(const char* key,double& v){
 char text[128]{};host->vr->get_mod_value(key,text,sizeof(text));char* end=nullptr;v=std::strtod(text,&end);
 return end!=text&&*end==0&&std::isfinite(v);
}
inline Snapshot snapshot(double knownWidth=0,double knownHeight=0){
 Snapshot s;if(!installed||!host||!host->vr||!host->vr->get_mod_value)return s;
 double type=0,angle=0;auto& c=s.current;
 if(!value("UI_Size",c.height)||!value("UI_Distance",c.distance)||!value("UI_Cylinder_Angle",angle)||!value("UI_OverlayType",type)||!value("UI_X_Offset",c.x)||!value("UI_Y_Offset",c.y))return s;
 const double w=knownWidth>0?knownWidth:host->vr->get_ui_width(),h=knownHeight>0?knownHeight:host->vr->get_ui_height();if(w<=0||h<=0||(type!=0&&type!=1))return s;
 s.width=w;s.height=h;c.aspect=w/h;c.angle=std::max(1.0,angle*weapon_surface_math::pi/180);c.flat=type==0;
 // Preserve the reference width/magnification for any current aspect, rather
 // than assuming a particular desktop resolution or stretching guidance in Y.
 const double referenceWidth=s.reference.height*s.reference.aspect;
 s.reference.aspect=c.aspect;s.reference.height=referenceWidth/c.aspect;
 s.valid=weapon_surface_math::valid(c);s.changed=s.valid&&!weapon_surface_math::same(s.reference,c);return s;
}
inline bool helpers(Screen& screen,Viewport& viewport){
 void* const sAddr=reinterpret_cast<void*>(base+ac8_game_compatibility::rva<0x3d42820>());
 void* const vAddr=reinterpret_cast<void*>(base+ac8_game_compatibility::rva<0x3d42480>());
 auto s=ac8_viewport_helper::verifiedScreenOriginal(sAddr);
 auto v=ac8_viewport_helper::verifiedOriginal(vAddr);
 // Optional locator hook absent: accept only the executable's verified original
 // entry. No dependency on another Lua/module being present or initializing first.
 constexpr std::array<unsigned char,32> sh{{0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xc0,0,0,0,0x48,0x8b,0x05,0xc6,0xa3,0xee,0x09,0x48,0x33,0xc4,0x48,0x89,0x84}};
 constexpr std::array<unsigned char,32> vh{{0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xf0,0,0,0,0x48,0x8b,0x05,0x66,0xa7,0xee,0x09,0x48,0x33,0xc4,0x48,0x89,0x84}};
#ifdef AC8_NATIVE_RESOLUTION_IDENTITY_FIXTURE
 // Fixture code_proof reads live bytes; keep the per-call proof.
 std::array<unsigned char,32> bytes{};
 if(!s&&ac8_game_compatibility::copy_verified_source(sAddr,bytes,sh)&&bytes==sh)s=sAddr;
 if(!v&&ac8_game_compatibility::copy_verified_source(vAddr,bytes,vh)&&bytes==vh)v=vAddr;
#else
 // Production proof reads immutable pre-hook spans: same result every call.
 static const bool sProof=[&]{std::array<unsigned char,32> b{};return ac8_game_compatibility::copy_verified_source(sAddr,b,sh)&&b==sh;}();
 static const bool vProof=[&]{std::array<unsigned char,32> b{};return ac8_game_compatibility::copy_verified_source(vAddr,b,vh)&&b==vh;}();
 if(!s&&sProof)s=sAddr;
 if(!v&&vProof)v=vAddr;
#endif
 screen=reinterpret_cast<Screen>(s);viewport=reinterpret_cast<Viewport>(v);return screen&&viewport;
}
inline bool viewportPoint(Screen screen,Viewport viewport,Object owner,Point p,Point& out){
 Point absolute{NAN,NAN},pixel{NAN,NAN};out={NAN,NAN};
 screen(owner,&p,&absolute,false);if(!weapon_surface_math::finite(absolute))return false;
 viewport(owner,&absolute,&pixel,&out);return weapon_surface_math::finite(out);
}
struct Scope {
 Context context{};Context* prior=current;
 Scope(Object owner,Object renderer,const Snapshot& s){
  context.renderer=renderer;context.surface=s;current=&context;
  if(!renderer||!s.valid||!s.changed)return;
  Point o{},x{},y{};Screen screen=nullptr;Viewport viewport=nullptr;
  if(!helpers(screen,viewport)||!viewportPoint(screen,viewport,owner,{},o)||!viewportPoint(screen,viewport,owner,{64,0},x)||!viewportPoint(screen,viewport,owner,{0,64},y))return;
  context.basis={o,{(x.x-o.x)/64,(x.y-o.y)/64},{(y.x-o.x)/64,(y.y-o.y)/64},s.width,s.height};
  context.active=weapon_surface_math::prepare(s.reference,s.current,context.basis,context.prepared);
 }
 ~Scope(){current=prior;}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline void pin(Point p){if(current&&weapon_surface_math::finite(p)){current->pin=p;current->pinned=true;}}
inline bool point(Context& c,Point in,Point& out){
 if(c.pinned&&std::abs(in.x-c.pin.x)<1e-5&&std::abs(in.y-c.pin.y)<1e-5){out=in;return true;}
 for(const auto& previous:c.cache)if(previous.valid&&previous.input.x==in.x&&previous.input.y==in.y){out=previous.output;return true;}
 if(!weapon_surface_math::preparedMap(c.prepared,in,out))return false;
 c.cache[c.next]={in,out,true};c.next^=1;return true;
}
// These are the guarded native renderer's own point arguments, not retained
// UObject pointers. SEH protects loads without per-vertex OS memory-read calls.
inline bool arguments(const Point* a,const Point* b,Point& av,Point& bv){
 if(!a||!b)return false;
 __try {av=*a;bv=*b;return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void segment(Object renderer,const Point* a,const Point* b,int divisions,const void* color,float thickness){
 if(!original)return;const auto c=current;Point av{},bv{},x{},y{};
 if(c&&c->active&&c->renderer==renderer&&arguments(a,b,av,bv)&&point(*c,av,x)&&point(*c,bv,y)){
  // Preserve physical stroke thickness near the calibrated HUD centre too.
  const double t=thickness*c->prepared.strokeScale;
  if(std::isfinite(t)&&t>0&&t<1e6){original(renderer,&x,&y,divisions,color,static_cast<float>(t));return;}
 }
 original(renderer,a,b,divisions,color,thickness);
}
inline bool initialize(const UEVR_PluginInitializeParam* p){
 host=p;if(!p||!p->vr||!p->vr->get_mod_value||!p->vr->get_ui_width||!p->vr->get_ui_height||!p->functions||!p->functions->register_inline_hook||!p->functions->unregister_inline_hook)return false;
 base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 constexpr uintptr_t rva=0x727c090;
 constexpr std::array<unsigned char,32> expected{{0x48,0x8b,0xc4,0x53,0x57,0x48,0x81,0xec,0xa8,0x00,0x00,0x00,0x48,0x89,0x68,0x08,0x49,0x8b,0xe8,0x48,0x89,0x70,0x10,0x48,0x8b,0xf2,0x4c,0x89,0x70,0x18,0x41,0xbe}};
 std::array<unsigned char,32> bytes{};if(!base||!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(base+ac8_game_compatibility::translate(rva)),bytes,expected)||bytes!=expected)return false;
 const int id=p->functions->register_inline_hook(reinterpret_cast<void*>(base+ac8_game_compatibility::translate(rva)),reinterpret_cast<void*>(&segment),reinterpret_cast<void**>(&original));
 installed=id>=0&&original;if(!installed&&id>=0)p->functions->unregister_inline_hook(id);return installed;
}
}
