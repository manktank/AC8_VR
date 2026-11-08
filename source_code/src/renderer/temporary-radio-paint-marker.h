#pragma once
// TEMP Plan C: explicit body-only native paint marker. Private style copy only.
// No settings/UObject/style writes, extra paint/tick, GPU commands or scene gate.
#include "selective-glow-inventory.h"
#include "temporary-glow-performance.h"
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include "prepaint-frame-policy.h"
#include "live-paint-contracts.h"
#include "../hud/glow-routing-mode.h"

namespace radio_paint_marker {
using Object=UEVR_UObjectHandle;
using Paint=int(*)(void*,const void*,const void*,const void*,void*,int,const void*,bool);
inline Paint original=nullptr;
inline Paint original_common=nullptr;
inline bool common_ready=false;
inline std::atomic<bool> common_enabled{false};
using AllocateSerial=int32_t(*)(UEVR_UObjectArrayHandle,int32_t);
inline AllocateSerial allocate_serial=nullptr;
inline const UEVR_PluginInitializeParam* api=nullptr;
inline uintptr_t image_base=0;
inline bool ready=false;
inline std::filesystem::path status_file;
inline const char* guard_reason="not_initialized";
inline void(*trace_paint_enter)(uintptr_t,uintptr_t)=nullptr;
inline void(*trace_paint_leave)()=nullptr;
inline void(*trace_radar_geometry)(uintptr_t,const prepaint_frame_policy::Rect&,uintptr_t)=nullptr;
struct alignas(16) Style {float values[12]{};};
static_assert(sizeof(Style)==48,"Exact-build three-linear-color paint style");
struct Leaf {
    Object object=nullptr;
    std::array<unsigned char,40> header{};
    uintptr_t registry_slot=0,slate=0,control=0,weak_field=0;
    uintptr_t table=0;
    unsigned roles=0;
    int32_t serial=0;
    bool serial_assigned_by_test=false;
    const char* failure="not_checked";
    std::string name;
};
// The main executable stays mapped for the process lifetime. Cache only its
// already-proved SWidget vtable/paint entry; UObject, weak pointer, live control,
// current vtable and method pointer are still checked for every attachment.
struct SlateTypeCache {uintptr_t image=0;std::unordered_map<uintptr_t,uintptr_t> methods;std::atomic<uint64_t> hits{0},misses{0};};
struct SlateProbe {
    uintptr_t field=0,widget=0,control=0,table=0,paint=0;
    int32_t strong=0;
    bool unreadable=false;
    const char* reason="not_probed";
};
struct MissingSlate {Object object=nullptr;unsigned roles=0;std::string klass;std::array<SlateProbe,2> probes;};
struct Roster {
    std::unordered_map<uintptr_t,Leaf> leaves;
    std::array<unsigned,4> roots{},widgets{};
    unsigned visited=0,missing_slate=0,slate_aliases=0;
    unsigned unreadable_candidates=0;
    unsigned deferred_slate=0;
    std::array<unsigned,4> missing_by_role{};
    std::vector<MissingSlate> missing_examples;
    uint64_t revision=0,completed_at=0;
    std::string failure;
    Object failed_object=nullptr;
    std::string failure_detail;
};
struct Evidence {
    bool present=false;
    uintptr_t slate=0,elements=0,args=0;
    int input_layer=0,output_layer=0;
    std::array<float,4> before{},after{};
    std::array<unsigned char,56> geometry{};
    std::array<float,4> clip{};
};
namespace widget_policy {struct Discovery;struct LiveState;}
struct Session {
    Object hud=nullptr,owner=nullptr;
    std::string id,hud_name;
    std::string selection_failure,failed_object;
    std::array<Leaf,2> bodies;
    unsigned count=0;
    bool trace_only=false;
    std::atomic<uint64_t> traced{0};
    std::atomic<uint64_t> tinted{0},rejected{0};
    std::mutex evidence_mutex;
    std::array<Evidence,2> evidence;
    std::shared_ptr<const Roster> roster;
    std::shared_ptr<widget_policy::LiveState> live_routing;
    // Publish each complete, validated forest independently. Small transient
    // cards must not wait for a full HUD traversal before their first paint.
    std::shared_ptr<const Roster> ordinary_roster,priority_roster,last_failed_roster;
    std::array<std::shared_ptr<const Roster>,4> priority_parts;
    std::array<std::shared_ptr<widget_policy::Discovery>,4> urgent_discoveries;
    std::array<uint64_t,4> next_urgent_refresh{};
    std::array<std::atomic<uint64_t>,4> urgent_cycles{},urgent_first_ms{},urgent_last_ms{},urgent_members{};
    unsigned discovery_turn=0;
    SlateTypeCache slate_types;
    std::array<Leaf,3> policy_classes;
    std::array<uint64_t,3> policy_class_retry{};
    std::array<std::atomic<uint64_t>,6> selected_paints{},elements{},planned_excluded{};
    std::atomic<uint64_t> common_calls{0},common_wrapper_ticks{0},refresh_ticks{0},refreshes{0};
    std::atomic<uint64_t> native_discovery_skips{0};
    std::atomic<uint64_t> discovery_slices{0},discovery_max_ticks{0},discovery_overruns{0},discovery_reused{0},discovery_new{0};
    std::atomic<uint64_t> discovery_class_queries{0},discovery_property_queries{0};
    std::atomic<uint64_t> discovery_successes{0},discovery_failures{0},discovery_last_cycle_ms{0};
    std::array<std::atomic<unsigned>,6> discovery_progress{};
    std::shared_ptr<widget_policy::Discovery> discovery; // Engine thread only; never read in paint.
    std::shared_ptr<widget_policy::Discovery> priority_discovery;
    std::atomic<uint64_t> priority_refreshes{0},priority_last_cycle_ms{0},priority_first_publish_ms{0};
    std::atomic<bool> priority_has_published{false};
    std::atomic<uint64_t> priority_failures{0};
    uint64_t next_refresh=0;
    uint64_t next_priority_refresh=0,discovery_started_at=0,roster_revision=0;
};
inline void refresh_widgets(const std::shared_ptr<Session>& session,bool force=false);
inline std::shared_ptr<Session> current;
inline unsigned sequence=0;
inline std::mutex redraw_mutex;
inline std::vector<Leaf> pending_redraw;
// Guarded reads of this process's memory. The registry/header/serial/weak-pair
// checks below still establish identity; copying does not establish lifetime.
// Avoid a kernel ReadProcessMemory round trip for every discovery field.
inline bool local_copy(void* out,const void* from,size_t bytes)noexcept{
    if(!from)return false;
    __try{std::memcpy(out,from,bytes);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T>inline bool local_read(const void* from,T& out)noexcept{return local_copy(&out,from,sizeof(T));}
inline bool pointer_field(uintptr_t address,uintptr_t& out){return local_read(reinterpret_cast<void*>(address),out);}
// Serial zero means no weak identity was requested yet. On the engine thread,
// ask the game's exact guarded allocator for one, then keep strict comparisons.
// Do not manufacture serials, relax serial validation or call this from paint.
inline bool ensure_serial(Leaf& leaf,int32_t index){
    using namespace selective_glow_inventory;
    const auto read=[](const auto* from,auto& out){return local_read(from,out);};
    std::array<unsigned char,40> before{};uintptr_t registered=0;int32_t actual_index=-1;
    if(index<0||!live(api,leaf.object)||!read(leaf.object,before)||before!=leaf.header||
       !pointer_field(leaf.registry_slot,registered)||registered!=reinterpret_cast<uintptr_t>(leaf.object))return false;
    std::memcpy(&actual_index,before.data()+12,4);if(actual_index!=index)return false;
    if(!read(reinterpret_cast<void*>(leaf.registry_slot+16),leaf.serial)||leaf.serial<0)return false;
    if(!leaf.serial){
        if(!allocate_serial||!api->sdk->functions->get_uobject_array)return false;
        const auto array=api->sdk->functions->get_uobject_array();if(!array)return false;
        const int32_t assigned=allocate_serial(array,index);
        if(assigned<=0||!read(reinterpret_cast<void*>(leaf.registry_slot+16),leaf.serial)||leaf.serial!=assigned)return false;
        leaf.serial_assigned_by_test=true;
    }
    std::array<unsigned char,40> after{};
    return leaf.serial>0&&live(api,leaf.object)&&read(leaf.object,after)&&after==leaf.header&&
        pointer_field(leaf.registry_slot,registered)&&registered==reinterpret_cast<uintptr_t>(leaf.object);
}
inline bool valid(const Leaf& leaf,void* slate){
    const auto read=[](const auto* from,auto& out){return local_read(from,out);};
    if(reinterpret_cast<uintptr_t>(slate)!=leaf.slate||!leaf.object||leaf.serial<=0)return false;
    std::array<unsigned char,40> header{};
    std::array<unsigned char,24> slot{};
    uintptr_t widget=0,control=0,table=0;int32_t strong=0;
    if(!read(leaf.object,header)||header!=leaf.header||
       !read(reinterpret_cast<void*>(leaf.registry_slot),slot))return false;
    uintptr_t registered=0;int32_t serial=0;
    std::memcpy(&registered,slot.data(),8);std::memcpy(&serial,slot.data()+16,4);
    const auto address=reinterpret_cast<uintptr_t>(leaf.object);
    return registered==address&&serial==leaf.serial&&
        pointer_field(address+leaf.weak_field,widget)&&widget==leaf.slate&&
        pointer_field(address+leaf.weak_field+8,control)&&control==leaf.control&&
        read(reinterpret_cast<void*>(control+8),strong)&&strong>0&&
        pointer_field(widget,table)&&table==(leaf.table?leaf.table:image_base+ac8_game_compatibility::rva<0xb515e00>());
}
inline bool marked_style(const void* input,Style& output){
    if(!selective_glow_inventory::read(input,output))return false;
    for(float value:output.values)if(!std::isfinite(value))return false;
    // Preserve all three alpha values and foreground/subdued style colors.
    output.values[0]*=0.10f;output.values[1]*=0.45f;output.values[2]*=1.0f;
    return true;
}
// Reset/control only queues work. UObject invalidation runs on the existing
// engine callback, after revalidation; no extra paint or gameplay tick.
inline void redraw_later(const std::shared_ptr<Session>& session){
    if(!session)return;
    std::lock_guard<std::mutex> lock(redraw_mutex);
    for(unsigned i=0;i<session->count;++i){const auto& leaf=session->bodies[i];
        const auto found=std::find_if(pending_redraw.begin(),pending_redraw.end(),[&](const Leaf& old){return old.object==leaf.object&&old.serial==leaf.serial;});
        if(found==pending_redraw.end()&&pending_redraw.size()<8)pending_redraw.push_back(leaf);
    }
}
inline void engine_tick(){
    selective_glow_performance::frame(false);
    if(auto session=std::atomic_load(&current))if(session->trace_only&&common_ready)refresh_widgets(session);
    std::vector<Leaf> leaves;
    {std::lock_guard<std::mutex> lock(redraw_mutex);leaves.swap(pending_redraw);}
    if(!selective_glow_inventory::available(api)||!api->sdk->uobject->call_function)return;
    for(const auto& leaf:leaves)if(valid(leaf,reinterpret_cast<void*>(leaf.slate))&&selective_glow_inventory::live(api,leaf.object))
        api->sdk->uobject->call_function(leaf.object,L"InvalidateLayoutAndVolatility",nullptr);
}
inline int paint(void* slate,const void* args,const void* geometry,const void* clip,
                 void* elements,int layer,const void* style,bool enabled){
    selective_glow_performance::PaintSample sample;
    const auto forward=[&](const void* draw_style){
        const auto start=sample.session?selective_glow_performance::clock():0;
        const int result=original(slate,args,geometry,clip,elements,layer,draw_style,enabled);
        if(sample.session)sample.original_ticks+=selective_glow_performance::clock()-start;
        return result;
    };
    auto session=std::atomic_load_explicit(&current,std::memory_order_acquire);
    if(!original)return layer; // Unreachable: publication requires a trampoline.
    unsigned selected=2;
    if(session)for(unsigned i=0;i<session->count;++i)if(session->bodies[i].slate==reinterpret_cast<uintptr_t>(slate)){selected=i;break;}
    if(selected==2)return forward(style);
    Style local;
    if(!valid(session->bodies[selected],slate)){
        session->rejected.fetch_add(1,std::memory_order_relaxed);
        return forward(style);
    }
    if(session->trace_only){
        if(common_ready)return forward(style); // Common SWidget boundary owns trace scopes.
        struct Scope {Scope(uintptr_t s,uintptr_t e){if(trace_paint_enter)trace_paint_enter(s,e);}~Scope(){if(trace_paint_leave)trace_paint_leave();}} scope(reinterpret_cast<uintptr_t>(slate),reinterpret_cast<uintptr_t>(elements));
        session->traced.fetch_add(1,std::memory_order_relaxed);return forward(style);
    }
    if(!marked_style(style,local)){session->rejected.fetch_add(1,std::memory_order_relaxed);return forward(style);}
    const int result=forward(&local);
    session->tinted.fetch_add(1,std::memory_order_relaxed);
    {std::lock_guard<std::mutex> lock(session->evidence_mutex);auto& e=session->evidence[selected];
        if(!e.present){
            e.slate=reinterpret_cast<uintptr_t>(slate);e.elements=reinterpret_cast<uintptr_t>(elements);e.args=reinterpret_cast<uintptr_t>(args);
            e.input_layer=layer;e.output_layer=result;
            Style before;if(selective_glow_inventory::read(style,before))std::copy_n(before.values,4,e.before.begin());
            std::copy_n(local.values,4,e.after.begin());
            selective_glow_inventory::read(geometry,e.geometry);selective_glow_inventory::read(clip,e.clip);e.present=true;
        }
    }
    return result;
}
inline bool owned(Object object,Object owner){
    for(unsigned i=0;object&&i<32;++i){if(object==owner)return true;if(!selective_glow_inventory::live(api,object))return false;object=api->sdk->uobject->get_outer(object);}
    return false;
}
inline bool identity_valid(const Leaf& leaf){
    const auto read=[](const auto* from,auto& out){return local_read(from,out);};
    std::array<unsigned char,40> header{};std::array<unsigned char,24> slot{};
    uintptr_t object=0;int32_t serial=0;
    if(!leaf.object||leaf.serial<=0||!read(leaf.object,header)||header!=leaf.header||!read(reinterpret_cast<void*>(leaf.registry_slot),slot))return false;
    std::memcpy(&object,slot.data(),8);std::memcpy(&serial,slot.data()+16,4);
    return object==reinterpret_cast<uintptr_t>(leaf.object)&&serial==leaf.serial;
}
inline bool make_identity(Object object,Leaf& leaf){
    using namespace selective_glow_inventory;
    const auto read=[](const auto* from,auto& out){return local_read(from,out);};
    const auto fail=[&](const char* why){leaf.failure=why;return false;};
    if(!live(api,object))return fail("leaf_not_live");
    leaf.object=object;
    if(!read(object,leaf.header))return fail("header_unreadable");
    uint32_t flags=0;int32_t index=-1;
    std::memcpy(&flags,leaf.header.data()+8,4);std::memcpy(&index,leaf.header.data()+12,4);
    if(flags&(0x10u|0x20u|0x8000u|0x10000u|0x60000000u)||index<0)return fail("header_flags_or_index");
    uintptr_t chunks=0,chunk=0;int32_t count=0;
    if(!pointer_field(image_base+ac8_game_compatibility::rva<0xdc60ce0>(),chunks)||!read(reinterpret_cast<void*>(image_base+ac8_game_compatibility::rva<0xdc60cf4>()),count)||
       index>=count||count>1000000||!pointer_field(chunks+static_cast<uintptr_t>(index/65536)*8,chunk))return fail("registry_layout_or_bounds");
    leaf.registry_slot=chunk+static_cast<uintptr_t>(index%65536)*24;
    if(!ensure_serial(leaf,index))return fail("lifetime_serial_acquisition_failed");
    return true;
}
inline bool readable_candidate(uintptr_t address,size_t bytes){
    // Unused adjacent widget fields can contain nonzero data, not weak pointers.
    // Refusing an unreadable candidate before dereference avoids normal cold
    // discovery raising thousands of first-chance exceptions in the host. This
    // is an extra admission check; SEH and every identity/control/vtable proof
    // remain, since a page can change after VirtualQuery.
    if(address<0x10000||address>UINTPTR_MAX-bytes)return false;
    MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))||page.State!=MEM_COMMIT||
       (page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
       !(page.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    const auto offset=address-reinterpret_cast<uintptr_t>(page.BaseAddress);
    return offset<=page.RegionSize&&bytes<=page.RegionSize-offset;
}
inline bool attach_slate(Leaf& leaf,bool generic,uintptr_t only_field=0,SlateProbe* diagnostic=nullptr,SlateTypeCache* types=nullptr){
    using namespace selective_glow_inventory;
    const auto read=[](const auto* from,auto& out){return local_read(from,out);};
    const auto fail=[&](const char* why){leaf.failure=why;return false;};
    if(!identity_valid(leaf))return fail("leaf_identity_changed");
    const auto object=leaf.object;
    const auto address=reinterpret_cast<uintptr_t>(object);
    for(const uintptr_t field:{uintptr_t{0x108},uintptr_t{0xf8}}){
        if(only_field&&field!=only_field)continue;
        SlateProbe candidate;candidate.field=field;
        const auto reject=[&](const char* reason){candidate.reason=reason;if(diagnostic)*diagnostic=candidate;};
        uintptr_t widget=0,control=0,table=0;int32_t strong=0;
        if(!pointer_field(address+field,widget)||!pointer_field(address+field+8,control)){reject("weak_field_unreadable");continue;}
        candidate.widget=widget;candidate.control=control;
        if(!widget||!control){reject("empty_weak_pair");continue;}
        if(control>UINTPTR_MAX-8||!readable_candidate(control+8,sizeof(strong))||!readable_candidate(widget,sizeof(table))){candidate.unreadable=true;reject("unreadable_weak_candidate");continue;}
        if(!read(reinterpret_cast<void*>(control+8),strong)){reject("control_changed_during_probe");continue;}
        candidate.strong=strong;if(strong<=0){reject("expired_weak_pair");continue;}
        if(!pointer_field(widget,table)){reject("widget_changed_during_probe");continue;}candidate.table=table;
        if(generic){
            if(types&&types->image!=image_base){types->methods.clear();types->image=image_base;}
            uintptr_t on_paint=0;bool proven=false;
            if(types){const auto cached=types->methods.find(table);if(cached!=types->methods.end()&&pointer_field(table+0x238,on_paint)&&on_paint==cached->second){proven=true;++types->hits;}}
            if(!proven){
                MEMORY_BASIC_INFORMATION data{},code{};
                if(types)++types->misses;
                if(!VirtualQuery(reinterpret_cast<void*>(table),&data,sizeof(data))||data.Type!=MEM_IMAGE||
                   data.AllocationBase!=reinterpret_cast<void*>(image_base)){reject("table_outside_game_image");continue;}
                if(!readable_candidate(table+0x238,sizeof(on_paint))||!pointer_field(table+0x238,on_paint)){reject("paint_entry_unreadable");continue;}
                if(!VirtualQuery(reinterpret_cast<void*>(on_paint),&code,sizeof(code))||code.Type!=MEM_IMAGE||
                   code.AllocationBase!=reinterpret_cast<void*>(image_base)||(code.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))==0){reject("paint_outside_executable_game_image");continue;}
                if(types&&types->methods.size()<256)types->methods[table]=on_paint;
            }
            candidate.paint=on_paint;leaf.table=table;
        }else if(table!=image_base+ac8_game_compatibility::rva<0xb515e00>())return fail("cached_slate_type_mismatch");
        leaf.slate=widget;leaf.control=control;leaf.weak_field=field;
        if(!valid(leaf,reinterpret_cast<void*>(widget)))return fail("final_identity_mismatch");
        candidate.reason="ok";if(diagnostic)*diagnostic=candidate;leaf.failure="ok";return true;
    }
    return fail("no_live_cached_slate");
}
inline bool make_leaf(Object object,Leaf& leaf,bool generic=false,uintptr_t only_field=0){
    using namespace selective_glow_inventory;
    if(!make_identity(object,leaf))return false;
    auto cls=reinterpret_cast<UEVR_UClassHandle>(api->sdk->uobject_array->find_uobject(generic?L"Class /Script/UMG.Widget":L"Class /Script/UMG.RichTextBlock"));
    if(!cls||!api->sdk->uobject->is_a(object,cls)){leaf.failure=generic?"not_widget_class":"not_rich_text_class";return false;}
    leaf.name=identity(api,object);
    return attach_slate(leaf,generic,only_field);
}
#include "temporary-glow-widget-selection.h"
inline void report(const char* reason,const std::shared_ptr<Session>& session,bool enabled){
    if constexpr(!selective_glow_performance::reporting)return;
    if(status_file.empty())return;
    using selective_glow_inventory::quote;
    std::ostringstream out;out<<"{\"version\":1,\"test\":\"radio_body_native_paint_marker\",\"enabled\":"<<(enabled?"true":"false")
        <<",\"hookReady\":"<<(ready?"true":"false")<<",\"reason\":"<<quote(reason)<<",\"separateRenderingImplemented\":false,\"savedSettingsChanged\":false,\"bodies\":[";
    if(session){std::lock_guard<std::mutex> lock(session->evidence_mutex);
        for(unsigned i=0;i<session->count;++i){if(i)out<<',';const auto& leaf=session->bodies[i];const auto& e=session->evidence[i];
            out<<"{\"object\":"<<quote(leaf.name)<<",\"slate\":"<<leaf.slate<<",\"serial\":"<<leaf.serial<<",\"paintObserved\":"<<(e.present?"true":"false")
                <<",\"serialAssignedByTest\":"<<(leaf.serial_assigned_by_test?"true":"false")<<",\"elementList\":"<<e.elements<<",\"inputLayer\":"<<e.input_layer<<",\"outputLayer\":"<<e.output_layer<<",\"tintBefore\":[";
            for(unsigned j=0;j<4;++j){if(j)out<<',';out<<e.before[j];}out<<"],\"tintAfter\":[";
            for(unsigned j=0;j<4;++j){if(j)out<<',';out<<e.after[j];}out<<"]}";
        }
        out<<"],\"hud\":"<<quote(session->hud_name)<<",\"id\":"<<quote(session->id)<<",\"tintedPaintCalls\":"<<session->tinted.load()<<",\"rejectedPaintCalls\":"<<session->rejected.load();
        out<<",\"selectionFailure\":"<<quote(session->selection_failure)<<",\"failedObject\":"<<quote(session->failed_object);
        out<<",\"traceOnly\":"<<(session->trace_only?"true":"false")<<",\"tracedSelectedPaintCalls\":"<<session->traced.load();
    }else out<<']';
    out<<",\"limit\":\"Style marker only. Visual confirmation identifies the affected HUD output; no separate GPU sources, current-frame input readiness, draw-list lifetime or cached-routing proof.\"}\n";
    std::ofstream file(status_file,std::ios::binary);file<<out.str();
    if(session&&!enabled){const auto archive=status_file.parent_path()/("AC8_VR-SelectiveGlowPaintMarker-"+session->id+".json");if(!std::filesystem::exists(archive)){std::ofstream saved(archive,std::ios::binary);saved<<out.str();}}
}
inline void off(const char* reason,bool finish_metrics=true){
    common_enabled=false;
    auto prior=std::atomic_exchange_explicit(&current,std::shared_ptr<Session>{},std::memory_order_acq_rel);
    if(prior&&!prior->trace_only)redraw_later(prior);
    if(finish_metrics)selective_glow_performance::stop(reason);
    report(reason,prior,false);
}
inline void control(const char* action){
    if(!action)return;
    if(!std::strcmp(action,"baseline")){
        off("baseline_requested");if(!ready){report(guard_reason,nullptr,false);return;}
        selective_glow_performance::baseline();report("baseline_collecting",nullptr,false);return;
    }
    if(!std::strcmp(action,"off")){off("off_requested");return;}
    const bool trace_only=!std::strcmp(action,"trace");
    if(std::strcmp(action,"on")&&!trace_only)return;
    off("replacing_request",trace_only);
    if(!ready||!selective_glow_inventory::available(api)){report(guard_reason,nullptr,false);return;}
    if(!api->sdk->uobject->call_function){report("targeted_redraw_api_unavailable",nullptr,false);return;}
    using namespace selective_glow_inventory;
    auto pc=api->sdk->functions->get_player_controller(0);auto hud=property(api,pc,L"MyHUD");
    auto root=property(api,hud,L"HudWidget");auto owner=root?api->sdk->uobject->get_outer(root):nullptr;
    if(!live(api,owner)){report("current_hud_owner_unavailable",nullptr,false);return;}
    auto cls=reinterpret_cast<UEVR_UClassHandle>(api->sdk->uobject_array->find_uobject(L"Class /Script/Live.LiveHUDRadioTextWidget"));
    std::array<Object,32> objects{};
    const int count=api->sdk->uobject_hook->get_objects_by_class_name(L"Class /Script/Live.LiveHUDRadioTextWidget",objects.data(),static_cast<unsigned>(objects.size()),false);
    if(!cls||count<0||count>static_cast<int>(objects.size())){report("radio_lookup_bound_or_class_failed",nullptr,false);return;}
    auto session=std::make_shared<Session>();session->hud=hud;session->owner=owner;session->hud_name=identity(api,hud);
    session->trace_only=trace_only;
    session->id=std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);
    for(int i=0;i<count;++i){auto radio=objects[i];
        if(!live(api,radio)||!api->sdk->uobject->is_a(radio,cls)||!owned(radio,owner))continue;
        Leaf body,speaker;
        if(!make_leaf(property(api,radio,L"SubTitleMessageText"),body)||!make_leaf(property(api,radio,L"SpeakerNameText"),speaker)){
            const auto& failed=std::strcmp(body.failure,"ok")?body:speaker;
            session->selection_failure=failed.failure;session->failed_object=failed.name;
            report("radio_leaf_lifetime_or_slate_guard_failed",session,false);return;
        }
        if(body.slate==speaker.slate||session->count==session->bodies.size()){report("ambiguous_radio_selection",session,false);return;}
        bool duplicate=false;for(unsigned j=0;j<session->count;++j)duplicate=duplicate||session->bodies[j].slate==body.slate;
        if(!duplicate)session->bodies[session->count++]=std::move(body);
    }
    if(!session->count){report("no_current_radio_bodies",session,false);return;}
    if(trace_only&&common_ready)refresh_widgets(session,true);
    if(!trace_only)selective_glow_performance::marker();
    std::atomic_store_explicit(&current,session,std::memory_order_release);
    common_enabled=trace_only&&common_ready;
    if(!trace_only)redraw_later(session);
    report(trace_only?"body_trace_enabled":"body_marker_enabled",session,true);
}
inline void initialize(const UEVR_PluginInitializeParam* host,const std::filesystem::path& assets){
    api=host;status_file=assets.parent_path().parent_path()/L"data"/L"AC8_VR-SelectiveGlowPaintMarker-state.json";
    selective_glow_performance::directory=status_file.parent_path();
    // Native glow is admitted by HUD_Glow.lua; test UI presence is irrelevant.
    if(!api||!api->functions||!api->functions->register_inline_hook){guard_reason="hook_api_unavailable";return;}
    const auto module=GetModuleHandleW(nullptr);image_base=reinterpret_cast<uintptr_t>(module);
    
    if(!module||!ac8_game_compatibility::allowed("glow")){
        guard_reason="widget_paint_contract_unavailable";return;
    }
    if(!api->sdk||!api->sdk->functions||!api->sdk->functions->get_uobject_array){guard_reason="lifetime_serial_api_unavailable";return;}
    constexpr std::array<unsigned char,131> serial_expected{{
        0x40,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x88,0x27,0x1b,0x0c,0x7d,0x1e,0x0f,0xb7,0xc2,0x8b,0xca,0x48,0xc1,0xe9,0x10,0x48,0x8d,0x14,0x40,0x48,0x8b,0x05,0x5e,0x27,
        0x1b,0x0c,0x48,0x8b,0x0c,0xc8,0x48,0x8d,0x3c,0xd1,0xeb,0x02,0x33,0xff,0x8b,0x47,0x10,0x85,0xc0,0x75,0x48,0x48,0x89,0x5c,0x24,0x30,0xbb,0x01,0x00,0x00,0x00,0xf0,
        0x0f,0xc1,0x1d,0xd9,0x27,0x1b,0x0c,0xff,0xc3,0x81,0xfb,0xe8,0x03,0x00,0x00,0x7f,0x1b,0x44,0x8b,0xcb,0x4c,0x8d,0x05,0x95,0xcb,0xa0,0x09,0xba,0xc5,0x01,0x00,0x00,
        0x48,0x8d,0x0d,0x19,0xc4,0xa0,0x09,0xe8,0x14,0x5f,0xd1,0xff,0x33,0xc0,0xf0,0x0f,0xb1,0x5f,0x10,0x0f,0x45,0xd8,0x8b,0xc3,0x48,0x8b,0x5c,0x24,0x30,0x48,0x83,0xc4,0x20,0x5f,0xc3}};
    std::array<unsigned char,131> serial_bytes{};
    auto serial_target=reinterpret_cast<void*>(image_base+ac8_game_compatibility::rva<0x1aae560>());
    if(!ac8_game_compatibility::copy_verified_source(serial_target,serial_bytes,serial_expected)||serial_bytes!=serial_expected){guard_reason="lifetime_serial_function_guard_failed";return;}
    allocate_serial=reinterpret_cast<AllocateSerial>(serial_target);
    constexpr std::array<unsigned char,32> expected{{0x48,0x8b,0xc4,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x10,0x01,0x00,0x00,0x0f,0x29,0x70,0xb8,0x0f,0x29,0x78,0xa8,0x44,0x0f,0x29,0x40}};
    std::array<unsigned char,32> bytes{};
    auto target=reinterpret_cast<void*>(image_base+ac8_game_compatibility::rva<0x1eaacb0>());
    if(!ac8_game_compatibility::copy_verified_source(target,bytes,expected)||bytes!=expected){guard_reason="rich_text_paint_entry_guard_failed";return;}
    const int hook=api->functions->register_inline_hook(target,reinterpret_cast<void*>(&paint),reinterpret_cast<void**>(&original));
    ready=hook>=0&&original;guard_reason=ready?"ready":"paint_hook_registration_failed";
    if(!ready)return;
    constexpr std::array<unsigned char,32> common_expected{{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x38,0xfe,0xff,0xff,0x48,0x81,0xec,0xc8,0x02,0x00,0x00,0x48,0x8b,0x05,0xad}};
    target=reinterpret_cast<void*>(image_base+ac8_game_compatibility::rva<0x1c60430>());
    if(!ac8_game_compatibility::copy_verified_source(target,bytes,common_expected)||bytes!=common_expected){ready=false;guard_reason="common_widget_paint_guard_failed";return;}
    common_ready=api->functions->register_inline_hook(target,reinterpret_cast<void*>(&common_paint),reinterpret_cast<void**>(&original_common))>=0&&original_common;
    if(common_ready)widget_policy::initialize_live_routing();
    ready=common_ready;guard_reason=ready?"ready":"common_widget_paint_registration_failed";
}
}
