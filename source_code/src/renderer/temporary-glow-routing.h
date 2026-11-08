#pragma once
// TEMP Plan C. Included inside selective_glow_batch_capture after its types.
// Identity follows native window generations, never asynchronous XR-copy cadence.
// At most 16 selected native element-list associations, with next-paint/current-
// batch owners and bounded metadata recycling. The draw pins a
// completed upload before original native cleanup. Old pointer lookup is forbidden.
struct RoutedWindow {uintptr_t window=0;std::shared_ptr<Capture> painting,active;uint64_t used=0;};
struct RouteEvent {uint64_t sequence=0,generation=0;uintptr_t window=0,data=0;const char* kind="";};
inline std::mutex route_mutex;
inline std::weak_ptr<Capture> route_session;
inline std::vector<RoutedWindow> route_windows;
inline std::vector<uintptr_t> route_recent_windows,route_ui_windows;
inline std::array<RouteEvent,64> route_events{};
inline uint64_t route_generation=0,route_sequence=0,route_copies=0;
inline uint64_t route_clock=0,route_evictions=0,route_pressure_evictions=0,route_skipped_windows=0,route_slot_high_water=0;
inline std::atomic<uint64_t> route_build_serial{0};
inline void route_event(const char* kind,uint64_t generation,uintptr_t window,uintptr_t data=0){
    const auto serial=++route_sequence;route_events[static_cast<size_t>((serial-1)%route_events.size())]={serial,generation,window,data,kind};
}
inline void route_start(const std::shared_ptr<Capture>& root){
    std::lock_guard<std::mutex> lock(route_mutex);route_session=root;route_windows.clear();route_recent_windows.clear();route_ui_windows.clear();route_events={};route_sequence=route_copies=0;route_clock=route_evictions=route_pressure_evictions=route_skipped_windows=route_slot_high_water=0;root->routing_session=true;
}
inline void route_end(){std::lock_guard<std::mutex> lock(route_mutex);route_session.reset();route_windows.clear();route_recent_windows.clear();route_ui_windows.clear();}
inline bool route_live(const std::shared_ptr<Capture>& root){return root&&armed.load()&&!root->done.load()&&std::atomic_load(&current)==root;}
inline std::shared_ptr<Capture> route_new(const std::shared_ptr<Capture>& root,uintptr_t window,const char* reason=nullptr){
    auto c=std::make_shared<Capture>(false);c->continuous=true;c->admission=root->admission;if(root->admission)c->selection=std::atomic_load(&root->admission->roster);c->ui_rhi=root->ui_rhi;c->ui_native=root->ui_native;c->route_root=root;c->route_window=window;c->route_reason=reason;
    c->frame_policy.authoritative=radio_paint_marker::widget_policy::live_bindings.enabled;c->frame_policy.owner=reinterpret_cast<uintptr_t>(c->admission.get());return c;
}
inline bool route_retired(const RoutedWindow& entry){
    if(entry.painting)return false;if(!entry.active)return true;
    const auto& c=entry.active;if(c->in_flight.load()||c->paint_scopes.load())return false;
    std::lock_guard<std::mutex> lock(c->mutex);if(c->done.load())return true;bool built=false;
    for(const auto& p:c->packets){if(!p.window_finished)return false;for(const auto& b:p.builds){built=true;if(!b.consumed)return false;}}
    return built;
}
inline RoutedWindow* route_find(const std::shared_ptr<Capture>& root,uintptr_t window,bool create){
    if(route_session.lock()!=root||!route_live(root))return nullptr;
    for(auto& entry:route_windows)if(entry.window==window){entry.used=++route_clock;return &entry;}
    if(!create)return nullptr;
    if(route_windows.size()>=16){
        // This is optional CPU metadata, not a game/GPU resource. Prefer an
        // idle completed generation, otherwise abandon the oldest association.
        // In-flight scopes retain their shared owner; later lookup refuses an
        // evicted generation. Pressure must never end the whole comparison.
        size_t victim=0;bool retired=false;
        for(size_t i=0;i<route_windows.size();++i){const bool candidate=route_retired(route_windows[i]);if((candidate&&!retired)||(candidate==retired&&route_windows[i].used<route_windows[victim].used)){victim=i;retired=candidate;}}
        auto& entry=route_windows[victim];const auto old=entry.active?entry.active:entry.painting;
        ++route_evictions;if(!retired)++route_pressure_evictions;route_event(retired?"retired_metadata_recycled":"pending_metadata_evicted",old?old->generation:0,entry.window);
        entry={window,{},{},++route_clock};return &entry;
    }
    route_windows.push_back({window,{},{},++route_clock});route_slot_high_water=std::max(route_slot_high_water,static_cast<uint64_t>(route_windows.size()));return &route_windows.back();
}
inline std::shared_ptr<Capture> route_paint(const std::shared_ptr<Capture>& root,uintptr_t window){
    std::lock_guard<std::mutex> lock(route_mutex);auto* entry=route_find(root,window,true);if(!entry)return {};
    if(!entry->painting){entry->painting=route_new(root,window);entry->painting->generation=++route_generation;}
    route_event("paint",entry->painting->generation,window);return entry->painting;
}
inline bool route_has_paint(const std::shared_ptr<Capture>& root,uintptr_t window){
    std::lock_guard<std::mutex> lock(route_mutex);if(route_session.lock()!=root||!route_live(root))return false;
    for(const auto& entry:route_windows)if(entry.window==window)return !!entry.painting;return false;
}
inline std::shared_ptr<Capture> route_window(const std::shared_ptr<Capture>& root,uintptr_t window,bool cached_selected=false){
    std::lock_guard<std::mutex> lock(route_mutex);if(route_session.lock()!=root||!route_live(root))return {};auto* entry=route_find(root,window,false);
    if(std::find(route_recent_windows.begin(),route_recent_windows.end(),window)==route_recent_windows.end()){if(route_recent_windows.size()==32)route_recent_windows.erase(route_recent_windows.begin());route_recent_windows.push_back(window);}
    // Learn UI lists from an ACTUAL draw target, not selected-paint presence.
    // Once learned, observe each fresh generation even when the UI is cached.
    const bool ui=std::find(route_ui_windows.begin(),route_ui_windows.end(),window)!=route_ui_windows.end();
    cached_selected=cached_selected||ui;
    if(!entry&&!cached_selected){++route_skipped_windows;return {};}
    if(!entry)entry=route_find(root,window,true);if(!entry)return {};
    if(!entry->painting&&!cached_selected){entry->active.reset();++route_skipped_windows;route_event("unselected_generation_invalidated",0,window);return {};}
    auto c=entry->painting;if(!c){c=route_new(root,window);c->generation=++route_generation;}
    entry->painting.reset();entry->active=c;
    if(c->paint_scopes.load()){std::lock_guard<std::mutex> capture_lock(c->mutex);c->route_reason="window_batched_with_open_paint";c->done=true;}
    route_event("window",c->generation,window);return c;
}
inline void route_observe_ui(const std::shared_ptr<Capture>& root,uintptr_t target_ref,uintptr_t data){
    uintptr_t target=0;if(!root||!root->ui_rhi||!pointer_field(target_ref,target)||target!=root->ui_rhi)return;
    std::lock_guard<std::mutex> lock(route_mutex);if(route_session.lock()!=root||!route_live(root))return;
    for(const auto window:route_recent_windows)if(data==window+0x48||data==window+0xa8){
        auto known=std::find(route_ui_windows.begin(),route_ui_windows.end(),window);
        if(known!=route_ui_windows.end())route_ui_windows.erase(known);
        else{if(route_ui_windows.size()==16)route_ui_windows.erase(route_ui_windows.begin());route_event("ui_list_target_observed",0,window,data);}
        route_ui_windows.push_back(window);return;
    }
}
inline std::shared_ptr<Capture> route_upload(const std::shared_ptr<Capture>& root,uintptr_t data){
    std::lock_guard<std::mutex> lock(route_mutex);
    if(route_session.lock()==root&&route_live(root))for(auto& entry:route_windows)if(entry.active&&(data==entry.window+0x48||data==entry.window+0xa8)){
        entry.used=++route_clock;
        route_event("upload",entry.active->generation,entry.window,data);return entry.active;
    }
    route_event("upload_without_window",0,0,data);return {};
}
inline std::shared_ptr<Capture> route_draw(const std::shared_ptr<Capture>& root,uintptr_t data){
    std::lock_guard<std::mutex> lock(route_mutex);auto out=route_new(root,0,"native_window_not_observed");
    if(route_session.lock()!=root||!route_live(root)){out->route_reason="routing_session_ended";return out;}
    for(auto& entry:route_windows)if(entry.active&&(data==entry.window+0x48||data==entry.window+0xa8)){
        entry.used=++route_clock;
        auto c=entry.active;std::lock_guard<std::mutex> capture_lock(c->mutex);out->route_window=entry.window;out->generation=c->generation;
        out->route_reason=c->route_reason;out->done=c->done.load();out->unresolved_elements=c->unresolved_elements;out->copy_mismatches=c->copy_mismatches;
        out->tags=c->tags;out->paint_sequence=c->paint_sequence;out->selection=c->selection;
        out->frame_policy=c->frame_policy;out->frame_policy.generation=c->generation;
        Build* built=nullptr;const Packet* owner=nullptr;
        for(auto p=c->packets.rbegin();p!=c->packets.rend()&&!built;++p)if(p->window_finished)for(auto b=p->builds.rbegin();b!=p->builds.rend();++b)if(b->data==data){built=&*b;owner=&*p;break;}
        if(!built)out->route_reason="completed_upload_not_observed";
        else if(built->consumed)out->route_reason="completed_upload_already_consumed";
        else{Packet packet;packet.id=owner->id;packet.window=owner->window;packet.main=owner->main;packet.hdr=owner->hdr;packet.window_finished=true;packet.builds.push_back(*built);out->packets.push_back(std::move(packet));out->draw_source=c;c->in_flight.fetch_add(1);built->consumed=true;}
        route_event(out->route_reason?out->route_reason:"draw_pinned",c->generation,entry.window,data);return out;
    }
    route_event(out->route_reason,0,0,data);return out;
}
inline bool route_current(const std::shared_ptr<Capture>& draw){
    auto root=draw->route_root.lock();if(!route_live(root))return false;
    std::lock_guard<std::mutex> lock(route_mutex);if(route_session.lock()!=root)return false;
    for(const auto& entry:route_windows)if(entry.window==draw->route_window&&entry.active&&entry.active->generation==draw->generation){
        std::lock_guard<std::mutex> capture_lock(entry.active->mutex);
        if(draw->packets.empty()||draw->packets[0].builds.empty())return false;
        const auto& pinned=draw->packets[0].builds[0];
        for(auto p=entry.active->packets.rbegin();p!=entry.active->packets.rend();++p)for(auto b=p->builds.rbegin();b!=p->builds.rend();++b)if(b->data==pinned.data)return b->serial==pinned.serial;
    }return false;
}
inline void route_copy(const std::shared_ptr<Capture>& root,uint64_t){
    std::lock_guard<std::mutex> lock(route_mutex);if(route_session.lock()!=root)return;++route_copies;route_event("xr_copy",0,0);
}
inline void route_json(std::ostream& out){
    std::lock_guard<std::mutex> lock(route_mutex);out<<"{\"scope\":\"current_tracking_interval\",\"windowSlots\":"<<route_windows.size()<<",\"slotHighWater\":"<<route_slot_high_water<<",\"unselectedListsSkipped\":"<<route_skipped_windows<<",\"metadataRecycled\":"<<route_evictions<<",\"pendingMetadataEvicted\":"<<route_pressure_evictions<<",\"xrCopies\":"<<route_copies<<",\"eventsTotal\":"<<route_sequence<<",\"events\":[";
    const uint64_t first=route_sequence>route_events.size()?route_sequence-route_events.size():0;
    for(auto i=first;i<route_sequence;++i){if(i!=first)out<<',';const auto& e=route_events[static_cast<size_t>(i%route_events.size())];out<<"{\"sequence\":"<<e.sequence<<",\"generation\":"<<e.generation<<",\"window\":"<<e.window<<",\"data\":"<<e.data<<",\"kind\":"<<quote(e.kind)<<'}';}out<<"]}";
}
