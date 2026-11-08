#pragma once
// Included inside radio_paint_marker::widget_policy after Walk. This is the
// first-draw route. Discovery remains a compatibility fallback, never a timer
// prerequisite for a newly attached page, notification or radar mode.
#include "prepaint-owner-proof.h"
inline live_owner::Bindings live_bindings;
inline const char* live_status="not_initialized";
template<class T>inline bool live_contract(uint32_t symbol,const T& bytes){
    const auto offset=ac8_game_compatibility::translate(symbol);
    return offset&&ac8_game_compatibility::code_proof(image_base+offset,bytes);
}
inline void initialize_live_routing(){
    using namespace live_paint_contracts;
    const auto table=ac8_game_compatibility::rva<0x21000000>();
    uintptr_t paint=0;
    live_bindings.enabled=table&&live_contract(wrapper_ctor_symbol,wrapper_ctor)&&live_contract(parent_detach_symbol,parent_detach)&&
        live_contract(wrapper_paint_symbol,wrapper_paint)&&live_contract(geometry_size_symbol,geometry_size)&&live_contract(geometry_transform_symbol,geometry_transform)&&
        pointer_field(image_base+table+0x238,paint)&&paint==image_base+ac8_game_compatibility::rva<0x20020000>();
    live_bindings.object_widget_table=image_base+table;
    glow_routing_mode::prepaint_ready.store(live_bindings.enabled,std::memory_order_release);
    live_status=live_bindings.enabled?"prepaint_owner_and_geometry_ready":"prepaint_contract_refused_roster_fallback";
}
inline bool radar_class(const std::string& path){
    static constexpr const char* modes[]{"MiniMapBattle_000_C","MiniMapBattleEnlarged_000_C","MiniMapBattleSearch_000_C","MiniMapBattleWhole_000_C"};
    // Compare the complete cooked class name below; directories are supplied by
    // the game class metadata. Ordinary message widget names are never selected.
    const auto dot=path.find_last_of('.');if(dot==std::string::npos)return false;
    for(const auto* mode:modes)if(path.substr(dot+1)==std::string("WBP_HUD_Radar_Parts_")+mode&&path.substr(0,dot).find("/WBP_HUD_Radar_Parts_")!=std::string::npos)return true;
    return false;
}
struct LiveState {
    struct Link {Leaf from,to,slot;const void* field=nullptr;bool panel=false;};
    struct PointerStamp {const void* address=nullptr;Object value=nullptr;};
    struct SlotStamp {const void* address=nullptr;selective_glow_inventory::Array list{};std::vector<Object> slots;};
    struct Dialog {
        Leaf root;std::vector<Link> path;bool tutorial=false,complete=false;
        std::vector<Leaf> identities;
        std::vector<PointerStamp> pointers;
        std::vector<SlotStamp> slot_arrays;
    };
    struct Cached {live_owner::Decision proof;uint64_t revision=0;};
    struct Radar {Leaf owner,image;live_owner::Decision proof;prepaint_frame_policy::Rect bounds;bool geometry=false;};
    std::recursive_mutex mutex;
    Walk metadata;
    std::unordered_map<uintptr_t,Cached> cache;
    std::unordered_map<Object,Dialog> dialogs;
    std::unordered_map<Object,Radar> radars;
    live_owner::Decision refused;
    uint64_t revision=1,hits=0,misses=0,dialog_searches=0,dialog_nodes=0,geometry_samples=0;
    uint64_t negative_hits=0,dialog_search_ticks=0,dialog_validation_ticks=0;
    bool negative_valid(const Dialog& d){
        // A negative result needs a change detector. Re-read captured structural
        // storage and serial/header proofs, without child enumeration, reflection,
        // class-name lookup, allocations or a timer. This detects deep insertion,
        // same-size slot replacement and reused UObject storage before painting.
        if(d.tutorial||!d.complete||!identity_valid(d.root))return false;
        for(const auto& leaf:d.identities)if(!identity_valid(leaf))return false;
        for(const auto& stamp:d.pointers){Object value=nullptr;if(!local_read(stamp.address,value)||value!=stamp.value)return false;}
        for(const auto& stamp:d.slot_arrays){selective_glow_inventory::Array now{};
            if(!local_read(stamp.address,now)||now.data!=stamp.list.data||now.count!=stamp.list.count||now.capacity!=stamp.list.capacity)return false;
            if(!stamp.slots.empty()&&!same_slots(now.data,stamp.slots))return false;
        }return true;
    }
    static bool same_slots(const Object* data,const std::vector<Object>& expected)noexcept{
        if(!data)return false;
        __try{return std::memcmp(data,expected.data(),expected.size()*sizeof(Object))==0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    bool path_valid(const Dialog& d){
        if(!d.tutorial||!identity_valid(d.root)||d.path.empty())return false;
        for(const auto& link:d.path){Object value=nullptr;
            if(!identity_valid(link.from)||!identity_valid(link.to)||!local_read(link.field,value)||value!=link.to.object)return false;
            if(link.panel){Object parent=nullptr,content=nullptr,back=nullptr;
                if(!identity_valid(link.slot)||!metadata.object(link.slot.object,L"Parent",parent)||parent!=link.from.object||
                   !metadata.object(link.slot.object,L"Content",content)||content!=link.to.object||!metadata.object(content,L"Slot",back)||back!=link.slot.object)return false;
                selective_glow_inventory::Array slots{};const auto field=metadata.field(parent,L"Slots");
                if(!field||!local_read(field,slots)||slots.count<0||slots.count>4096||slots.capacity<slots.count)return false;
                bool found=false;for(int i=0;i<slots.count;++i){Object slot=nullptr;if(!local_read(slots.data+i,slot))return false;if(slot==link.slot.object){found=true;break;}}
                if(!found)return false;
            }
        }return (class_role(metadata.class_path(d.path.back().to.object))&tutorial_window)!=0;
    }
    bool find_tutorial(Object root,Dialog& out){
        struct Pending {Object object=nullptr;std::vector<Link> path;};
        std::vector<Pending> pending{{root,{}}};std::unordered_map<Object,bool> seen;
        ++dialog_searches;out.complete=true;
        const auto remember=[&](Object object){Leaf leaf;if(!make_identity(object,leaf)){out.complete=false;return false;}out.identities.push_back(std::move(leaf));return true;};
        const auto pointer=[&](Object object,const wchar_t* key,Object& value){
            value=nullptr;const auto at=metadata.field(object,key);if(metadata.failure){out.complete=false;return false;}
            if(!at)return true;
            if(!local_read(at,value)||(value&&!selective_glow_inventory::live(api,value))){out.complete=false;return false;}
            out.pointers.push_back({at,value});return true;
        };
        while(!pending.empty()&&seen.size()<2048){auto next=std::move(pending.back());pending.pop_back();if(seen.count(next.object))continue;seen[next.object]=true;++dialog_nodes;
            if(!remember(next.object))continue;
            if(next.object!=root&&(class_role(metadata.class_path(next.object))&tutorial_window)){out.path=std::move(next.path);out.tutorial=true;return path_valid(out);}
            if(next.path.size()>=32){out.complete=false;continue;}
            const auto follow=[&](Object child,const void* field,Object slot){
                if(!child)return;Link edge;if(!make_identity(next.object,edge.from)||!make_identity(child,edge.to)){out.complete=false;return;}
                edge.field=field;edge.panel=slot!=nullptr;if(slot&&!make_identity(slot,edge.slot)){out.complete=false;return;}
                auto path=next.path;path.push_back(std::move(edge));pending.push_back({child,std::move(path)});
            };
            Object tree=nullptr,child=nullptr;
            if(pointer(next.object,L"WidgetTree",tree)&&tree){
                if(api->sdk->uobject->get_outer(tree)==next.object)follow(tree,metadata.field(next.object,L"WidgetTree"),nullptr);else out.complete=false;
            }
            if(pointer(next.object,L"RootWidget",child)&&child){
                if(api->sdk->uobject->get_outer(child)==next.object)follow(child,metadata.field(next.object,L"RootWidget"),nullptr);else out.complete=false;
            }
            const auto slots_field=metadata.field(next.object,L"Slots");selective_glow_inventory::Array slots{};
            if(metadata.failure)out.complete=false;
            if(slots_field){
                if(!local_read(slots_field,slots)||slots.count<0||slots.count>512||slots.capacity<slots.count||(!slots.data&&slots.count)){out.complete=false;continue;}
                SlotStamp stamp;stamp.address=slots_field;stamp.list=slots;stamp.slots.resize(static_cast<size_t>(slots.count));
                if(slots.count&&!local_copy(stamp.slots.data(),slots.data,stamp.slots.size()*sizeof(Object))){out.complete=false;continue;}
                out.slot_arrays.push_back(std::move(stamp));
                for(int i=0;i<slots.count;++i){Object slot=nullptr,parent=nullptr,content=nullptr,back=nullptr;
                    if(local_read(slots.data+i,slot)&&remember(slot)&&pointer(slot,L"Parent",parent)&&parent==next.object&&pointer(slot,L"Content",content)&&content&&pointer(content,L"Slot",back)&&back==slot)
                        follow(content,metadata.field(slot,L"Content"),slot);
                    else out.complete=false;
                }
            }
        }if(!pending.empty())out.complete=false;return false;
    }
    void dialog_before_paint(Object root){
        metadata.failure=nullptr;auto& entry=dialogs[root];
        const auto begin=selective_glow_performance::sample_clock();
        const bool positive=path_valid(entry),negative=!positive&&negative_valid(entry);dialog_validation_ticks+=selective_glow_performance::sample_clock()-begin;
        if(positive||negative){if(negative)++negative_hits;return;}
        const auto search_begin=selective_glow_performance::sample_clock();
        const bool was=entry.tutorial;entry={};if(make_identity(root,entry.root))find_tutorial(root,entry);
        dialog_search_ticks+=selective_glow_performance::sample_clock()-search_begin;
        if(was!=entry.tutorial){++revision;cache.clear();}
    }
    unsigned policy(Session& session,Object object){
        if(!owned(object,session.owner))return 0;
        const auto path=metadata.class_path(object);unsigned result=class_role(path);
        if(dialog_class(path)){const auto it=dialogs.find(object);if(it!=dialogs.end()&&path_valid(it->second))result|=tutorial_window;}
        if(radar_class(path))result|=64;
        return result;
    }
    const live_owner::Decision& resolve(Session& session,uintptr_t slate){
        auto binding=live_bindings;binding.owner_generation=reinterpret_cast<uintptr_t>(&session);
        auto found=cache.find(slate);
        if(found!=cache.end()&&found->second.revision==revision&&live_owner::validate(found->second.proof,binding)){++hits;return found->second.proof;}
        ++misses;metadata.failure=nullptr;
        auto proof=live_owner::classify(slate,binding,[&](Object object){return policy(session,object);});
        if(cache.size()>=8192)cache.clear();if(proof.proven)return cache.insert_or_assign(slate,Cached{std::move(proof),revision}).first->second.proof;
        refused=std::move(proof);return refused;
    }
    Radar* radar(const live_owner::Decision& proof){
        if(!(proof.roles&64))return nullptr;auto& result=radars[proof.owner.object];
        Object image=nullptr;metadata.failure=nullptr;
        if(!metadata.object(proof.owner.object,L"MiniMapBackground",image)||!image||!owned(image,proof.owner.object)||metadata.class_path(image)!="/Script/UMG.Image")return nullptr;
        if(!identity_valid(result.owner)||!valid(result.image,reinterpret_cast<void*>(result.image.slate))||result.image.object!=image){
            result={};result.owner=proof.owner;if(!make_identity(image,result.image)||!attach_slate(result.image,true,0,nullptr))return nullptr;
        }return &result;
    }
};
inline std::mutex live_creation_mutex;
inline std::shared_ptr<LiveState> live_state(Session& session){
    auto result=std::atomic_load(&session.live_routing);if(result)return result;
    std::lock_guard<std::mutex> lock(live_creation_mutex);result=std::atomic_load(&session.live_routing);
    if(!result){result=std::make_shared<LiveState>();std::atomic_store(&session.live_routing,result);}return result;
}
inline live_owner::Decision live_resolve(Session& session,uintptr_t slate){
    if(!live_bindings.enabled)return{};auto state=live_state(session);std::lock_guard<std::recursive_mutex> guard(state->mutex);return state->resolve(session,slate);
}
inline unsigned live_roles(Session& session,uintptr_t slate,bool& proven){
    proven=false;if(!live_bindings.enabled)return 0;auto state=live_state(session);std::lock_guard<std::recursive_mutex> guard(state->mutex);
    const auto& result=state->resolve(session,slate);proven=result.proven;return result.roles;
}
inline void live_json(std::ostream& out,const Session& session){
    out<<",\"prepaintStatus\":"<<selective_glow_inventory::quote(live_status);
    out<<",\"periodicDiscoveryBypassed\":"<<session.native_discovery_skips.load();
    out<<",\"portraitDiscoveryRequests\":"<<glow_routing_mode::portrait_requests.load()<<",\"portraitDiscoveryNodes\":"<<glow_routing_mode::portrait_nodes.load()<<",\"radarDiscoveryBypassed\":"<<glow_routing_mode::radar_walks_bypassed.load();
    auto state=std::atomic_load(&session.live_routing);if(state){std::lock_guard<std::recursive_mutex> guard(state->mutex);
        out<<",\"prepaintCacheHits\":"<<state->hits<<",\"prepaintColdOwners\":"<<state->misses<<",\"prepaintDialogSearches\":"<<state->dialog_searches<<",\"prepaintDialogNodes\":"<<state->dialog_nodes<<",\"prepaintRadarGeometrySamples\":"<<state->geometry_samples
            <<",\"dialogNegativeCacheHits\":"<<state->negative_hits<<",\"dialogSearchCpuUs\":"<<state->dialog_search_ticks*1000000.0/selective_glow_performance::frequency()<<",\"dialogValidationCpuUs\":"<<state->dialog_validation_ticks*1000000.0/selective_glow_performance::frequency();}
    out<<",\"framePolicyQueued\":"<<prepaint_frame_policy::queued.load()<<",\"framePolicyExecuted\":"<<prepaint_frame_policy::executed.load()<<",\"framePolicyProducerBindings\":"<<prepaint_frame_policy::bound.load();
}
inline void live_before_paint(Session& session,uintptr_t slate){
    if(!live_bindings.enabled)return;uintptr_t table=0;Object object=nullptr;
    if(!pointer_field(slate,table)||table!=live_bindings.object_widget_table||!local_read(reinterpret_cast<void*>(slate+0x298),object))return;
    auto state=live_state(session);std::lock_guard<std::recursive_mutex> guard(state->mutex);
    if(selective_glow_inventory::live(api,object)&&owned(object,session.owner)&&dialog_class(state->metadata.class_path(object)))state->dialog_before_paint(object);
}
inline bool radar_geometry(const void* geometry,const void* clip,prepaint_frame_policy::Rect& out){
    std::array<float,14> g{};std::array<float,4> c{};if(!local_read(geometry,g)||!local_read(clip,c))return false;
    for(float v:g)if(!std::isfinite(v))return false;for(float v:c)if(!std::isfinite(v))return false;
    if(g[0]<=0||g[1]<=0||std::abs(g[8])>.0001f||std::abs(g[9])>.0001f)return false;
    const float x=g[0]*g[7]+g[11],y=g[1]*g[10]+g[12];
    out={std::max(std::min(g[11],x),c[0]),std::max(std::min(g[12],y),c[1]),std::min(std::max(g[11],x),c[2]),std::min(std::max(g[12],y),c[3])};
    return out.left<out.right&&out.top<out.bottom;
}
inline void live_geometry(Session& session,uintptr_t slate,const void* geometry,const void* clip,uintptr_t elements){
    if(!live_bindings.enabled||!trace_radar_geometry)return;auto state=live_state(session);std::lock_guard<std::recursive_mutex> guard(state->mutex);
    const auto proof=state->resolve(session,slate);auto* radar=state->radar(proof);if(!radar||radar->image.slate!=slate)return;
    prepaint_frame_policy::Rect rect;if(!radar_geometry(geometry,clip,rect))return;
    radar->bounds=rect;radar->proof=proof;radar->geometry=true;++state->geometry_samples;trace_radar_geometry(slate,rect,elements);
}
inline bool live_cached_radar(Session& session,uintptr_t slate,prepaint_frame_policy::Rect& rect){
    if(!live_bindings.enabled)return false;auto state=live_state(session);std::lock_guard<std::recursive_mutex> guard(state->mutex);
    const auto proof=state->resolve(session,slate);auto* radar=state->radar(proof);
    auto binding=live_bindings;binding.owner_generation=reinterpret_cast<uintptr_t>(&session);
    if(!radar||!radar->geometry||!live_owner::validate(radar->proof,binding))return false;rect=radar->bounds;return true;
}
