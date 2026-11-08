#pragma once
// Included inside glow_discovery. Read-only portrait ownership traversal.
// Layout offsets come from reflection; unsupported layouts use the old roster.
namespace owned {
struct Array {Object* data;int32_t count,capacity;};
struct Field {int offset=-1;bool present=false,valid=false;};
template<class T> bool read(const void* pointer,T& value){
 SIZE_T count=0;
 return pointer&&ReadProcessMemory(GetCurrentProcess(),pointer,&value,sizeof(value),&count)&&count==sizeof(value);
}
inline std::wstring type(UEVR_FPropertyHandle property){
 if(!property)return {};
 const auto cls=host->sdk->ffield->get_class(reinterpret_cast<UEVR_FFieldHandle>(property));
 if(!cls)return {};
 wchar_t text[128]{};host->sdk->fname->to_string(host->sdk->ffield_class->get_fname(cls),text,128);
 return text;
}
inline bool available(){
 const auto s=host->sdk;
 return s->functions&&s->functions->get_player_controller&&s->ustruct&&s->ustruct->find_property&&s->ustruct->get_properties_size&&
  s->ffield&&s->ffield->get_class&&s->ffield_class&&s->ffield_class->get_fname&&
  s->fproperty&&s->fproperty->get_offset&&s->farrayproperty&&s->farrayproperty->get_inner;
}
struct Walk {
 // Metadata is reused only within this synchronous request, never across owners.
 std::map<std::pair<Object,std::wstring>,Field> fields;
 std::unordered_set<Object> seen,active;
 int sources=0;
 std::string packet;
 Field field(Object object,const wchar_t* key,bool array=false){
  const auto cls=reinterpret_cast<Object>(host->sdk->uobject->get_class(object));
  if(!valid(cls))return {};
  const auto cacheKey=std::make_pair(cls,std::wstring(key));
  const auto found=fields.find(cacheKey);if(found!=fields.end())return found->second;
  Field result{};
  const auto structure=reinterpret_cast<UEVR_UStructHandle>(cls);
  const auto property=host->sdk->ustruct->find_property(structure,key);
  if(property){
   result.present=true;
   const auto kind=type(property);
   const bool matching=array?(kind==L"ArrayProperty"&&type(host->sdk->farrayproperty->get_inner(reinterpret_cast<UEVR_FArrayPropertyHandle>(property)))==L"ObjectProperty"):kind==L"ObjectProperty";
   const int size=host->sdk->ustruct->get_properties_size(structure);
   const int width=array?static_cast<int>(sizeof(Array)):static_cast<int>(sizeof(Object));
   result.offset=host->sdk->fproperty->get_offset(property);
   result.valid=matching&&size>=width&&result.offset>=0&&result.offset<=size-width;
  }
  fields.emplace(cacheKey,result);return result;
 }
 bool object(Object from,const wchar_t* key,Object& out,bool required=false){
  out=nullptr;if(!valid(from))return false;
  const auto f=field(from,key);
  if(!f.present)return !required;
  return f.valid&&read(reinterpret_cast<const unsigned char*>(from)+f.offset,out)&&(!out||valid(out));
 }
 bool visit(Object widget,unsigned depth=0){
  if(!valid(widget)||depth>=32||active.count(widget))return false;
  if(seen.count(widget))return true;
  if(seen.size()>=8192)return false;
  seen.insert(widget);active.insert(widget);
  const auto leaf=name(widget);if(leaf.empty())return false;
  const auto sourceClass=relevant(leaf)?name(reinterpret_cast<Object>(host->sdk->uobject->get_class(widget))):std::wstring{};
  if(source_kind(sourceClass,leaf)){
   const auto full=path(widget);std::string encoded;
   if(full.empty()||full.find(L'|')!=std::wstring::npos||!utf8(sourceClass+L" "+full,encoded)||++sources>maxSources)return false;
   packet+='|';packet+=encoded;
  }
  Object tree=nullptr;
  if(!object(widget,L"WidgetTree",tree))return false;
  if(tree){
   Object root=nullptr;
   if(host->sdk->uobject->get_outer(tree)!=widget||!object(tree,L"RootWidget",root,true)||!root||
      host->sdk->uobject->get_outer(root)!=tree||!visit(root,depth+1))return false;
  }
  const auto slots=field(widget,L"Slots",true);
  if(slots.present){
   Array list{};
   if(!slots.valid||!read(reinterpret_cast<const unsigned char*>(widget)+slots.offset,list)||
      list.count<0||list.count>4096||list.capacity<list.count||list.capacity>65536||(!list.data&&list.count))return false;
   for(int32_t i=0;i<list.count;++i){
    Object slot=nullptr,parent=nullptr,child=nullptr,back=nullptr;
    if(!read(list.data+i,slot)||!object(slot,L"Parent",parent,true)||parent!=widget||
       !object(slot,L"Content",child,true)||!child||!object(child,L"Slot",back,true)||back!=slot||!visit(child,depth+1))return false;
   }
  }
  active.erase(widget);return true;
 }
};
inline bool collect(const std::string& identity,std::string& result){
 if(!available()||identity.empty()||identity.size()>32768)return false;
 const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,identity.data(),static_cast<int>(identity.size()),nullptr,0);
 if(count<=0)return false;
 std::wstring wide(static_cast<size_t>(count),L'\0');
 if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,identity.data(),static_cast<int>(identity.size()),wide.data(),count)!=count)return false;
 Walk walk;Object hud=nullptr;
 if(!walk.object(host->sdk->functions->get_player_controller(0),L"MyHUD",hud,true))return false;
 if(!valid(hud)||name(reinterpret_cast<Object>(host->sdk->uobject->get_class(hud)))+L" "+path(hud)!=wide)return false;
 // Only current message and radar subtrees. Do not traverse flight
 // status, dynamic world markers or unrelated HUD trees for this shortlist.
 Object message=nullptr;
 if(!walk.object(hud,L"HUDMessageWidget",message))return false;
 if(!message){Object root=nullptr;
  if(!walk.object(hud,L"HudWidget",root,true)||!root||!walk.object(root,L"HUDMessageWidget",message))return false;
 }
 if(message&&!walk.visit(message))return false;
 Object root=nullptr,radar=nullptr;
 if(!walk.object(hud,L"HudWidget",root))return false;
 // This remains necessary: native coverage/producer association was not active
 // in the real capture despite successful owner ABI checks. Do not equate those.
 if(root){
  if(!walk.object(root,L"MiniMapLayer",radar))return false;
  if(radar&&!walk.visit(radar))return false;
 }
 ++glow_routing_mode::portrait_requests;glow_routing_mode::portrait_nodes+=walk.seen.size();
 result=std::move(walk.packet);return true;
}
} // namespace owned
