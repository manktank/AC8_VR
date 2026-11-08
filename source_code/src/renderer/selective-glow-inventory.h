#pragma once
// TEMP Plan C: explicit one-shot CPU inventory. No draw/render/visibility writes,
// GPU capture, timer, hook or cross-Lua coordination. Remove after research.
#include "../backend/backend_boundary.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
namespace selective_glow_inventory {
using Object=UEVR_UObjectHandle;
template<class T>inline bool read(const void* p,T& v){SIZE_T n=0;return p&&ReadProcessMemory(GetCurrentProcess(),p,&v,sizeof(v),&n)&&n==sizeof(v);}
inline bool live(const UEVR_PluginInitializeParam* p,Object o){return o&&p->sdk->uobject_hook->exists(o);}
inline std::string quote(const std::string& in){std::ostringstream s;s<<'"';for(unsigned char c:in){if(c=='"'||c=='\\')s<<'\\'<<c;else if(c<32)s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else s<<c;}s<<'"';return s.str();}
inline std::string text(const std::wstring& w){if(w.empty())return{};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);if(n<=0)return{};std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr);return s;}
inline std::string identity(const UEVR_PluginInitializeParam* p,Object o){
 std::wstring path;std::array<Object,32> seen{};
 for(unsigned i=0;o&&i<seen.size();++i){if(!live(p,o))return{};for(unsigned j=0;j<i;++j)if(seen[j]==o)return{};seen[i]=o;
  wchar_t name[1024]{};p->sdk->fname->to_string(p->sdk->uobject->get_fname(o),name,1024);if(!name[0]||name[1023])return{};
  path=path.empty()?name:std::wstring(name)+L"."+path;o=p->sdk->uobject->get_outer(o);
 }
 return o?std::string{}:text(path);
}
inline Object property(const UEVR_PluginInitializeParam* p,Object o,const wchar_t* key){Object v=nullptr;if(!live(p,o)||!read(p->sdk->uobject->get_property_data(o,key),v)||!live(p,v))return nullptr;return v;}
struct Array {Object* data=nullptr;int32_t count=0,capacity=0;};
inline bool array(const void* address,Array& a){return read(address,a)&&a.count>=0&&a.count<=64&&a.capacity>=a.count&&a.capacity<=65536&&(!a.count||a.data);}
inline bool available(const UEVR_PluginInitializeParam* p){return p&&p->sdk&&p->sdk->uobject&&p->sdk->uobject->get_outer&&p->sdk->uobject->get_fname&&p->sdk->uobject->get_property_data&&p->sdk->uobject->is_a&&p->sdk->uobject_hook&&p->sdk->uobject_hook->exists&&p->sdk->uobject_hook->get_objects_by_class_name&&p->sdk->uobject_array&&p->sdk->uobject_array->find_uobject&&p->sdk->functions&&p->sdk->functions->get_player_controller&&p->sdk->fname&&p->sdk->fname->to_string;}
using Observe=void(*)(const UEVR_PluginInitializeParam*,Object,const char*,const std::filesystem::path&);
inline bool capture(const UEVR_PluginInitializeParam* p,const char* id,const std::filesystem::path& assets,Observe observe=nullptr){
 if(!available(p)||!id)return false;std::string suffix=id;if(suffix.empty()||suffix.size()>48||suffix.find_first_not_of("0123456789-")!=std::string::npos)return false;
 // This optional raw-layout diagnostic shares the native widget/Slate
 // contract. A changed unrelated file byte must not block reflected discovery.
 const bool guarded=ac8_game_compatibility::allowed("glow");
 std::ostringstream s;s<<"{\"version\":1,\"readOnly\":true,\"gpuContentsVerified\":false,\"sameFrameGpuReadinessVerified\":false,\"executableGuard\":"<<(guarded?"true":"false");
 auto pc=p->sdk->functions->get_player_controller(0);auto hud=property(p,pc,L"MyHUD");auto root=property(p,hud,L"HudWidget");auto owner=root?p->sdk->uobject->get_outer(root):nullptr;
 s<<",\"hud\":"<<quote(identity(p,hud))<<",\"root\":"<<quote(identity(p,root))<<",\"owner\":"<<quote(identity(p,owner))<<",\"roots\":[";
 bool first=true;for(const auto key:{L"HudWidget",L"GlowWidgetPtr",L"NonGlowFrontWidgetPtr",L"NonGlowBackWidgetPtr",L"HUDMessageWidget",L"SubtitleWidget",L"NonGlowWidget"}){
  if(!first)s<<',';first=false;s<<"{\"property\":"<<quote(text(key))<<",\"widget\":"<<quote(identity(p,property(p,hud,key)))<<'}';
 }
 s<<']';Object converter=nullptr;int matches=0,found=0;
 if(guarded&&live(p,owner)){
  std::array<Object,32> objects{};found=p->sdk->uobject_hook->get_objects_by_class_name(L"Class /Script/Live.WidgetToTextureSystem",objects.data(),static_cast<unsigned>(objects.size()),false);
  auto cls=reinterpret_cast<UEVR_UClassHandle>(p->sdk->uobject_array->find_uobject(L"Class /Script/Live.WidgetToTextureSystem"));
  if(found>=0&&found<=static_cast<int>(objects.size())&&cls)for(int i=0;i<found;++i){auto o=objects[i];if(live(p,o)&&p->sdk->uobject->is_a(o,cls)&&p->sdk->uobject->get_outer(o)==owner){converter=o;++matches;}}
 }
 s<<",\"converterCandidates\":"<<found<<",\"currentOwnerMatches\":"<<matches<<",\"converter\":"<<quote(matches==1?identity(p,converter):std::string{})<<",\"buckets\":[";
 if(matches==1){
  if(observe)observe(p,converter,id,assets);
  constexpr uintptr_t offsets[]{0x78,0xc8,0x118};const char* names[]{"Glow","NoGlowFront","NoGlowBack"};
  for(unsigned i=0;i<3;++i){if(i)s<<',';auto bytes=reinterpret_cast<const unsigned char*>(converter)+offsets[i];Array retained{},renderers{},widgets{},windows{};
   const bool valid=array(bytes,renderers)&&array(bytes+0x10,retained)&&array(bytes+0x20,widgets)&&array(bytes+0x30,windows);
   Object target=nullptr;read(bytes+0x40,target);s<<"{\"name\":"<<quote(names[i])<<",\"layoutValid\":"<<(valid?"true":"false")<<",\"target\":"<<quote(live(p,target)?identity(p,target):std::string{})<<",\"retained\":"<<retained.count<<",\"renderers\":"<<renderers.count<<",\"slateWidgets\":"<<widgets.count<<",\"windows\":"<<windows.count;
   int32_t width=0,height=0;if(live(p,target)){read(p->sdk->uobject->get_property_data(target,L"SizeX"),width);read(p->sdk->uobject->get_property_data(target,L"SizeY"),height);}
   s<<",\"size\":["<<width<<','<<height<<"],\"members\":[";
   if(valid)for(int n=0;n<retained.count;++n){Object o=nullptr;if(n)s<<',';read(retained.data+n,o);s<<quote(live(p,o)?identity(p,o):std::string{});}
   s<<"]}";
  }
 }
 s<<"],\"limit\":\"Registry membership and allocated targets do not prove rendered pixels, current GPU production or matching coordinates.\"}";
 std::error_code ec;const auto directory=assets.parent_path().parent_path()/L"data";std::filesystem::create_directories(directory,ec);if(ec)return false;
 const auto file=directory/("AC8_VR-SelectiveGlowInventory-"+suffix+".json");if(std::filesystem::exists(file))return false;
 std::ofstream out(file,std::ios::binary);out<<s.str()<<'\n';out.close();return bool(out);
}
}
