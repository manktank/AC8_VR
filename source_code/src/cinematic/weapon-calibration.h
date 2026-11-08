#pragma once
#include <intrin.h>
#include <cstdio>
// Visual guidance calibration only. No weapon-component values are written.
namespace weapon_calibration {
using Object=UEVR_UObjectHandle;
struct Vec {double x,y,z;};
using Guidance=void(*)(Object,const Vec*,const Vec*,const Vec*,const Vec*);
// Stack slots 6 in Circle and 4/5 in Tick are unused by the verified native
// bodies. Preserve their raw bits; circle radius is float, colour a pointer.
using Circle=void(*)(Object,const Vec*,const Vec*,const Vec*,float,uint64_t,const void*,unsigned char);
using Tick=void(*)(Object,const Vec*,const Vec*,uint64_t,uint64_t,const void*);
inline const UEVR_PluginInitializeParam* host=nullptr;
inline Guidance originalGuidance=nullptr;inline Circle originalCircle=nullptr;inline Tick originalTick=nullptr;
inline uintptr_t base=0;
inline std::atomic<double> saamScale{1};
inline std::atomic<bool> flight{false};
struct Context {Object canvas=nullptr;double scale=1;Vec centre{};bool ring=false;};
inline thread_local Context* context=nullptr;
inline bool finite(Vec v){return std::isfinite(v.x+v.y+v.z)&&std::abs(v.x)<1e12&&std::abs(v.y)<1e12&&std::abs(v.z)<1e12;}
inline bool live(Object o){return o&&host&&host->sdk&&host->sdk->uobject_hook&&host->sdk->uobject_hook->exists&&host->sdk->uobject_hook->exists(o);}
inline Object reference(Object o,const wchar_t* name){Object value=nullptr;if(live(o))bomb_projection::read(host->sdk->uobject->get_property_data(o,name),value);return live(value)?value:nullptr;}
inline Object canvasFor(Object component){
 Object hud=nullptr;if(!live(component)||!bomb_projection::read(reinterpret_cast<const unsigned char*>(component)+0x988,hud)||!live(hud))return nullptr;
 auto pc=host->sdk->functions->get_player_controller(0);if(reference(pc,L"MyHUD")!=hud)return nullptr;
 return reference(hud,L"DynamicCanvas");
}
inline void guidance(Object component,const Vec* a,const Vec* b,const Vec* c,const Vec* d){
 const double factor=saamScale.load();Context local;
 const auto surface=weapon_surface::snapshot();
 if(factor!=1||surface.changed){local.canvas=canvasFor(component);local.scale=factor;}
 Object renderer=nullptr;if(local.canvas)bomb_projection::read(reinterpret_cast<const unsigned char*>(local.canvas)+0x190,renderer);
 weapon_surface::Scope presentation(local.canvas,renderer,surface);
 const auto prior=context;context=local.canvas?&local:nullptr;
 struct Restore {Context* previous;~Restore(){context=previous;}}restore{prior};
 if(originalGuidance)originalGuidance(component,a,b,c,d);
}
inline void circleAt(uintptr_t site,Object canvas,const Vec* centre,const Vec* u,const Vec* v,float radius,uint64_t unused,const void* color,unsigned char dashed){
 if(!originalCircle)return;Vec p{},x{},y{};
 if(site==base+ac8_game_compatibility::rva<0x729347c>()&&context&&context->scale!=1&&context->canvas==canvas&&std::isfinite(radius)&&radius>0&&
    bomb_projection::read(centre,p)&&bomb_projection::read(u,x)&&bomb_projection::read(v,y)&&finite(p)&&finite(x)&&finite(y)){
  context->centre=p;context->ring=true;const auto s=context->scale;
  x={x.x*s,x.y*s,x.z*s};y={y.x*s,y.y*s,y.z*s};
  originalCircle(canvas,centre,&x,&y,radius,unused,color,dashed);return;
 }
 originalCircle(canvas,centre,u,v,radius,unused,color,dashed);
}
__declspec(noinline) inline void circle(Object canvas,const Vec* centre,const Vec* u,const Vec* v,float radius,uint64_t unused,const void* color,unsigned char dashed){
 circleAt(reinterpret_cast<uintptr_t>(_ReturnAddress()),canvas,centre,u,v,radius,unused,color,dashed);
}
inline bool tickSite(uintptr_t site){return site==base+ac8_game_compatibility::rva<0x72934d7>()||site==base+ac8_game_compatibility::rva<0x7293564>()||site==base+ac8_game_compatibility::rva<0x72935b5>()||site==base+ac8_game_compatibility::rva<0x7293632>();}
inline void tickAt(uintptr_t site,Object canvas,const Vec* point,const Vec* direction,uint64_t a,uint64_t b,const void* color){
 if(!originalTick)return;Vec p{};
 if(tickSite(site)&&context&&context->scale!=1&&context->ring&&context->canvas==canvas&&bomb_projection::read(point,p)&&finite(p)){
  const auto c=context->centre;const auto s=context->scale;p={c.x+(p.x-c.x)*s,c.y+(p.y-c.y)*s,c.z+(p.z-c.z)*s};
  originalTick(canvas,&p,direction,a,b,color);return;
 }
 originalTick(canvas,point,direction,a,b,color);
}
__declspec(noinline) inline void tick(Object canvas,const Vec* point,const Vec* direction,uint64_t a,uint64_t b,const void* color){tickAt(reinterpret_cast<uintptr_t>(_ReturnAddress()),canvas,point,direction,a,b,color);}
inline void control(const char* event,const char* data){
 if(!event||!data||std::strcmp(event,"AC8_WeaponCalibration"))return;
 double scale=1;int keys=0,inFlight=0;char a=0,b=0;std::istringstream in(data);
 if(!(in>>scale>>a>>keys>>b>>inFlight)||a!='|'||b!='|'||!std::isfinite(scale)||scale<.25||scale>2||(keys!=0&&keys!=1)||(inFlight!=0&&inFlight!=1))return;
 in>>std::ws;if(!in.eof())return;saamScale=scale;flight=inFlight!=0;
}
#ifndef AC8_WEAPON_CALIBRATION_FIXTURE
inline bool initialize(const UEVR_PluginInitializeParam* p){
 host=p;if(!p||!p->sdk||!p->sdk->uobject||!p->sdk->uobject->get_property_data||!p->sdk->uobject_hook||!p->sdk->uobject_hook->exists||!p->sdk->functions||!p->sdk->functions->get_player_controller||!p->functions||!p->functions->dispatch_lua_event||!p->functions->register_inline_hook||!p->functions->unregister_inline_hook||!p->callbacks||!p->callbacks->on_custom_event)return false;
 auto module=GetModuleHandleW(nullptr);
 if(!module||!ac8_game_compatibility::allowed("weapon"))return false;
 struct Guard {uintptr_t rva;std::array<unsigned char,32> head;};
 const std::array<Guard,3> guards{{{0x7293070,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x78,0x20,0x48,0x89,0x50,0x10,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xa0,0x48,0x81,0xec}},
 {0x7245ba0,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x68,0x18,0x48,0x89,0x70,0x20,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x00,0x01,0x00,0x00,0x0f,0x29,0x70,0xd8,0x49}},
 {0x7245850,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x55,0x57,0x41,0x56,0x48,0x8d,0x68,0xb1,0x48,0x81,0xec,0x00,0x01,0x00,0x00,0x0f,0x29,0x70,0xd8,0x4d,0x8b}}}};
 base=reinterpret_cast<uintptr_t>(module);
 for(const auto& guard:guards){std::array<unsigned char,32> bytes{};if(!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(base+ac8_game_compatibility::translate(guard.rva)),bytes,guard.head)||bytes!=guard.head)return false;}
 const std::array<void*,3> replacements{reinterpret_cast<void*>(&guidance),reinterpret_cast<void*>(&circle),reinterpret_cast<void*>(&tick)};
 const std::array<void**,3> originals{reinterpret_cast<void**>(&originalGuidance),reinterpret_cast<void**>(&originalCircle),reinterpret_cast<void**>(&originalTick)};
 std::array<int,3> ids{};ids.fill(-1);unsigned attached=0;
 for(unsigned i=0;i<3;++i){ids[i]=p->functions->register_inline_hook(reinterpret_cast<void*>(base+ac8_game_compatibility::translate(guards[i].rva)),replacements[i],originals[i]);if(ids[i]<0||!*originals[i])break;++attached;}
 if(attached!=3||!p->callbacks->on_custom_event(control)){
  for(auto id:ids)if(id>=0)p->functions->unregister_inline_hook(id);originalGuidance=nullptr;originalCircle=nullptr;originalTick=nullptr;return false;
 }
 return true;
}
#endif
}
