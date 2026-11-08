#pragma once
#include <intrin.h>
#include "../viewport-helper.h"
#include "bomb_projection.h"
#include "bomb_circle.h"
#include "bomb_line_anchor.h"
namespace bomb_render {
using Object=UEVR_UObjectHandle;
struct Vec{double x,y,z;};struct XY{double x,y;};
using Project=bool(*)(Object,const Vec*,XY*,bool);
using Map=void(*)(Object,const XY*,XY*,XY*);
inline Project projectOriginal=nullptr;inline Map mapOriginal=nullptr;
inline uintptr_t gameBase=0;
inline bool landingTrim(uintptr_t caller,XY& v){
 if(!bomb_projection::scope.active||!bomb_projection::calibrationEnabled.load()||!bomb_line_anchor::landingCaller(caller)||!std::isfinite(v.x+v.y))return false;
 const auto offset=bomb_projection::verticalOffset.load();if(offset==0)return false;v.y+=offset;return true;
}
__declspec(noinline) inline bool project(Object o,const Vec* p,XY* out,bool relative){
 const bool result=projectOriginal?projectOriginal(o,p,out,relative):false;
 if(result&&bomb_projection::scope.active){XY v{};if(bomb_projection::read(out,v)&&bomb_projection::scalePixel(v.x,v.y))std::memcpy(out,&v,sizeof(v));}
 return result;
}
inline void map(Object o,const XY* p,XY* intermediate,XY* out){
 if(mapOriginal)mapOriginal(o,p,intermediate,out);
 if(!bomb_projection::scope.active)return;
 const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto caller=address>=gameBase?address-gameBase:address;
 if(caller!=ac8_game_compatibility::rva<bomb_line_anchor::originCaller>()&&!bomb_line_anchor::landingCaller(caller))return;
 XY v{};if(!bomb_projection::read(out,v)||!std::isfinite(v.x+v.y))return;
 bool changed=caller==ac8_game_compatibility::rva<bomb_line_anchor::originCaller>()?bomb_line_anchor::apply(caller,v.x,v.y,GetTickCount64()):bomb_line_anchor::applyHorizontal(caller,v.x);
 if(landingTrim(caller,v))changed=true;
 if(caller==ac8_game_compatibility::rva<bomb_line_anchor::originCaller>()&&changed)weapon_surface::pin({v.x,v.y});
 if(changed)std::memcpy(out,&v,sizeof(v));
}
#include "bomb_render_guards.h"
inline bool initialize(const UEVR_PluginInitializeParam* p){
 if(!p||!bomb_projection::installed||!bomb_circle::installed)return false;
 gameBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 for(const auto& g:guards){std::array<unsigned char,32> bytes{};if(!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(gameBase+ac8_game_compatibility::translate(g.rva)),bytes,g.head)||bytes!=g.head)return false;}
 const std::array<void*,2> replacements{reinterpret_cast<void*>(&project),reinterpret_cast<void*>(&map)};
 const std::array<void**,2> originals{reinterpret_cast<void**>(&projectOriginal),reinterpret_cast<void**>(&mapOriginal)};
 std::array<int,2> ids{};ids.fill(-1);unsigned attached=0;
 for(unsigned i=0;i<ids.size();++i){ids[i]=p->functions->register_inline_hook(reinterpret_cast<void*>(gameBase+ac8_game_compatibility::translate(guards[i].rva)),replacements[i],originals[i]);if(ids[i]<0||!*originals[i])break;++attached;}
 if(attached!=ids.size()||!p->callbacks->on_custom_event(bomb_projection::calibrationControl)||!p->callbacks->on_custom_event(bomb_line_anchor::control)){
  for(auto id:ids)if(id>=0)p->functions->unregister_inline_hook(id);return false;
 }
 // Only publish after guarded registration and all initialization succeeded.
 ac8_viewport_helper::publish(reinterpret_cast<void*>(gameBase+ac8_game_compatibility::translate(guards[1].rva)),reinterpret_cast<void*>(mapOriginal));
 return true;
}
}
