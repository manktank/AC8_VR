#pragma once
// Bootstrap only. Subsequent view changes arrive through the game's delegate.
// No engine/render callback, hook, scan, object write or extra DLL.
#include <windows.h>
#include <string>
#include <cstdlib>
#include <cstring>
namespace plane_view {
inline const UEVR_PluginInitializeParam* host=nullptr;
inline bool checked=false,code_ok=false;
inline bool bytes_match(uintptr_t rva,const unsigned char* expected,size_t count){
 unsigned char actual[64]{};SIZE_T n=0;
 return count<=sizeof(actual)&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+rva,actual,count,&n)&&n==count&&!std::memcmp(actual,expected,count);
}
inline void event(const char* event,const char* data){
 if(!event||!data||std::strcmp(event,"AC8_PlaneViewBootstrap"))return;
 // A serial identifies this exact owner/request. Do not accept arbitrary pointers.
 char* end=nullptr;const auto serial=std::strtoull(data,&end,10);
 if(end==data||*end!='|'||std::strlen(end+1)>2048)return;
 const std::string name=end+1;
 if(name.rfind("LiveCameraViewComponent ",0)!=0)return;
 if(!checked){
  checked=true;
  const unsigned char first[]={0xc6,0x87,0x08,0x01,0,0,0};
  const unsigned char cockpit[]={0xc6,0x87,0x08,0x01,0,0,1};
  const unsigned char third[]={0xc6,0x87,0x08,0x01,0,0,2};
  code_ok=bytes_match(0x70ab480+0x25,first,sizeof(first))&&bytes_match(0x70ab570+0x25,cockpit,sizeof(cockpit))&&bytes_match(0x70ab660+0x25,third,sizeof(third));
 }
 int selected=-1;
 if(code_ok){
  const std::wstring wide(name.begin(),name.end());
  auto o=host->sdk->uobject_array->find_uobject(wide.c_str());
  auto cls=reinterpret_cast<UEVR_UClassHandle>(host->sdk->uobject_array->find_uobject(L"Class /Script/Live.LiveCameraViewComponent"));
  if(o&&cls&&host->sdk->uobject->is_a(o,cls)){
   bool schema=true;
   for(const auto& item:{std::pair<const wchar_t*,int>{L"CachedFirstPersonCamera",0xb0},{L"CachedCockpitCamera",0xc0},{L"CachedThirdPersonCamera",0xd0}}){
    auto p=host->sdk->ustruct->find_property(reinterpret_cast<UEVR_UStructHandle>(cls),item.first);
    schema=schema&&p&&host->sdk->fproperty->get_offset(p)==item.second;
   }
   unsigned char value=255;SIZE_T n=0;
   if(schema&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<unsigned char*>(o)+0x108,&value,1,&n)&&n==1&&value<=5)selected=value;
  }
 }
 const auto reply=std::to_string(serial)+"|"+std::to_string(selected);
 host->functions->dispatch_lua_event("AC8_PlaneViewBootstrapResult",reply.c_str());
}
inline bool initialize(const UEVR_PluginInitializeParam* p){
 host=p;
 return p&&p->sdk&&p->sdk->uobject_array&&p->sdk->uobject&&p->sdk->ustruct&&p->sdk->fproperty&&p->functions&&p->functions->dispatch_lua_event&&p->callbacks&&p->callbacks->on_custom_event&&p->sdk->uobject_array->find_uobject&&p->sdk->uobject->is_a&&p->sdk->ustruct->find_property&&p->sdk->fproperty->get_offset&&p->callbacks->on_custom_event(event);
}
}
