#pragma once
// TEMP Plan C: one requested CPU trace through EXISTING D3D callbacks only.
// No GPU instructions, fences, waits, draws, hooks, state/setting/object writes.
#include "selective-glow-inventory.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <utility>

namespace selective_glow_production {
using Microsoft::WRL::ComPtr;
using Object=UEVR_UObjectHandle;
struct Source {
    const char* label="";
    std::string object,reason;
    uintptr_t object_address=0,resource_address=0,rhi_address=0;
    uintptr_t object_resource_field=0;
    ComPtr<ID3D12Resource> native;
    D3D12_RESOURCE_DESC desc{};
};
struct Event {
    const char* kind="";
    uintptr_t list=0,resource=0;
    uint64_t submit=0,frame=0;
    uint32_t before=0,after=0,flags=0,subresource=0;
    bool reset_before_submit=false;
};
struct Submission {
    uint64_t ordinal=0;
    uintptr_t queue=0;
    bool direct=false,same_device=false,uevr_queue=false;
};
inline std::array<Source,12> sources;
inline std::array<Event,512> events;
inline std::array<Submission,128> submissions;
inline size_t event_count=0,submission_count=0;
inline uint64_t submit_ordinal=0,hud_frame=0;
inline uintptr_t hud_list=0;
inline unsigned ticks=0;
inline bool armed=false,finished=false,overflow=false;
inline std::string capture_id,finish_reason;
inline std::filesystem::path directory;
inline ComPtr<ID3D12Device> device;
inline ID3D12CommandQueue* uevr_queue=nullptr;

inline bool retain(uintptr_t address,ID3D12Resource** output){
    __try{return address&&SUCCEEDED(reinterpret_cast<ID3D12Resource*>(address)->QueryInterface(__uuidof(ID3D12Resource),reinterpret_cast<void**>(output)));}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline Source resolve(const UEVR_PluginInitializeParam* api,Object object,const char* label,uintptr_t base,uintptr_t field){
    using namespace selective_glow_inventory;
    Source out;out.label=label;out.object_address=reinterpret_cast<uintptr_t>(object);out.object_resource_field=field;
    if(!live(api,object)){out.reason="target_unavailable";return out;}
    out.object=identity(api,object);
    uintptr_t resource=0,table=0,owner=0,rhi=0,wrapper=0,native=0,again=0;
    // Exact-image historical path, revalidated here; no virtual game call.
    const auto address=reinterpret_cast<uintptr_t>(object);
    if(!read(reinterpret_cast<void*>(address+field),resource)||
       !read(reinterpret_cast<void*>(resource),table)||table!=base+ac8_game_compatibility::rva<0xbea1670>()||
       !read(reinterpret_cast<void*>(resource+0x90),owner)||owner!=address||
       !read(reinterpret_cast<void*>(resource+0x58),rhi)||
       !read(reinterpret_cast<void*>(rhi),table)||table!=base+ac8_game_compatibility::rva<0xb8ce150>()||
       !read(reinterpret_cast<void*>(rhi+0xb8),wrapper)||
       !read(reinterpret_cast<void*>(wrapper+0x20),native)||
       !read(reinterpret_cast<void*>(address+field),again)||again!=resource){
        out.reason="exact_resource_layout_guard_failed";return out;
    }
    ID3D12Resource* result=nullptr;
    if(!retain(native,&result)){out.reason="native_interface_unavailable";return out;}
    out.native.Attach(result);out.desc=result->GetDesc();
    ComPtr<ID3D12Device> actual;
    const auto& d=out.desc;
    if(FAILED(result->GetDevice(IID_PPV_ARGS(&actual)))||actual.Get()!=device.Get()||
       d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||!d.Width||d.Width>8192||!d.Height||d.Height>8192||
       d.DepthOrArraySize!=1||d.MipLevels!=1||d.SampleDesc.Count!=1||
       (d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8A8_TYPELESS)){
        out.native.Reset();out.reason="native_desc_or_device_guard_failed";return out;
    }
    // Resource identity must still match after COM retention and description.
    uintptr_t check_rhi=0,check_wrapper=0,check_native=0;
    if(!live(api,object)||!read(reinterpret_cast<void*>(address+field),again)||again!=resource||
       !read(reinterpret_cast<void*>(resource+0x58),check_rhi)||check_rhi!=rhi||
       !read(reinterpret_cast<void*>(rhi+0xb8),check_wrapper)||check_wrapper!=wrapper||
       !read(reinterpret_cast<void*>(wrapper+0x20),check_native)||check_native!=native){
        out.native.Reset();out.reason="resource_changed_during_resolution";return out;
    }
    out.resource_address=resource;out.rhi_address=rhi;out.reason="identity_pinned_contents_unverified";
    return out;
}
inline bool matches(ID3D12Resource* resource){
    if(!armed||!resource)return false;
    for(const auto& source:sources)if(source.native.Get()==resource)return true;
    return false;
}
inline void append(const Event& event){
    if(event_count==events.size()){overflow=true;finished=true;finish_reason="event_capacity";return;}
    events[event_count++]=event;
}
// Caller holds hud_renderer::mutex throughout every public observer operation.
inline void arm(const UEVR_PluginInitializeParam* api,Object converter,const char* id,const std::filesystem::path& assets){
    if(!api||!api->renderer||!api->renderer->device||!id||!converter)return;
    if(armed){
        const auto file=assets.parent_path().parent_path()/L"data"/(std::string("AC8_VR-SelectiveGlowProduction-")+id+".json");
        if(!std::filesystem::exists(file)){std::ofstream out(file,std::ios::binary);out<<"{\"version\":1,\"readOnly\":true,\"gpuContentsVerified\":false,\"sameFrameGpuReadinessVerified\":false,\"reason\":\"previous_request_still_active\",\"previousId\":"<<selective_glow_inventory::quote(capture_id)<<"}\n";}
        return;
    }
    capture_id=id;directory=assets.parent_path().parent_path()/L"data";
    device=static_cast<ID3D12Device*>(api->renderer->device);
    uevr_queue=static_cast<ID3D12CommandQueue*>(api->renderer->command_queue);
    sources={};events={};submissions={};event_count=submission_count=0;
    submit_ordinal=hud_frame=0;hud_list=0;ticks=0;finished=overflow=false;finish_reason.clear();
    constexpr uintptr_t offsets[]{0x38,0x40,0x48,0xb8,0x108,0x158};
    const char* labels[]{"public_Glow_118","public_Glow_120","public_NoGlowFront_118","public_NoGlowFront_120","public_NoGlowBack_118","public_NoGlowBack_120",
        "bucket_Glow_118","bucket_Glow_120","bucket_NoGlowFront_118","bucket_NoGlowFront_120","bucket_NoGlowBack_118","bucket_NoGlowBack_120"};
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    unsigned resolved=0;
    for(size_t i=0;i<sources.size();++i){
        Object target=nullptr;
        selective_glow_inventory::read(reinterpret_cast<const unsigned char*>(converter)+offsets[i/2],target);
        sources[i]=resolve(api,target,labels[i],base,i%2?0x120:0x118);
        if(sources[i].native)++resolved;
    }
    armed=true;
    if(!resolved){finished=true;finish_reason="no_guarded_native_resources";}
}
inline void barrier(ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    if(!armed||finished||!barriers)return;
    for(UINT i=0;i<count;++i){const auto& b=barriers[i];
        if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||!matches(b.Transition.pResource))continue;
        Event event;event.kind="transition";event.list=reinterpret_cast<uintptr_t>(list);
        event.resource=reinterpret_cast<uintptr_t>(b.Transition.pResource);
        event.before=static_cast<uint32_t>(b.Transition.StateBefore);event.after=static_cast<uint32_t>(b.Transition.StateAfter);
        event.flags=static_cast<uint32_t>(b.Flags);event.subresource=b.Transition.Subresource;append(event);
        if(finished)return;
    }
}
inline void copy(ID3D12GraphicsCommandList* list,ID3D12Resource* dst,ID3D12Resource* src){
    if(!armed||finished)return;
    for(const auto pair:{std::pair<const char*,ID3D12Resource*>{"copy_source",src},{"copy_destination",dst}}){
        if(!matches(pair.second))continue;
        Event event;event.kind=pair.first;event.list=reinterpret_cast<uintptr_t>(list);
        event.resource=reinterpret_cast<uintptr_t>(pair.second);append(event);
    }
}
inline void hud_copy(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,uint64_t frame){
    if(!armed||finished||hud_list||!frame)return;
    hud_list=reinterpret_cast<uintptr_t>(list);hud_frame=frame;
    Event event;event.kind="current_xr_hud_copy";event.list=hud_list;
    event.resource=reinterpret_cast<uintptr_t>(resource);event.frame=frame;append(event);
}
inline void reset(ID3D12GraphicsCommandList* list){
    if(!armed||finished)return;const auto address=reinterpret_cast<uintptr_t>(list);
    for(size_t i=0;i<event_count;++i)if(events[i].list==address&&!events[i].submit)events[i].reset_before_submit=true;
    if(hud_list==address){hud_list=0;hud_frame=0;}
}
inline void execute(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists){
    if(!armed||finished||!lists)return;
    ++submit_ordinal;
    bool relevant=false,hud_submitted=false;
    for(UINT n=0;n<count;++n){const auto address=reinterpret_cast<uintptr_t>(lists[n]);
        for(size_t i=0;i<event_count;++i){auto& event=events[i];
            if(event.list!=address||event.submit||event.reset_before_submit)continue;
            relevant=true;event.submit=submit_ordinal;
        }
        if(address==hud_list)hud_submitted=true;
    }
    if(relevant){
        if(submission_count==submissions.size()){overflow=true;finished=true;finish_reason="submission_capacity";return;}
        ComPtr<ID3D12Device> actual;Submission row;row.ordinal=submit_ordinal;row.queue=reinterpret_cast<uintptr_t>(queue);
        row.direct=queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT;
        row.same_device=SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&actual)))&&actual.Get()==device.Get();
        row.uevr_queue=queue==uevr_queue;submissions[submission_count++]=row;
    }
    if(hud_submitted){finished=true;finish_reason="first_current_xr_hud_list_submitted";}
}
inline void cancel(){if(armed){finished=true;finish_reason="temporary_lua_disabled";}}
struct Report {std::filesystem::path file;std::string body;};
inline Report tick(){
    if(!armed)return{};
    if(!finished&&++ticks>=120){finished=true;finish_reason="bounded_engine_callback_limit";}
    if(!finished)return{};
    using selective_glow_inventory::quote;
    std::ostringstream s;s<<"{\"version\":1,\"readOnly\":true,\"gpuCommandsAdded\":false,\"gpuContentsVerified\":false,\"sameFrameGpuReadinessVerified\":false,\"captureId\":"<<quote(capture_id)
        <<",\"reason\":"<<quote(finish_reason)<<",\"overflow\":"<<(overflow?"true":"false")<<",\"engineCallbacks\":"<<ticks<<",\"hudFrame\":"<<hud_frame<<",\"sources\":[";
    for(size_t i=0;i<sources.size();++i){if(i)s<<',';const auto& r=sources[i];
        s<<"{\"label\":"<<quote(r.label)<<",\"object\":"<<quote(r.object)<<",\"objectAddress\":"<<r.object_address<<",\"objectResourceField\":"<<r.object_resource_field<<",\"resourceAddress\":"<<r.resource_address<<",\"rhiAddress\":"<<r.rhi_address
            <<",\"nativeAddress\":"<<reinterpret_cast<uintptr_t>(r.native.Get())<<",\"reason\":"<<quote(r.reason)<<",\"width\":"<<r.desc.Width<<",\"height\":"<<r.desc.Height<<",\"format\":"<<static_cast<unsigned>(r.desc.Format)<<'}';
    }
    s<<"],\"events\":[";
    for(size_t i=0;i<event_count;++i){if(i)s<<',';const auto& e=events[i];
        s<<"{\"kind\":"<<quote(e.kind)<<",\"list\":"<<e.list<<",\"resource\":"<<e.resource<<",\"submit\":"<<e.submit<<",\"hudFrame\":"<<e.frame<<",\"before\":"<<e.before<<",\"after\":"<<e.after<<",\"flags\":"<<e.flags<<",\"subresource\":"<<e.subresource<<",\"resetBeforeSubmit\":"<<(e.reset_before_submit?"true":"false")<<'}';
    }
    s<<"],\"submissions\":[";
    for(size_t i=0;i<submission_count;++i){if(i)s<<',';const auto& r=submissions[i];s<<"{\"ordinal\":"<<r.ordinal<<",\"queue\":"<<r.queue<<",\"direct\":"<<(r.direct?"true":"false")<<",\"sameDevice\":"<<(r.same_device?"true":"false")<<",\"uevrQueue\":"<<(r.uevr_queue?"true":"false")<<'}';}
    s<<"],\"limit\":\"Only exact-guarded resource+0x58 RHI path observed; other RHI paths remain unknown. No draw/clear, enhanced-barrier, GPU-completion or pixel witness. Earlier native events have no producer frame tag. Zero matches do not prove empty/stale targets. CPU identities and submission order are clues only.\"}\n";
    Report result{directory/("AC8_VR-SelectiveGlowProduction-"+capture_id+".json"),s.str()};
    armed=false;sources={};device.Reset();return result;
}
inline void save(const Report& report){
    if(report.body.empty()||std::filesystem::exists(report.file))return;
    std::ofstream stream(report.file,std::ios::binary);stream<<report.body;
}
}
