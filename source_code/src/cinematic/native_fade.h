#pragma once
#include "../backend/game-compatibility-runtime.h"
#include <atomic>
#include <array>
#include <cmath>
namespace native_fade {
inline uintptr_t gameBase=0;
inline bool ready=false;
inline const UEVR_PluginInitializeParam* host=nullptr;
inline std::atomic<uintptr_t> widget_vtable{0};
inline std::atomic<uintptr_t> source{0};
inline std::atomic<float> multiplier{1};
template<class T>inline bool read(const void* p,T& value){SIZE_T size=0;return p&&ReadProcessMemory(GetCurrentProcess(),p,&value,sizeof(value),&size)&&size==sizeof(value);}
inline bool opacity(float& result){
 const auto widget=source.load(),expected=widget_vtable.load();if(!ready||!widget||!expected||!host||!host->sdk->uobject_hook->exists(reinterpret_cast<UEVR_UObjectHandle>(widget)))return false;
 uintptr_t table=0,slate=0,child=0;float alpha=0;
 if(!read(reinterpret_cast<void*>(widget),table)||table!=expected||
  !read(reinterpret_cast<void*>(widget+0x310),slate)||!slate||
  !read(reinterpret_cast<void*>(slate+0x288),child)||!child||
  !read(reinterpret_cast<void*>(child+0x1cc),alpha)||!std::isfinite(alpha)||alpha<0||alpha>1)return false;
 result=alpha*multiplier.load();return std::isfinite(result)&&result>=0&&result<=1;
}
inline void control(const char* event,const char* data){
 if(!event||std::strcmp(event,"AC8_NativeFadeSource")||!data)return;
 uintptr_t widget=0;float value=0;char trailing=0;
 if(sscanf_s(data,"%llu|%f%c",&widget,&value,&trailing,1u)!=2||!std::isfinite(value)||value<0||value>1)return;
 if(widget&&ready&&!widget_vtable.load()){
  // The native widget type's table can move independently of its setters.
  // Bind from the engine's named class/default object on this engine event,
  // with live membership and exact class ownership. Two byte-identical native
  // constructors are insufficient evidence for choosing a table by address.
  const auto* sdk=host->sdk;
  const auto klass=sdk->uobject_array->find_uobject(L"Class /Script/Live.LiveFadeWidget");
  const auto cdo=klass&&sdk->uobject_hook->exists(klass)?sdk->uclass->get_class_default_object(reinterpret_cast<UEVR_UClassHandle>(klass)):nullptr;
  uintptr_t table=0;
  if(cdo&&sdk->uobject_hook->exists(cdo)&&sdk->uobject->get_class(cdo)==reinterpret_cast<UEVR_UClassHandle>(klass)&&read(cdo,table)&&ac8_game_compatibility::contains(table,sizeof(uintptr_t)))widget_vtable=table;
 }
 multiplier=value;source=widget;
}
inline void initialize(const UEVR_PluginInitializeParam* p){
 host=p;
 if(!host||!host->sdk||!host->sdk->uobject_array||!host->sdk->uobject_array->find_uobject||!host->sdk->uclass||!host->sdk->uclass->get_class_default_object||!host->sdk->uobject||!host->sdk->uobject->get_class||!host->sdk->uobject_hook||!host->sdk->uobject_hook->exists)return;
 gameBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 // Generated signatures verify the native SetFadeOpacity implementation and
 // the source-to-Slate binding before reading their fixed field offsets.
 const std::array<unsigned char,32> opacityHead={{0x48,0x8b,0x89,0x10,0x3,0x0,0x0,0x48,0x85,0xc9,0x74,0x28,0x48,0x8b,0x89,0x88,0x2,0x0,0x0,0x48,0x85,0xc9,0x74,0x1c,0xf3,0xf,0x10,0x2,0xf,0x2e,0x81,0xcc}};
 const std::array<unsigned char,32> bindingHead={{0x48,0x89,0x5c,0x24,0x8,0x57,0x48,0x83,0xec,0x20,0x80,0x3d,0x37,0xca,0x9d,0x6,0x0,0x48,0x8b,0xda,0x48,0x8b,0xf9,0x74,0xd,0x48,0x85,0xd2,0x74,0x8,0x48,0x8b}};
 std::array<unsigned char,32> a{},b{};
 ready=ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(gameBase+ac8_game_compatibility::rva<0x7a55010>()),a,opacityHead)&&a==opacityHead&&ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(gameBase+ac8_game_compatibility::rva<0x7a54fa0>()),b,bindingHead)&&b==bindingHead;
}
}
