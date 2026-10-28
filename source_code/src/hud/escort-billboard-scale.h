// Existing reflected billboard setters; refueling uses its separately guarded native layout adapter.
#include <map>
#include <set>
#include <cmath>
#include <iomanip>
#include <cstdlib>
namespace escort_draw {
struct Sizes {float x,y;};
struct Saved {std::vector<Sizes> original,last;float factor=1;};
static std::map<std::wstring,Saved> saved;
static bool enabled=false;
static bool refuel_logged=false;
static float guideScale(unsigned kind,float value){return kind==1?.25f:kind==4?1.f:value;}

static std::atomic<float> pin_scale{1};


static std::atomic<bool> presentation_allowed{true};
static unsigned componentKind(const std::wstring& name){
    const auto leaf=name.substr(name.rfind(L'.')+1);
    return leaf==L"EscortBillboardImage"?1u:leaf==L"ClockTargetPinBillboardImage"?2u:leaf==L"AntiClockTargetPinBillboardImage"?3u:leaf==L"TextureBox"?4u:0u;
}
static void control(const char* event,const char* data){
    if(!event||!data)return;
    if(!std::strcmp(event,"AC8_TargetsPresentation")){presentation_allowed=std::strcmp(data,"off")!=0;return;}
    if(!std::strcmp(event,"AC8_WorldPipScale")){
        char* end=nullptr;const auto value=std::strtof(data,&end);
        if(end&&end!=data&&!*end&&std::isfinite(value)&&value>=.1f&&value<=2.f)pin_scale=value;
    }
}


static std::filesystem::path directory;
struct View {
    ArrayView array{};int stride=0,x=0,y=0;
    UEVR_UScriptStructHandle structure=nullptr;
};
// Only reflected layout is retained. Instance arrays, material references,
// signed animated sizes and curve data remain freshly read every engine tick.
struct Layout {int stride=0,x=0,y=0,material=-1;UEVR_UScriptStructHandle structure=nullptr;};
static std::map<UEVR_UStructHandle,Layout> layout_cache;
static std::wstring layout_level;
static void select_layout_level(const std::wstring& level){if(level!=layout_level){layout_cache.clear();layout_level=level;refuel_logged=false;}}
static bool view(UEVR_UObjectHandle o,View& v){
    auto cls=reinterpret_cast<UEVR_UStructHandle>(host->sdk->uobject->get_class(o));
    const auto cached=layout_cache.find(cls);
    if(cached!=layout_cache.end()){
        const auto& schema=cached->second;v.stride=schema.stride;v.x=schema.x;v.y=schema.y;v.structure=schema.structure;
        return copy(host->sdk->uobject->get_property_data(o,L"Elements"),v.array)&&v.array.count>=0&&v.array.count<=16&&v.array.capacity>=v.array.count&&v.array.capacity<=4096&&(!v.array.count||v.array.data);
    }
    auto p=property(cls,L"Elements");if(!p||propertyType(p)!=L"ArrayProperty")return false;
    auto inner=host->sdk->farrayproperty->get_inner(reinterpret_cast<UEVR_FArrayPropertyHandle>(p));
    if(!inner||propertyType(inner)!=L"StructProperty")return false;
    v.structure=host->sdk->fstructproperty->get_struct(reinterpret_cast<UEVR_FStructPropertyHandle>(inner));
    if(!v.structure)return false;
    v.stride=host->sdk->uscriptstruct->get_struct_size(v.structure);
    if(v.stride<8||v.stride>1024)return false;
    for(auto spec:{std::pair<const wchar_t*,int*>(L"BaseSizeX",&v.x),{L"BaseSizeY",&v.y}}){
        auto f=property(reinterpret_cast<UEVR_UStructHandle>(v.structure),spec.first);
        if(!f||propertyType(f)!=L"FloatProperty")return false;
        *spec.second=host->sdk->fproperty->get_offset(f);
        if(*spec.second<0||*spec.second>v.stride-4)return false;
    }
    const bool valid=copy(host->sdk->uobject->get_property_data(o,L"Elements"),v.array)&&v.array.count>=0&&v.array.count<=16&&v.array.capacity>=v.array.count&&v.array.capacity<=4096&&(!v.array.count||v.array.data);
    if(valid){
        Layout schema{v.stride,v.x,v.y,-1,v.structure};
        auto m=property(reinterpret_cast<UEVR_UStructHandle>(v.structure),L"Material");
        if(m&&propertyType(m)==L"ObjectProperty"){
            const int offset=host->sdk->fproperty->get_offset(m);
            if(offset>=0&&offset<=v.stride-static_cast<int>(sizeof(void*)))schema.material=offset;
        }
        if(layout_cache.size()>=128)layout_cache.clear();layout_cache[cls]=schema;
    }
    return valid;
}
static bool sizes(const View& v,std::vector<Sizes>& values){
    values.clear();
    for(int i=0;i<v.array.count;++i){
        const auto base=v.array.data+static_cast<size_t>(i)*static_cast<size_t>(v.stride);
        Sizes s{};if(!copy(base+v.x,s.x)||!copy(base+v.y,s.y)||!std::isfinite(s.x)||!std::isfinite(s.y)||std::abs(s.x)>100||std::abs(s.y)>100)return false;
        values.push_back(s);
    }
    return true;
}
static bool equal(const std::vector<Sizes>& a,const std::vector<Sizes>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(std::abs(a[i].x-b[i].x)>1e-7f||std::abs(a[i].y-b[i].y)>1e-7f)return false;
    return true;
}
static std::vector<Sizes> scaled(std::vector<Sizes> values,float factor=.25f){for(auto& s:values){s.x*=factor;s.y*=factor;}return values;}
static bool set(UEVR_UObjectHandle o,const View& v,const std::vector<Sizes>& desired){
    if(desired.size()!=static_cast<size_t>(v.array.count)||!v.array.count)return false;
    auto cls=reinterpret_cast<UEVR_UStructHandle>(host->sdk->uobject->get_class(o));
    auto fn=host->sdk->ustruct->find_function(cls,L"SetElements");if(!fn)return false;
    auto fs=reinterpret_cast<UEVR_UStructHandle>(fn);
    const int length=host->sdk->ustruct->get_properties_size(fs);
    if(length<16||length>4096)return false;
    UEVR_FPropertyHandle parameter=nullptr;
    auto f=host->sdk->ustruct->get_child_properties(fs);
    for(int i=0;f&&i<32;++i,f=host->sdk->ffield->get_next(f)){
        auto p=reinterpret_cast<UEVR_FPropertyHandle>(f);
        if(!host->sdk->fproperty->is_param(p))continue;
        if(parameter||propertyType(p)!=L"ArrayProperty"||host->sdk->fproperty->is_return_param(p))return false;
        if(!(host->sdk->fproperty->get_property_flags(p)&2ULL))return false; // const input array only
        auto inner=host->sdk->farrayproperty->get_inner(reinterpret_cast<UEVR_FArrayPropertyHandle>(p));
        if(!inner||propertyType(inner)!=L"StructProperty"||host->sdk->fstructproperty->get_struct(reinterpret_cast<UEVR_FStructPropertyHandle>(inner))!=v.structure)return false;
        parameter=p;
    }
    if(!parameter)return false;
    const int offset=host->sdk->fproperty->get_offset(parameter);if(offset<0||offset>length-16)return false;
    // Keep every material, curve, screen-space flag and other field unchanged.
    std::vector<unsigned char> entries(static_cast<size_t>(v.array.count)*static_cast<size_t>(v.stride));
    SIZE_T bytes=0;
    if(!ReadProcessMemory(GetCurrentProcess(),v.array.data,entries.data(),entries.size(),&bytes)||bytes!=entries.size())return false;
    for(size_t i=0;i<desired.size();++i){
        std::memcpy(entries.data()+i*static_cast<size_t>(v.stride)+v.x,&desired[i].x,4);
        std::memcpy(entries.data()+i*static_cast<size_t>(v.stride)+v.y,&desired[i].y,4);
    }
    std::vector<unsigned char> params(static_cast<size_t>(length));
    ArrayView input{entries.data(),v.array.count,v.array.count};
    std::memcpy(params.data()+offset,&input,sizeof(input));
    host->sdk->uobject->process_event(o,fn,params.data());
    View after;std::vector<Sizes> result;
    return view(o,after)&&sizes(after,result)&&equal(result,desired);
}
// Asset-confirmed refueling guide: its TextureBox uses the same screen-space
// billboard path as target pips, but belongs to a different native actor family.
static bool nameMatches(const std::wstring& name,const std::wstring& ownerClass){
    if(name.find(L".PersistentLevel.")==std::wstring::npos||name.find(L"Default__")!=std::wstring::npos)return false;
    const auto kind=componentKind(name);
    if(ownerClass==L"BP_LiveAerialRefuel3DUIActor_C")return kind==4;
    return ownerClass==L"ChronicleTargetContainerActorBP_C"&&kind>=1&&kind<=3;
}
static bool refuelMaterialName(const std::wstring& name){
    const auto leaf=name.substr(name.rfind(L'.')+1);
    return leaf==L"M_HUD_Texture3DUI_ScreenSpaceNoFollowHMD_000"||
        leaf.rfind(L"MID_M_HUD_Texture3DUI_ScreenSpaceNoFollowHMD_000_",0)==0;
}
static bool refuelMaterial(UEVR_UObjectHandle material){
    for(unsigned depth=0;material&&depth<6;++depth){
        if(refuelMaterialName(billboardFname(host->sdk->uobject->get_fname(material))))return true;
        UEVR_UObjectHandle parent=nullptr;
        if(!copy(host->sdk->uobject->get_property_data(material,L"Parent"),parent)||parent==material)return false;
        material=parent;
    }
    return false;
}
static bool eligible(UEVR_UObjectHandle o,const std::wstring& name,View* current=nullptr){
    auto actor=host->sdk->uobject->get_outer(o);
    auto cls=actor?host->sdk->uobject->get_class(actor):nullptr;
    if(!cls||!nameMatches(name,billboardFname(host->sdk->uobject->get_fname(reinterpret_cast<UEVR_UObjectHandle>(cls)))))return false;
    const auto kind=componentKind(name);
    View v;if(!view(o,v)||v.array.count<1||(kind!=4&&v.array.count!=1))return false;
    auto componentClass=reinterpret_cast<UEVR_UStructHandle>(host->sdk->uobject->get_class(o));
    const auto schema=layout_cache.find(componentClass);if(schema==layout_cache.end())return false;
    const int offset=schema->second.material;
    if(offset<0||offset>v.stride-static_cast<int>(sizeof(void*)))return false;
    if(kind==4){
        // Retain every original texture, opacity/distance curve, screen-space
        // flag and animated signed size. Require the asset's actual material.
        for(int i=0;i<v.array.count;++i){
            UEVR_UObjectHandle material=nullptr;
            if(!copy(v.array.data+size_t(i)*size_t(v.stride)+offset,material)||!refuelMaterial(material))return false;
        }
        if(current)*current=v;
        return true;
    }
    UEVR_UObjectHandle m=nullptr;
    if(!copy(v.array.data+offset,m)||!m)return false;
    const auto matches=[&](const std::wstring& path){return kind==1?path.find(L".MID_M_HUD_Container_Escort_000_")!=std::wstring::npos:
        kind==2?path.find(L".MID_MI_HUD_Multi_TargetPinClockwise_002_")!=std::wstring::npos:
        kind==3&&path.find(L".MID_MI_HUD_Multi_TargetPinAntiClockwise_002_")!=std::wstring::npos;};
    const bool match=matches(L"."+billboardFname(host->sdk->uobject->get_fname(m)))||matches(objectName(m));
    if(match&&current)*current=v;
    return match;
}
static Saved desiredState(const std::vector<Sizes>& now,const Saved* previous,float factor=.25f){
    Saved s{now,now};
    // A new game size on one axis must not compound the unchanged scaled axis.
    if(previous&&previous->original.size()==now.size()){
        for(size_t i=0;i<now.size();++i){
            if(std::abs(now[i].x-previous->last[i].x)<=1e-7f)s.original[i].x=previous->original[i].x;
            if(std::abs(now[i].y-previous->last[i].y)<=1e-7f)s.original[i].y=previous->original[i].y;
        }
    }
    s.last=scaled(s.original,factor);s.factor=factor;return s;
}
static bool unchanged(const std::vector<Sizes>& now,const Saved& previous,float factor){
    if(previous.factor!=factor||previous.original.size()!=now.size()||previous.last.size()!=now.size())return false;
    for(size_t i=0;i<now.size();++i)if(now[i].x!=previous.last[i].x||now[i].y!=previous.last[i].y)return false;
    return true;
}
static bool restore(const std::wstring& name,Saved& s){
    auto o=host->sdk->uobject_array->find_uobject((L"MaterialBillboardComponent "+name).c_str());
    if(!o)return true;
    View v;std::vector<Sizes> now;
    if(!view(o,v)||!sizes(v,now))return false;
    if(equal(now,s.last)&&!set(o,v,s.original)){static_cast<void>(0);return false;}
    return true;
}
#include "refueling-layout-group.h"
[[maybe_unused]] static void tick(){
    if(!presentation_allowed.load())return;
    if(directory.empty()){
        wchar_t p[32768]{};host->functions->get_persistent_dir(p,32768);directory=std::filesystem::path(p)/L"data";
        std::error_code error;enabled=std::filesystem::exists(directory/L"AC8_VR-EscortDrawScaleEnabled.flag",error);
    }

    std::wstring level;
    auto pc=host->sdk->functions->get_player_controller(0);UEVR_UObjectHandle hud=nullptr;
    if(pc&&copy(host->sdk->uobject->get_property_data(pc,L"MyHUD"),hud)&&hud){auto name=objectName(hud);auto at=name.find(L".PersistentLevel.");if(at!=std::wstring::npos&&name.find(L"LiveMainHUDParent4K_C_")!=std::wstring::npos)level=name.substr(0,at+17);}
    select_layout_level(level);
    std::set<std::wstring> seen,guideSeen;unsigned matches=0,pinMatches=0,refuelMatches=0;
    if((enabled||pin_scale.load()!=1)&&!level.empty()){
        auto c=host->sdk->uobject_array->find_uobject(L"Class /Script/Engine.MaterialBillboardComponent");
        if(c){
            std::array<UEVR_UObjectHandle,4096> objects{}; // Call-local scratch; no retained object lifetime.
            int count=host->sdk->uobject_hook->get_objects_by_class(reinterpret_cast<UEVR_UClassHandle>(c),objects.data(),4096,false);
            for(int i=0;i<std::min(count,4096);++i){
                auto o=objects[static_cast<size_t>(i)];if(!o)continue;
                // The exact leaf-name test is independent of outer paths and
                // rejects unrelated billboards before constructing full names.
                if(!componentKind(billboardFname(host->sdk->uobject->get_fname(o))))continue;
                View v;auto name=objectName(o);if(name.compare(0,level.size(),level)!=0||!eligible(o,name,&v))continue;
                const auto kind=componentKind(name);if(kind==1&&!enabled)continue;
                ++matches;if(kind==4)++refuelMatches;else if(kind>1){++pinMatches;static_cast<void>(0);}
                if(kind==4){
                    auto actor=host->sdk->uobject->get_outer(o);
                    guideSeen.insert(name);
                    refueling_group::update(actor,name,pin_scale.load());
                    continue; // Never scale individual refueling Elements here.
                }
                std::vector<Sizes> now;if(!sizes(v,now)||now.empty())continue;
                seen.insert(name);
                auto it=saved.find(name);
                const auto factor=guideScale(kind,pin_scale.load());
                if(it!=saved.end()&&unchanged(now,it->second,factor))continue;
                
                // Game-generated new sizes become the new baseline; never compound our own output.
                auto state=desiredState(now,it==saved.end()?nullptr:&it->second,factor);
                if(equal(now,state.last))continue;
                saved[name]=state;
                if(set(o,v,state.last)){
                    static_cast<void>(0);
                    if(kind==4&&!refuel_logged){
                        refuel_logged=true;
                        
                    }
                }
            }
        }
    }
    refueling_group::finish(guideSeen);
    for(auto it=saved.begin();it!=saved.end();){if(!seen.count(it->first)&&restore(it->first,it->second))it=saved.erase(it);else ++it;}
    
}
}
