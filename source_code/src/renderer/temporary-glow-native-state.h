#pragma once
// TEMP explicit native D3D12 state pilot, ONLY for the rooted private image.
// Called after its queued RHI transition executor on the actual execution list.
// The engine state tracker records the barrier; submitted GPU proof stays mandatory.
#include "temporary-glow-execution.h"
#include "temporary-glow-native-state-guards.h"
#include <d3d12.h>
namespace selective_glow_native_state {
namespace own=selective_glow_ownership;
namespace ex=selective_glow_execution;
using State=void(*)(uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t);
inline State state_call=nullptr;
inline std::atomic<bool> ready{false},active{false},failed{false};
inline std::atomic<uintptr_t> owned_rhi{0},owned_native{0};
inline std::atomic<uintptr_t> second_rhi{0},second_native{0};
inline void bind_second(uintptr_t rhi,uintptr_t native){second_native=native;second_rhi=rhi;}
inline std::atomic<const char*> status{"not_initialized"};
inline std::mutex mutex;
inline uint64_t rtv_calls=0,copy_calls=0,refused=0;
inline uintptr_t last_list=0,last_context=0,last_texture=0;
inline uint32_t last_gpu_mask=0,last_state=0;
inline uint32_t last_context_mask=0;
inline uintptr_t last_context_table=0,last_get_context=0,last_dynamic_table=0;
inline std::atomic<uintptr_t> canonical_dynamic_table{0};
inline bool dynamic_table_proof(uintptr_t base,uintptr_t table){
#ifdef AC8_NATIVE_RESOLUTION_IDENTITY_FIXTURE
    return table==base+0xb8cda50;
#else
    // GDynamicRHI is independently located by reviewed native references. The
    // canonical receiver's table and its state method establish the current
    // backend type; a relocated table need not keep the old numeric RVA.
    // Bind once and retain exact identity thereafter. Other receiver/context,
    // texture ownership, GPU mask, state, epoch and fence checks still apply.
    if(!table||!ac8_game_compatibility::contains(table,0x5f0))return false;
    auto expected=canonical_dynamic_table.load();if(expected)return table==expected;
    uintptr_t method=0;if(!own::read(table+0x5e8,method)||method!=base+ac8_game_compatibility::rva<selective_glow_native_state_guards::explicit_state_rva>())return false;
    canonical_dynamic_table.compare_exchange_strong(expected,table);return !expected||expected==table;
#endif
}
inline selective_glow_performance::Samples cpu;
inline void fail(const char* reason){failed=true;status=reason;std::lock_guard<std::mutex> lock(mutex);++refused;}
inline void bind(uintptr_t owned,uintptr_t native_owned){owned_native=native_owned;owned_rhi=owned;}
inline void apply(const ex::Witness& w){
    if(!active.load()||failed.load()||ex::failed.load()||!ready.load()||!w.owned||(w.rhi!=owned_rhi.load()&&w.rhi!=second_rhi.load()))return;
    // The private UObject stays rooted and its RHI/native resource pinned until
    // process exit. The sharp UI has no entry in this state-changing path.
    if(w.resource_type!=1||!w.completed||!w.texture_readable||w.native!=(w.rhi==owned_rhi.load()?owned_native.load():second_native.load())||w.linked_native!=w.native){fail("explicit_state_private_texture_identity_refused");return;}
    uint32_t desired=0;
    if(w.after==0x800)desired=D3D12_RESOURCE_STATE_RENDER_TARGET;
    else if(w.after==0x40)desired=D3D12_RESOURCE_STATE_COPY_SOURCE;
    else{fail("explicit_state_access_refused");return;}
    uintptr_t dynamic=0,table=0,method=0,context=0,context_table=0,get_context=0;
    uint32_t mask=0,context_mask=0;const auto base=radio_paint_marker::image_base;
    const bool valid=own::read(base+ac8_game_compatibility::rva<0xe485f50>(),dynamic)&&own::read(dynamic,table)&&dynamic_table_proof(base,table)&&
       own::read(table+0x5e8,method)&&method==base+ac8_game_compatibility::rva<selective_glow_native_state_guards::explicit_state_rva>()&&
       own::read(w.list+0x18,context)&&context==w.context&&own::read(context,context_table)&&context_table==base+ac8_game_compatibility::rva<0xb8c6d28>()&&
       own::read(context_table+0x2c8,get_context)&&get_context==base+ac8_game_compatibility::rva<0x3490170>()&&
       own::read(w.list+0x1a8,mask)&&mask&&own::read(context+0x118,context_mask)&&context_mask&&mask==(context_mask&(~context_mask+1u));
    {std::lock_guard<std::mutex> lock(mutex);last_list=w.list;last_context=context;last_texture=w.rhi;last_state=desired;last_gpu_mask=mask;last_context_mask=context_mask;last_context_table=context_table;last_get_context=get_context;last_dynamic_table=table;}
    if(!valid){fail("explicit_state_execution_context_or_mask_refused");return;}
    // Native34e2320 borrows the texture and routes through the execution GPU
    // mask/context into the normal FD3D12 state tracker. No smart ref consumed.
    const auto begin=selective_glow_performance::sample_clock();
    state_call(dynamic,w.list,w.rhi,desired,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    const auto elapsed=selective_glow_performance::sample_clock()-begin;
    std::lock_guard<std::mutex> lock(mutex);cpu.add(elapsed);if(desired==D3D12_RESOURCE_STATE_RENDER_TARGET)++rtv_calls;else ++copy_calls;
    last_list=w.list;last_context=context;last_texture=w.rhi;last_state=desired;last_gpu_mask=mask;status="explicit_native_state_returned_waiting_actual_barrier";
}
inline void session(){
    active=false;second_rhi=second_native=0;std::lock_guard<std::mutex> lock(mutex);rtv_calls=copy_calls=refused=0;last_list=last_context=last_texture=last_context_table=last_get_context=last_dynamic_table=0;last_gpu_mask=last_state=last_context_mask=0;cpu={};
}
inline void json(std::ostream& out){
    std::lock_guard<std::mutex> lock(mutex);
    out<<"{\"ready\":"<<(ready.load()?"true":"false")<<",\"failed\":"<<(failed.load()?"true":"false")<<",\"status\":"<<selective_glow_inventory::quote(status.load())
       <<",\"ownedRtvCalls\":"<<rtv_calls<<",\"ownedCopySourceCalls\":"<<copy_calls<<",\"refused\":"<<refused
       <<",\"lastExecutionList\":"<<last_list<<",\"lastContext\":"<<last_context<<",\"lastTexture\":"<<last_texture<<",\"lastGpuMask\":"<<last_gpu_mask<<",\"lastContextMask\":"<<last_context_mask<<",\"lastContextTable\":"<<last_context_table<<",\"lastGetContext\":"<<last_get_context<<",\"lastDynamicTable\":"<<last_dynamic_table<<",\"lastRequestedD3D12State\":"<<last_state<<",\"nativeStateCpuUs\":";
    selective_glow_performance::samples_json(out,cpu,1000000.0/selective_glow_performance::frequency());
    out<<",\"limit\":\"Rooted private image only. Explicit native state recording is not GPU proof; actual submitted barrier/state/queue/epoch/fence/XR admission remains mandatory. CPU duration is not GPU time.\"}";
}
inline void initialize(){
    using namespace selective_glow_native_state_guards;
    if(!own::ready||!own::guard(explicit_state_rva,explicit_state)||!own::guard(backend_state_rva,backend_state)||!own::guard(get_context_rva,get_context)){failed=true;status="explicit_native_state_guard_failed";return;}
    state_call=reinterpret_cast<State>(radio_paint_marker::image_base+ac8_game_compatibility::rva<explicit_state_rva>());ready=true;status="waiting_explicit_native_state_execution";
}
}
