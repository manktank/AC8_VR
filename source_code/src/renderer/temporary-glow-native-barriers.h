#pragma once
// TEMP: discover the ACTUAL engine command-list ResourceBarrier endpoint from
// guarded native flush34dfa00. Forward all native work unchanged. If different
// from the existing API endpoint, attach one additional observer on the engine
// callback. Existing source/queue/state proof still decides all admission.
#include "temporary-glow-pass-ownership.h"
#include "temporary-glow-native-barrier-guards.h"
#include <d3d12.h>
namespace selective_glow_native_barriers {
using Flush=uintptr_t(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using Barrier=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*);
inline const UEVR_PluginInitializeParam* api=nullptr;
inline Flush original_flush=nullptr;inline Barrier original_additional=nullptr;
inline void(*observe)(uintptr_t,UINT,const D3D12_RESOURCE_BARRIER*)=nullptr;
inline std::atomic<bool> active{false},ready{false},failed{false};
inline std::atomic<uintptr_t> known_method{0},discovered_method{0},hook_method{0},sample_list{0},last_method{0};
inline std::atomic<uint64_t> native_flushes{0},direct_flushes{0},same_endpoint{0},additional_calls{0};
inline std::atomic<const char*> status{"not_initialized"};
inline bool attach_attempted=false;
inline thread_local unsigned barrier_depth=0;
struct BarrierScope {bool outer=false;BarrierScope():outer(barrier_depth++==0){}~BarrierScope(){--barrier_depth;}};
inline bool direct(uintptr_t command_list)noexcept{
    __try{return reinterpret_cast<ID3D12GraphicsCommandList*>(command_list)->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void STDMETHODCALLTYPE additional(ID3D12GraphicsCommandList* command_list,UINT barrier_count,const D3D12_RESOURCE_BARRIER* values){
    BarrierScope scope;original_additional(command_list,barrier_count,values);
    if(!scope.outer||!values)return;
    if(active.load(std::memory_order_relaxed))++additional_calls; // owned lifetime observation also runs after mode off
    try{if(observe)observe(reinterpret_cast<uintptr_t>(command_list),barrier_count,values);}
    catch(...){failed=true;status="additional_barrier_observer_exception";}
}
inline uintptr_t flush(uintptr_t array_ref,uintptr_t list_data,uintptr_t context,uintptr_t opaque4){
    if(active.load(std::memory_order_relaxed)){
        ++native_flushes;
        if(!hook_method.load(std::memory_order_relaxed)&&!discovered_method.load(std::memory_order_relaxed)&&same_endpoint.load(std::memory_order_relaxed)<64){
            int32_t batch_count=0;uintptr_t command_list=0,methods=0,method=0;
            // Full native function proves count+8 and CommandListData+28, with
            // direct COM ResourceBarrier at vtable26. No arbitrary array scan.
            if(selective_glow_ownership::read(array_ref+8,batch_count)&&batch_count>0&&batch_count<=4096&&
               selective_glow_ownership::read(list_data+0x28,command_list)&&command_list&&
               selective_glow_ownership::read(command_list,methods)&&
               selective_glow_ownership::read(methods+26*8,method)&&method&&direct(command_list)){
                ++direct_flushes;sample_list=command_list;last_method=method;
                if(method==known_method.load()){++same_endpoint;status="native_endpoint_matches_existing_observer";}
                else{uintptr_t none=0;discovered_method.compare_exchange_strong(none,method);status="native_endpoint_discovered";}
            }
        }
    }
    return original_flush(array_ref,list_data,context,opaque4);
}
inline bool executable(uintptr_t method){
    MEMORY_BASIC_INFORMATION info{};if(VirtualQuery(reinterpret_cast<void*>(method),&info,sizeof(info))!=sizeof(info)||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
}
// Only the existing engine callback calls service, outside the renderer lock.
inline bool service(){
    const auto method=discovered_method.load();if(!ready.load()||!active.load()||!method||hook_method.load()||attach_attempted)return false;
    attach_attempted=true;
    if(!executable(method)){failed=true;status="native_endpoint_not_executable";return false;}
    const auto result=api->functions->register_inline_hook(reinterpret_cast<void*>(method),reinterpret_cast<void*>(&additional),reinterpret_cast<void**>(&original_additional));
    if(result<0||!original_additional){failed=true;status="native_endpoint_hook_failed";return false;}
    hook_method=method;status="native_endpoint_observer_attached";return true;
}
inline void session(){native_flushes=0;direct_flushes=0;same_endpoint=0;additional_calls=0;}
inline void json(std::ostream& out){
    out<<"{\"status\":"<<selective_glow_inventory::quote(status.load())<<",\"ready\":"<<(ready.load()?"true":"false")<<",\"failed\":"<<(failed.load()?"true":"false")
       <<",\"nativeFlushes\":"<<native_flushes.load()<<",\"directFlushesSampled\":"<<direct_flushes.load()<<",\"sameEndpointSamples\":"<<same_endpoint.load()<<",\"additionalCalls\":"<<additional_calls.load()
       <<",\"existingBarrierMethod\":"<<known_method.load()<<",\"nativeBarrierMethod\":"<<last_method.load()<<",\"attachedBarrierMethod\":"<<hook_method.load()<<",\"sampleNativeList\":"<<sample_list.load()<<'}';
}
inline void initialize(const UEVR_PluginInitializeParam* host){
    api=host;using namespace selective_glow_native_barrier_guards;
    if(!radio_paint_marker::ready||!selective_glow_ownership::guard(flush_rva,selective_glow_native_barrier_guards::flush)){status="native_flush_guard_failed";return;}
    const auto result=api->functions->register_inline_hook(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::rva<flush_rva>()),reinterpret_cast<void*>(&selective_glow_native_barriers::flush),reinterpret_cast<void**>(&original_flush));
    if(result<0||!original_flush){status="native_flush_hook_failed";return;}
    ready=true;status="waiting_native_barrier_endpoint";
}
}
