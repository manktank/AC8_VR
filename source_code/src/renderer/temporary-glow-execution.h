#pragma once
// TEMP Plan C: bounded read-only witnesses at exact native queued executors and
// texture-to-D3D12 mapping. Originals run once unchanged. These CPU witnesses
// NEVER grant source/queue/state/fence/XR admission or change resource identity.
#include "temporary-glow-pass-ownership.h"
#include "temporary-glow-execution-guards.h"
#include "prepaint-frame-policy.h"
namespace selective_glow_execution {
namespace own=selective_glow_ownership;
using Execute=void(*)(uintptr_t,uintptr_t);
using Map=uintptr_t(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline Execute original_begin=nullptr,original_transition=nullptr;
inline Map original_map=nullptr;
inline bool(*needs_transition)()=nullptr;
inline std::atomic<bool> ready{false},active{false},failed{false};
inline std::atomic<uintptr_t> owned_rhi{0},ui_rhi{0},expected_native{0},alignment{0};
inline std::atomic<uintptr_t> second_rhi{0},second_native{0};
inline bool owns(uintptr_t value){return value&&(value==owned_rhi.load()||value==second_rhi.load());}
inline std::atomic<uint64_t> generation{0},begin_calls{0},transition_calls{0},map_calls{0};
inline std::atomic<uint64_t> reads{0},ui_begins{0},ui_transitions{0},ui_maps{0};
inline std::atomic<bool> budget_exhausted{false};
inline std::mutex mutex;
inline constexpr uint64_t read_budget=262144;
struct Witness {
    const char* kind="";bool completed=false,texture_readable=false,owned=false;
    uintptr_t transition=0;
    uintptr_t command=0,list=0,context=0,context_table=0,context_method=0;
    uintptr_t rhi=0,rhi_table=0,wrapper=0,native=0,linked_texture=0,linked_wrapper=0,linked_native=0;
    uintptr_t resolved_wrapper=0,resolved_native=0;
    uint32_t before=0,after=0;uint8_t resource_type=0,tracking_flags=0;
};
inline std::array<Witness,16> witnesses;
inline size_t witness_count=0;
inline uint64_t owned_begins=0,owned_transitions=0,owned_maps=0,unreadable=0,dropped=0;
inline void(*after_transition)(const Witness&)=nullptr;
inline bool sample(){
    if(!active.load(std::memory_order_relaxed)||!owned_rhi.load(std::memory_order_relaxed)||failed.load())return false;
    if(reads.fetch_add(1,std::memory_order_relaxed)>=read_budget){budget_exhausted=true;return false;}return true;
}
inline void texture(Witness& w){
    w.texture_readable=own::read(w.rhi,w.rhi_table)&&own::read(w.rhi+0xb8,w.wrapper)&&
        own::read(w.wrapper+0x20,w.native)&&own::read(w.rhi+0x128,w.linked_texture);
    if(w.linked_texture)w.texture_readable=w.texture_readable&&own::read(w.linked_texture+0xb8,w.linked_wrapper)&&own::read(w.linked_wrapper+0x20,w.linked_native);
}
inline void context(Witness& w,uintptr_t offset){
    if(own::read(w.context,w.context_table))own::read(w.context_table+offset,w.context_method);
}
inline void append(const Witness& w,uint64_t token){
    std::lock_guard<std::mutex> lock(mutex);if(token!=generation.load()||!active.load())return;
    if(w.owned){if(w.kind[0]=='b')++owned_begins;else if(w.kind[0]=='t')++owned_transitions;else ++owned_maps;}
    if(witness_count<witnesses.size())witnesses[witness_count++]=w;else ++dropped;
}
inline void read_failure(){std::lock_guard<std::mutex> lock(mutex);++unreadable;}
template<class F>inline void safe(F work)noexcept{try{work();}catch(...){failed=true;}}
inline Witness begin_witness(uintptr_t command,uintptr_t list){
    Witness w;uintptr_t rhi=0;if(!own::read(command+0x10,rhi)){read_failure();return w;}
    if(rhi==ui_rhi.load()){++ui_begins;return w;}if(!owns(rhi))return w;
    w.kind="begin_pass";w.owned=true;w.command=command;w.list=list;w.rhi=rhi;
    own::read(list+0x10,w.context);context(w,0x250);texture(w);return w;
}
inline Witness transition_witness(uintptr_t command,uintptr_t list,uintptr_t wanted=0){
    Witness w;uintptr_t transition=0;const auto a=alignment.load();
    if(!a||!own::read(command+0x10,transition)||!transition){read_failure();return w;}
    const auto data=(transition+a)&~(a-1);uintptr_t heap=0;int32_t count=0;
    if(!own::read(data+0xe8,heap)||!own::read(data+0xf0,count)||count<0||count>64){read_failure();return w;}
    const auto start=heap?heap:data+8;
    for(int32_t i=0;i<count;++i){
        const auto info=start+static_cast<uintptr_t>(i)*56;uintptr_t rhi=0;
        if(!own::read(info+0x10,rhi)){read_failure();return w;}
        if(rhi==ui_rhi.load())++ui_transitions;
        if(!owns(rhi)||(wanted&&rhi!=wanted))continue;
        w.kind="transition_execute";w.owned=owns(rhi);w.transition=transition;w.command=command;w.list=list;w.rhi=rhi;
        own::read(info+0x18,w.resource_type);own::read(info+0x1c,w.before);own::read(info+0x20,w.after);
        own::read(list+0x18,w.context);context(w,0x38);texture(w);break;
    }return w;
}
inline Witness map_witness(uintptr_t capture,uintptr_t info,uintptr_t resource){
    Witness w;uintptr_t rhi=0;if(!own::read(info+0x10,rhi)){read_failure();return w;}
    if(rhi==ui_rhi.load()){++ui_maps;return w;}if(!owns(rhi))return w;
    w.kind="resource_map";w.owned=true;w.rhi=rhi;own::read(info+0x18,w.resource_type);own::read(info+0x1c,w.before);own::read(info+0x20,w.after);
    // Native34af160 resolves the linked GPU texture before this exact call.
    // R8 is the authoritative FD3D12Resource for this transition, not an SDK guess.
    w.resolved_wrapper=resource;own::read(resource+0x20,w.resolved_native);own::read(resource+0xdf,w.tracking_flags);
    own::read(capture+8,w.context);texture(w);return w;
}
inline void begin_pass(uintptr_t command,uintptr_t list){
    Witness w;const auto token=generation.load();if(active.load())++begin_calls;
    if(sample())safe([&]{w=begin_witness(command,list);});
    original_begin(command,list);
    if(w.rhi){w.completed=true;safe([&]{append(w,token);});}
}
inline void transition_execute(uintptr_t command,uintptr_t list){
    auto policy=prepaint_frame_policy::consume(command,list);
    prepaint_frame_policy::Scope policy_scope(policy?policy:prepaint_frame_policy::executing);
    Witness w,second;const auto token=generation.load();if(active.load())++transition_calls;
    const bool functional=needs_transition&&needs_transition();
    // Functional private state handling does not depend on the
    // diagnostic budget, sampling mode, or witness capacity.
    if(sample()||functional)safe([&]{w=transition_witness(command,list,owned_rhi.load());const auto other=second_rhi.load();if(other&&w.rhi!=other)second=transition_witness(command,list,other);});
    original_transition(command,list);
    if(w.rhi){w.completed=true;safe([&]{append(w,token);if(after_transition)after_transition(w);});}
    if(second.rhi){second.completed=true;safe([&]{append(second,token);if(after_transition)after_transition(second);});}
}
inline uintptr_t resource_map(uintptr_t capture,uintptr_t info,uintptr_t resource,uintptr_t opaque4){
    Witness w;const auto token=generation.load();if(active.load())++map_calls;
    if(sample())safe([&]{w=map_witness(capture,info,resource);});
    const auto result=original_map(capture,info,resource,opaque4);
    if(w.rhi){w.completed=true;safe([&]{append(w,token);});}return result;
}
inline void bind_second(uintptr_t rhi,uintptr_t native){second_native=native;second_rhi=rhi;}
inline void bind(uintptr_t ui,uintptr_t owned,uintptr_t native){ui_rhi=ui;expected_native=native;owned_rhi=owned;}
inline void session(){
    active=false;std::lock_guard<std::mutex> lock(mutex);++generation;
    owned_rhi=ui_rhi=expected_native=second_rhi=second_native=0;begin_calls=transition_calls=map_calls=reads=0;ui_begins=ui_transitions=ui_maps=0;
    budget_exhausted=false;witness_count=0;witnesses={};owned_begins=owned_transitions=owned_maps=unreadable=dropped=0;
}
inline void json(std::ostream& out){
    std::lock_guard<std::mutex> lock(mutex);
    out<<"{\"ready\":"<<(ready.load()?"true":"false")<<",\"failed\":"<<(failed.load()?"true":"false")
       <<",\"readBudget\":"<<read_budget<<",\"readAttempts\":"<<reads.load()<<",\"budgetExhausted\":"<<(budget_exhausted.load()?"true":"false")
       <<",\"beginCalls\":"<<begin_calls.load()<<",\"transitionCalls\":"<<transition_calls.load()<<",\"mapCalls\":"<<map_calls.load()
       <<",\"uiBegins\":"<<ui_begins.load()<<",\"uiTransitions\":"<<ui_transitions.load()<<",\"uiMaps\":"<<ui_maps.load()
       <<",\"ownedBeginsCompleted\":"<<owned_begins<<",\"ownedTransitionsCompleted\":"<<owned_transitions<<",\"ownedMapsCompleted\":"<<owned_maps
       <<",\"unreadable\":"<<unreadable<<",\"dropped\":"<<dropped<<",\"expectedNative\":"<<expected_native.load()<<",\"transitionPrivateAlignment\":"<<alignment.load()<<",\"witnesses\":[";
    for(size_t i=0;i<witness_count;++i){if(i)out<<',';const auto& w=witnesses[i];
        out<<"{\"kind\":"<<selective_glow_inventory::quote(w.kind)<<",\"completed\":"<<(w.completed?"true":"false")<<",\"textureReadable\":"<<(w.texture_readable?"true":"false")<<",\"owned\":"<<(w.owned?"true":"false")<<",\"transition\":"<<w.transition
           <<",\"command\":"<<w.command<<",\"list\":"<<w.list<<",\"context\":"<<w.context<<",\"contextTable\":"<<w.context_table<<",\"contextMethod\":"<<w.context_method
           <<",\"rhi\":"<<w.rhi<<",\"rhiTable\":"<<w.rhi_table<<",\"wrapper\":"<<w.wrapper<<",\"native\":"<<w.native<<",\"linkedTexture\":"<<w.linked_texture<<",\"linkedWrapper\":"<<w.linked_wrapper<<",\"linkedNative\":"<<w.linked_native
           <<",\"resolvedWrapper\":"<<w.resolved_wrapper<<",\"resolvedNative\":"<<w.resolved_native<<",\"trackingFlags\":"<<static_cast<unsigned>(w.tracking_flags)
           <<",\"type\":"<<static_cast<unsigned>(w.resource_type)<<",\"before\":"<<w.before<<",\"after\":"<<w.after<<'}';
    }out<<"],\"limit\":\"Read-only CPU native execution/mapping witnesses only. Never grants GPU completion, image identity, state, queue or XR admission. Diagnostic read overhead is not a performance comparison.\"}";
}
inline void initialize(const UEVR_PluginInitializeParam* host){
    using namespace selective_glow_execution_guards;uintptr_t a=0;
    if(!radio_paint_marker::ready||!own::guard(begin_pass_rva,selective_glow_execution_guards::begin_pass)||
       !own::guard(transition_execute_rva,selective_glow_execution_guards::transition_execute)||!own::guard(resource_map_rva,selective_glow_execution_guards::resource_map)||
       !own::read(radio_paint_marker::image_base+ac8_game_compatibility::rva<0xe4866e0>(),a)||a<1||a>4096||(a&(a-1))){failed=true;return;}alignment=a;
    struct Hook{uintptr_t rva;void* replacement;void** original;};
    const std::array<Hook,3> hooks{{
        {begin_pass_rva,reinterpret_cast<void*>(&selective_glow_execution::begin_pass),reinterpret_cast<void**>(&original_begin)},
        {transition_execute_rva,reinterpret_cast<void*>(&selective_glow_execution::transition_execute),reinterpret_cast<void**>(&original_transition)},
        {resource_map_rva,reinterpret_cast<void*>(&selective_glow_execution::resource_map),reinterpret_cast<void**>(&original_map)}}};
    for(const auto& h:hooks)if(host->functions->register_inline_hook(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::translate(h.rva)),h.replacement,h.original)<0||!*h.original){failed=true;return;}
    ready=true;
}
}
