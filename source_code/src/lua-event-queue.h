#pragma once
// Deferred native -> Lua events for replies to Lua-originated custom events.
//
// UEVR runs Lua SDK callbacks (engine ticks, UFunction hooks) while holding the
// script context lock, and api:dispatch_custom_event invokes our handlers on
// that same stack. dispatch_lua_event takes LuaLoader's access lock first and
// the script context lock second. Calling it from a custom-event handler is
// therefore context -> access, the inverse of every other dispatcher (UEVR's
// own UI/config paths, other plugins' present callbacks). Under contention the
// game thread and the dispatching thread wait on each other forever: the
// picture freezes while audio continues.
//
// Custom-event handlers must post() instead. drain() delivers from a native
// pre-engine tick, where no Lua lock is held, so dispatch keeps UEVR's order.
// The queue mutex is a leaf: never held across dispatch or any UEVR call.
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "../vendor/uevr/plugin-api.h"

namespace ac8_lua_events {
struct Event {const char* name;std::string data;};
inline std::mutex mutex;
inline std::vector<Event> pending; // Guarded by mutex.
inline UEVR_PluginFunctions host{};
inline std::atomic<bool> registered{false};
// Replies are correlated/idempotent on the Lua side; drop excess, never block.
constexpr size_t limit=64;

// `name` must be a string literal (stored by pointer until delivery).
inline void post(const char* name,std::string data) noexcept{
    if(!name||!registered)return;
    try{
        {
            std::lock_guard<std::mutex> lock(mutex);
            for(const auto& queued:pending)if(queued.name==name&&queued.data==data)return;
            if(pending.size()<limit){pending.push_back(Event{name,std::move(data)});return;}
        }
        if(host.log_error)host.log_error("[AC8] Deferred Lua event queue full; reply dropped: %s",name);
    }catch(const std::exception& e){
        if(host.log_error)host.log_error("[AC8] Deferred Lua reply failed: %s",e.what());
    }
}
inline void drain(UEVR_UGameEngineHandle,float){
    std::vector<Event> ready;
    {std::lock_guard<std::mutex> lock(mutex);ready.swap(pending);}
    if(!host.dispatch_lua_event)return;
    for(const auto& event:ready)host.dispatch_lua_event(event.name,event.data.c_str());
}
inline bool initialize(const UEVR_PluginInitializeParam* p){
    if(registered)return true;
    if(!p||!p->functions||!p->functions->dispatch_lua_event||!p->sdk||!p->sdk->callbacks||
       !p->sdk->callbacks->on_pre_engine_tick)return false;
    host=*p->functions;
    const bool ok=p->sdk->callbacks->on_pre_engine_tick(drain);
    registered=ok;
    return ok;
}
} // namespace ac8_lua_events
