#pragma once
// Included inside radio_paint_marker. Engine-thread owned-tree discovery only.
// Exact cooked widget classes select entire paint subtrees; no pixel/rect tests.
namespace widget_policy {
constexpr unsigned tutorial_window=4,tutorial_text=8,button_hint=16,notification=32;
inline constexpr const char* family_names[]{"radio_body_0","radio_body_1","tutorial_window","tutorial_text","button_hint","notification"};
inline unsigned class_role(const std::string& path){
    // Default glow includes pause, retry and quit. A common dialog is selected
    // only when its attached CP_002 tutorial contents prove that policy below.
    struct Rule {const char* package;unsigned role;};
    static const Rule rules[]{
        // HOME20261105: explicit tutorial CTA and controller instructions are
        // separate from the popup. These exact tutorial widgets are sharp;
        // ordinary Message/EventNotify/PULL UP classes retain default glow.
        {"/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_GuideText_000",tutorial_text},
        {"/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_GuideText_001",tutorial_text},
        {"/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_ButtonGuide_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_ButtonGuide_Parts_000",button_hint},
        {"/Game/Blueprints/UI/Dialog/WBP_Menu_Common_Dialog_Parts_Contents_CP_002",tutorial_window},
        {"/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_GuidRealButtonWidget_000",button_hint},
        {"/Game/NUI/System/WBP_NUIGuideFooter",button_hint},
        {"/Game/Blueprints/UI/Menu/Common/WBP_MenuCommon_InputGuide_000",button_hint},
        {"/Game/Blueprints/UI/System/Parts/WBP_System_Button_Guide_000",button_hint},
        {"/Game/Blueprints/UI/System/Parts/WBP_System_Button_Guide_Parts_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_001",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_YesNo_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButtonSwitcher_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Icon_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_000",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_002",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_002_Double",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_003",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_004",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_005",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_Command_RealButton_Parts_006",button_hint},
        {"/Game/Blueprints/UI/WBP/HUD4K/Campaign/WBP_HUD_NonGlowMessage_000",notification}
    };
    static const auto exact=[](){std::unordered_map<std::string,unsigned> result;
        for(const auto& rule:rules){const std::string package=rule.package;result.emplace(package+"."+package.substr(package.find_last_of('/')+1)+"_C",rule.role);}return result;}();
    const auto found=exact.find(path);if(found!=exact.end())return found->second;
    return 0;
}
inline bool dialog_class(const std::string& path){return path=="/Game/NUI/Common/WBP_Menu_Common_Dialog_000.WBP_Menu_Common_Dialog_000_C"||path=="/Script/Live.LiveNUICommonDialogWidget";}
inline unsigned named_role(const std::string& class_path,const std::wstring& name,const std::string& outer_widget_class){
    (void)name;
    // Ordinary flight messages and standalone guide text keep default glow.
    // The old LayerOne/EventNotify and whole guide-widget exclusions also caught
    // message text. Only the glyph images of this precise input-hint owner are
    // selected even on a separately encountered image. Its tutorial instructions
    // now inherit the exact control class policy; ordinary message text remains
    // emissive. MISSION START keeps its dedicated class.
    if(class_path=="/Script/UMG.Image"&&outer_widget_class=="/Game/Blueprints/UI/WBP/HUD4K/Common/Tutorial/WBP_HUD_Tutorial_ButtonGuide_Parts_000.WBP_HUD_Tutorial_ButtonGuide_Parts_000_C")return button_hint;
    return 0;
}
struct Node {Object object=nullptr;int parent=-1;unsigned own_role=0,role=0;bool dialog=false;};
struct Walk {
    struct Metadata {Leaf identity;std::string path;std::unordered_map<std::wstring,uintptr_t> fields;};
    std::vector<Node> nodes;
    std::unordered_map<Object,size_t> seen;
    std::unordered_map<Object,Metadata> classes;
    uint64_t property_queries=0;
    const char* failure=nullptr;
    Metadata* metadata(Object object){
        auto cls=reinterpret_cast<Object>(api->sdk->uobject->get_class(object));
        auto it=classes.find(cls);if(it!=classes.end()){if(identity_valid(it->second.identity))return &it->second;classes.erase(it);}
        if(classes.size()>=4096){failure="class_metadata_capacity";return nullptr;}
        Metadata value;if(!make_identity(cls,value.identity)){failure="class_metadata_identity";return nullptr;}
        value.path=selective_glow_inventory::identity(api,cls);if(value.path.empty()){failure="class_metadata_name";return nullptr;}
        return &classes.emplace(cls,std::move(value)).first->second;
    }
    std::string class_path(Object object){const auto value=metadata(object);return value?value->path:std::string{};}
    const void* field(Object from,const wchar_t* key){
        auto value=metadata(from);if(!value)return nullptr;
        constexpr auto absent=~uintptr_t{0};auto found=value->fields.find(key);
        if(found==value->fields.end()){
            ++property_queries;
            const auto address=reinterpret_cast<uintptr_t>(api->sdk->uobject->get_property_data(from,key));
            const auto base=reinterpret_cast<uintptr_t>(from);const auto offset=address?address-base:absent;
            if(address&&(address<base||offset>0x100000)){failure="reflected_field_offset_bounds";return nullptr;}
            found=value->fields.emplace(key,offset).first;
        }
        return found->second==absent?nullptr:reinterpret_cast<const unsigned char*>(from)+found->second;
    }
    bool object(Object from,const wchar_t* key,Object& out){
        out=nullptr;if(!selective_glow_inventory::live(api,from))return false;
        const auto address=field(from,key);if(failure)return false;if(!address)return true;
        return local_read(address,out)&&(!out||selective_glow_inventory::live(api,out));
    }
    void assign(){
        // A shared dialog becomes a tutorial window only with its actual CP_002
        // contents attached below it. Other dialogs are never selected by name.
        for(size_t i=0;i<nodes.size();++i)if(nodes[i].own_role&tutorial_window){
            int at=nodes[i].parent;while(at>=0){auto& n=nodes[static_cast<size_t>(at)];if(n.dialog){n.own_role|=tutorial_window;break;}at=n.parent;}
        }
        for(auto& n:nodes)n.role=n.own_role|(n.parent>=0?nodes[static_cast<size_t>(n.parent)].role:0);
    }
};
inline unsigned roster_roles(const std::shared_ptr<const Roster>& roster,uintptr_t slate){
    if(!roster)return 0;const auto it=roster->leaves.find(slate);
    return it!=roster->leaves.end()&&valid(it->second,reinterpret_cast<void*>(slate))?it->second.roles:0;
}
#include "live-paint-routing.h"
inline unsigned roles(Session& session,const std::shared_ptr<const Roster>& roster,uintptr_t slate,unsigned* paint_style=nullptr){
    bool proven=false;const auto native=live_roles(session,slate,proven);
    unsigned result=proven?(native&63):roster_roles(roster,slate);if(paint_style)*paint_style=native&64;
    for(unsigned i=0;i<session.count;++i)if(session.bodies[i].slate==slate&&valid(session.bodies[i],reinterpret_cast<void*>(slate)))result|=1u<<i;
    return result;
}
inline void count(std::array<std::atomic<uint64_t>,6>& counts,unsigned roles){for(unsigned i=0;i<counts.size();++i)if(roles&(1u<<i))counts[i].fetch_add(1,std::memory_order_relaxed);}
inline void add_leaf(Roster& roster,Leaf leaf){
    auto it=roster.leaves.find(leaf.slate);
    if(it==roster.leaves.end()){roster.leaves.emplace(leaf.slate,std::move(leaf));return;}
    if(!valid(it->second,reinterpret_cast<void*>(it->second.slate))&&valid(leaf,reinterpret_cast<void*>(leaf.slate))){it->second=std::move(leaf);return;}
    // A UserWidget's cached MyWidget can be the same SWidget as its root
    // panel. Both are independently verified selected identities; this is a
    // normal shared handle, not evidence that unselected pixels joined them.
    // Retain one complete lifetime proof; if it expires we refuse until refresh.
    if(it->second.object!=leaf.object)++roster.slate_aliases;
    it->second.roles|=leaf.roles;
}
inline void publish_rosters(Session& session){
    if(!session.ordinary_roster&&(!session.priority_roster||session.priority_roster->leaves.empty())&&
       std::none_of(session.priority_parts.begin(),session.priority_parts.end(),[](const auto& p){return p&&!p->leaves.empty();})&&!std::atomic_load(&session.roster))return;
    auto result=std::make_shared<Roster>();
    // Prioritize the fresh transient proof when a shared handle occurs in both
    // forests. Every leaf still receives its original per-paint lifetime check.
    const std::array<std::shared_ptr<const Roster>,6> sources{session.priority_parts[0],session.priority_parts[1],session.priority_parts[2],session.priority_parts[3],session.priority_roster,session.ordinary_roster};
    for(const auto& source:sources)if(source){
        for(const auto& item:source->leaves)add_leaf(*result,item.second);
        for(size_t i=0;i<result->roots.size();++i){result->roots[i]+=source->roots[i];result->widgets[i]+=source->widgets[i];}
        result->visited+=source->visited;result->missing_slate+=source->missing_slate;result->slate_aliases+=source->slate_aliases;
        result->unreadable_candidates+=source->unreadable_candidates;
        result->deferred_slate+=source->deferred_slate;
        for(size_t i=0;i<result->missing_by_role.size();++i)result->missing_by_role[i]+=source->missing_by_role[i];
        for(const auto& sample:source->missing_examples)if(result->missing_examples.size()<16)result->missing_examples.push_back(sample);
        if(!source->failure.empty()){result->failure=source->failure;result->failed_object=source->failed_object;result->failure_detail=source->failure_detail;}
    }
    if(result->leaves.size()>2048){result->leaves.clear();result->failure="combined_selected_slate_capacity";}
    result->revision=++session.roster_revision;
    std::atomic_store(&session.roster,std::shared_ptr<const Roster>(result));
}
#include "temporary-glow-widget-discovery.h"
inline void json(std::ostream& out,const Session& session){
    const auto roster=std::atomic_load(&session.roster);out<<"{\"commonPaintHookReady\":"<<(common_ready?"true":"false")<<",\"refreshes\":"<<session.refreshes.load()
        <<",\"discoveryCpuUsTotal\":"<<session.refresh_ticks.load()*1000000.0/selective_glow_performance::frequency()
        <<",\"discoverySlices\":"<<session.discovery_slices.load()<<",\"discoveryMaxSliceCpuUs\":"<<session.discovery_max_ticks.load()*1000000.0/selective_glow_performance::frequency()
        <<",\"discoverySliceBudgetUs\":1000,\"discoveryBudgetOverruns\":"<<session.discovery_overruns.load()<<",\"discoveryReusedSlateEntries\":"<<session.discovery_reused.load()<<",\"discoveryNewSlateEntries\":"<<session.discovery_new.load()
        <<",\"discoveryGlobalClassQueries\":"<<session.discovery_class_queries.load()<<",\"discoveryReflectedFieldQueries\":"<<session.discovery_property_queries.load()
        <<",\"discoverySuccessfulCycles\":"<<session.discovery_successes.load()<<",\"discoveryFailedCycles\":"<<session.discovery_failures.load()<<",\"discoveryLastCycleMs\":"<<session.discovery_last_cycle_ms.load()<<",\"publishedMemberCount\":"<<(roster?roster->leaves.size():0)
        <<",\"priorityDiscoveryCycles\":"<<session.priority_refreshes.load()<<",\"priorityDiscoveryLastCycleMs\":"<<session.priority_last_cycle_ms.load()<<",\"priorityHasPublished\":"<<(session.priority_has_published.load()?"true":"false")<<",\"priorityFirstPublishMs\":"<<session.priority_first_publish_ms.load()
        <<",\"ordinaryDiscoveryStage\":"<<session.discovery_progress[0].load()<<",\"ordinaryDiscoveryCursor\":"<<session.discovery_progress[1].load()<<",\"ordinaryDiscoveryVisited\":"<<session.discovery_progress[2].load()
        <<",\"priorityDiscoveryStage\":"<<session.discovery_progress[3].load()<<",\"priorityDiscoveryCursor\":"<<session.discovery_progress[4].load()<<",\"priorityDiscoveryVisited\":"<<session.discovery_progress[5].load()
        <<",\"commonPaintCalls\":"<<session.common_calls.load()<<",\"commonPaintWrapperCpuUsTotal\":"<<session.common_wrapper_ticks.load()*1000000.0/selective_glow_performance::frequency()
        <<",\"rosterRevision\":"<<(roster?roster->revision:0)<<",\"visitedWidgets\":"<<(roster?roster->visited:0)<<",\"missingCachedSlate\":"<<(roster?roster->missing_slate:0)<<",\"sharedSelectedSlateAliases\":"<<(roster?roster->slate_aliases:0)
        <<",\"discoveryFailure\":"<<selective_glow_inventory::quote(roster?roster->failure:"no_roster")<<",\"discoveryFailureObject\":"<<(roster?reinterpret_cast<uintptr_t>(roster->failed_object):0)<<",\"discoveryFailureDetail\":"<<selective_glow_inventory::quote(roster?roster->failure_detail:"")<<",\"families\":[";
    for(unsigned i=0;i<6;++i){if(i)out<<',';out<<"{\"name\":"<<selective_glow_inventory::quote(family_names[i])<<",\"matchedRoots\":"<<(i<2?(i<session.count?1:0):(roster?roster->roots[i-2]:0))
        <<",\"widgetsWithCachedSlate\":"<<(i<2?(i<session.count?1:0):(roster?roster->widgets[i-2]:0))<<",\"selectedPaints\":"<<session.selected_paints[i].load()<<",\"elementInitializations\":"<<session.elements[i].load()<<",\"plannedExcludedDraws\":"<<session.planned_excluded[i].load()<<'}';}
    out<<"],\"independentForests\":[";
    static constexpr const char* components[]{"notification","tutorial_prompts","tutorial_dialog","input_guides"};
    for(unsigned i=0;i<4;++i){if(i)out<<',';out<<"{\"name\":"<<selective_glow_inventory::quote(components[i])<<",\"cycles\":"<<session.urgent_cycles[i].load()<<",\"firstPublishMs\":"<<session.urgent_first_ms[i].load()<<",\"lastCycleMs\":"<<session.urgent_last_ms[i].load()<<",\"members\":"<<session.urgent_members[i].load()<<'}';}
    const auto last_failed=std::atomic_load(&session.last_failed_roster);
    out<<"],\"slateTypeCacheHits\":"<<session.slate_types.hits.load()<<",\"slateTypeImageProofs\":"<<session.slate_types.misses.load()<<",\"lastFailedDiscoveryAtMs\":"<<(last_failed?last_failed->completed_at:0)<<",\"priorityFailedCycles\":"<<session.priority_failures.load()
        <<",\"lastFailedDiscoveryReason\":"<<selective_glow_inventory::quote(last_failed?last_failed->failure:"")<<",\"lastFailedDiscoveryDetail\":"<<selective_glow_inventory::quote(last_failed?last_failed->failure_detail:"")
        <<",\"lastFailedDiscoveryObject\":"<<(last_failed?reinterpret_cast<uintptr_t>(last_failed->failed_object):0)<<",\"unreadableSlateCandidates\":"<<(roster?roster->unreadable_candidates:0)<<",\"deferredColdSlateWidgets\":"<<(roster?roster->deferred_slate:0)<<",\"missingSlateByRole\":[";
    for(unsigned i=0;i<4;++i){if(i)out<<',';out<<(roster?roster->missing_by_role[i]:0);}out<<"],\"missingSlateExamples\":[";
    bool first=true;if(roster)for(const auto& sample:roster->missing_examples){if(!first)out<<',';first=false;out<<"{\"object\":"<<reinterpret_cast<uintptr_t>(sample.object)<<",\"class\":"<<selective_glow_inventory::quote(sample.klass)<<",\"roles\":"<<sample.roles<<",\"probes\":[";
        for(unsigned i=0;i<2;++i){if(i)out<<',';const auto& probe=sample.probes[i];out<<"{\"field\":"<<probe.field<<",\"widget\":"<<probe.widget<<",\"control\":"<<probe.control<<",\"strong\":"<<probe.strong<<",\"table\":"<<probe.table<<",\"paint\":"<<probe.paint<<",\"reason\":"<<selective_glow_inventory::quote(probe.reason)<<'}';}out<<"]}";}
    out<<"],\"members\":[";first=true;if(roster)for(const auto& item:roster->leaves){if(!first)out<<',';first=false;const auto& leaf=item.second;out<<"{\"name\":"<<selective_glow_inventory::quote(leaf.name)<<",\"object\":"<<reinterpret_cast<uintptr_t>(leaf.object)<<",\"roles\":"<<leaf.roles<<",\"slate\":"<<leaf.slate<<",\"serial\":"<<leaf.serial<<'}';}out<<']';live_json(out,session);out<<'}';
}
}
inline void refresh_widgets(const std::shared_ptr<Session>& session,bool force){
    // Current owner routing already resolves fresh and cached descendants at
    // the draw boundary. Running five periodic forests in parallel duplicates
    // that work and delays nothing less. Retain the jobs for an unavailable
    // native contract only; forced startup must not revive duplicate scans.
    if(widget_policy::live_bindings.enabled){++session->native_discovery_skips;return;}
    // Even forced startup receives one bounded slice, never a synchronous walk.
    const auto now=GetTickCount64();
    const auto begin=selective_glow_performance::clock();
    if(!session->discovery)session->discovery=std::make_shared<widget_policy::Discovery>();
    auto& ordinary=*session->discovery;
    if(!ordinary.running&&(force||now>=session->next_refresh))ordinary.begin(*session);
    std::array<widget_policy::Discovery*,4> urgent{};
    for(unsigned i=0;i<urgent.size();++i){
        auto& item=session->urgent_discoveries[i];
        if(!item)item=std::make_shared<widget_policy::Discovery>(true,i+1);
        if(!item->running&&(force||now>=session->next_urgent_refresh[i])){item->widget_class=ordinary.widget_class;item->begin(*session);}
        urgent[i]=item.get();
    }
    if(!ordinary.running&&std::none_of(urgent.begin(),urgent.end(),[](const auto* q){return q->running;}))return;
    const auto budget=static_cast<uint64_t>(selective_glow_performance::frequency()/1000.0);
    // Independent complete forests publish independently. A tiny notification
    // or tutorial prompt never waits for cold dialog/footer Slate handles.
    // Weighted round-robin preserves one shared1ms/512-step budget and gives
    // every queue progress; the cursor persists when one costly step overruns.
    widget_policy::run_scoped_queues(*session,ordinary,urgent,begin,budget,[]{return selective_glow_performance::clock();});
    uint64_t queries=ordinary.walk.property_queries;
    for(const auto* queue:urgent)queries+=queue->walk.property_queries;
    session->discovery_property_queries=queries;
    session->discovery_progress[0]=ordinary.running?ordinary.stage:9;session->discovery_progress[1]=static_cast<unsigned>(ordinary.cursor);session->discovery_progress[2]=static_cast<unsigned>(ordinary.walk.nodes.size());
    const auto* dialog=urgent[2];session->discovery_progress[3]=dialog->running?dialog->stage:9;session->discovery_progress[4]=static_cast<unsigned>(dialog->cursor);session->discovery_progress[5]=static_cast<unsigned>(dialog->walk.nodes.size());
    const auto elapsed=selective_glow_performance::clock()-begin;
    session->refresh_ticks.fetch_add(elapsed,std::memory_order_relaxed);++session->discovery_slices;
    if(elapsed>session->discovery_max_ticks.load())session->discovery_max_ticks=elapsed;
    if(elapsed>budget)++session->discovery_overruns;
}
inline int common_paint(void* slate,const void* args,const void* geometry,const void* clip,void* elements,int layer,const void* style,bool enabled){
    if(!original_common)return layer;
    if(!common_enabled.load(std::memory_order_relaxed))return original_common(slate,args,geometry,clip,elements,layer,style,enabled);
    const auto begin=selective_glow_performance::sample_clock();auto session=std::atomic_load(&current);
    const auto address=reinterpret_cast<uintptr_t>(slate);
    if(session&&session->trace_only)widget_policy::live_before_paint(*session,address);
    unsigned paint_style=0;
    const auto selected=session&&session->trace_only?widget_policy::roles(*session,widget_policy::live_bindings.enabled?std::shared_ptr<const Roster>{}:std::atomic_load(&session->roster),address,&paint_style):0;
    const bool radar=paint_style!=0;
    if((selected||radar)&&trace_paint_enter){trace_paint_enter(address,reinterpret_cast<uintptr_t>(elements));widget_policy::count(session->selected_paints,selected);session->traced.fetch_add(1,std::memory_order_relaxed);}
    if(radar)widget_policy::live_geometry(*session,address,geometry,clip,reinterpret_cast<uintptr_t>(elements));
    const auto original_begin=selective_glow_performance::sample_clock();const int result=original_common(slate,args,geometry,clip,elements,layer,style,enabled);const auto original_end=selective_glow_performance::sample_clock();
    if((selected||radar)&&trace_paint_enter&&trace_paint_leave)trace_paint_leave();
    if constexpr(selective_glow_performance::reporting){
        if(session){session->common_calls.fetch_add(1,std::memory_order_relaxed);session->common_wrapper_ticks.fetch_add(original_begin-begin+selective_glow_performance::clock()-original_end,std::memory_order_relaxed);}
    }
    return result;
}
