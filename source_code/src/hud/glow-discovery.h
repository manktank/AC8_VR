#pragma once
// Glow-only, on-demand image shortlist. No tick, scene work or widget writes.
// Preserve the existing 500ms discovery cadence for new/replaced widgets, but
// keep per-image FName filtering inside native code instead of bridging every
// image and every unrelated UserWidget into Lua on each scan.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <mutex>
#include <map>
#include <unordered_set>
#include <string>
#include <vector>
#include "../../vendor/uevr/plugin-api.h"
#include "glow-routing-mode.h"
namespace glow_discovery {
using Object=UEVR_UObjectHandle;
inline const UEVR_PluginInitializeParam* host=nullptr;
inline UEVR_UClassHandle imageClass=nullptr;
inline std::vector<Object> images;
inline std::mutex mutex;

constexpr int maxImages=32768,maxSources=256;
inline bool valid(Object object){return object&&host->sdk->uobject_hook->exists(object);}
inline std::wstring name(Object object){
 if(!valid(object))return {};
 wchar_t text[1024]{};auto value=host->sdk->uobject->get_fname(object);
 if(!value)return {};host->sdk->fname->to_string(value,text,1024);
 if(text[1023])return {};return text;
}
inline bool relevant(const std::wstring& value){
 return value==L"Portrait"||value==L"MiniMapBackground";
}
inline bool source_kind(const std::wstring& cls,const std::wstring& leaf){
 return cls==L"Image"&&(leaf==L"Portrait"||leaf==L"MiniMapBackground");
}
inline std::wstring path(Object object){
 std::array<Object,32> seen{};std::wstring result;
 for(unsigned depth=0;object&&depth<seen.size();++depth){
  if(!valid(object))return {};
  for(unsigned i=0;i<depth;++i)if(seen[i]==object)return {};
  seen[depth]=object;auto leaf=name(object);if(leaf.empty())return {};
  result=leaf+(result.empty()?L"":L"."+result);
  if(result.size()>8192)return {};
  object=host->sdk->uobject->get_outer(object);
  if(!object)return result;
 }
 return {};
}
inline bool utf8(const std::wstring& input,std::string& output){
 if(input.empty())return false;
 const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr);
 if(count<=0||count>32768)return false;
 output.resize(static_cast<size_t>(count));
 return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),output.data(),count,nullptr,nullptr)==count;
}
inline bool collect(std::string& packet){
 auto sdk=host->sdk;
 if(!valid(reinterpret_cast<Object>(imageClass)))imageClass=reinterpret_cast<UEVR_UClassHandle>(sdk->uobject_array->find_uobject(L"Class /Script/UMG.Image"));
 if(!valid(reinterpret_cast<Object>(imageClass)))return false;
 int sources=0;std::string result;
 const auto cls=imageClass;
 const int count=sdk->uobject_hook->get_objects_by_class(cls,nullptr,0,false);
 if(count<0||count>maxImages)return false;
 images.resize(static_cast<size_t>(count));
 const int actual=count?sdk->uobject_hook->get_objects_by_class(cls,images.data(),static_cast<unsigned>(images.size()),false):0;
 if(actual<0||actual>count)return false;
 for(int i=0;i<actual;++i){
  const auto object=images[static_cast<size_t>(i)];static_cast<void>(0);
  if(!valid(object))continue;
  const auto leaf=name(object);if(leaf.empty())return false;
  if(!relevant(leaf))continue;
  // Radar ownership ABI readiness does not prove native coverage reached its
  // submitted image. Keep the accepted radar policy source until that complete
  // producer/consumer path is verified in game; removing it erased radar masks.
  // The existing Lua policy deliberately accepts exact Image, not subclasses.
  const auto sourceClass=name(reinterpret_cast<Object>(sdk->uobject->get_class(object)));
  if(!source_kind(sourceClass,leaf))continue;
  const auto full=path(object);std::string encoded;
  if(full.empty()||full.find(L'|')!=std::wstring::npos||!utf8(sourceClass+L" "+full,encoded))return false;
  if(++sources>maxSources)return false;
  result+='|';result+=encoded;
 }
 static_cast<void>(0);packet=std::move(result);return true;
}
#include "glow-owned-discovery.h"
inline void event(const char* eventName,const char* data){
 if(!eventName||!data||!*data)return;
 const bool scoped=std::strcmp(eventName,"AC8_GlowOwnedDiscoveryRequest")==0;
 if(!scoped&&std::strcmp(eventName,"AC8_GlowDiscoveryRequest"))return;
 size_t length=0;const char* tail=data;
 for(;*tail&&*tail!='|';++tail){if(*tail<'0'||*tail>'9'||++length>9)return;}
 if(!length||(!scoped&&*tail)||(scoped&&*tail!='|'))return;
 std::string response(data,length);
 try{
  std::string sources;bool ok=false;
  {std::lock_guard<std::mutex> guard(mutex);ok=scoped?owned::collect(tail+1,sources):collect(sources);}
  response+=ok?"|ok":"|fallback";if(ok)response+=sources;
 }catch(...){response+="|fallback";}
 // Dispatch outside the lock: Lua may request other native services in reply.
 host->functions->dispatch_lua_event("AC8_GlowDiscoverySources",response.c_str());
}
inline bool initialize(const UEVR_PluginInitializeParam* api){
 if(!api||!api->functions||!api->functions->dispatch_lua_event||!api->callbacks||!api->callbacks->on_custom_event||!api->sdk)return false;
 auto sdk=api->sdk;
 if(!sdk->uobject_array||!sdk->uobject_array->find_uobject||!sdk->uobject_hook||!sdk->uobject_hook->exists||!sdk->uobject_hook->get_objects_by_class||!sdk->uobject||!sdk->uobject->get_fname||!sdk->uobject->get_outer||!sdk->uobject->get_class||!sdk->fname||!sdk->fname->to_string)return false;
 host=api;return api->callbacks->on_custom_event(event);
}
} // namespace glow_discovery
