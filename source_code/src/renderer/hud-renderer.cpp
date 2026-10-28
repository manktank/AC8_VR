// Coordinate D3D12 HUD-copy observation, glow generation and per-eye composition.
// The recursive lock serializes host callbacks with the OpenXR bridge. Only
// current, fully covered, waited images may be written; fence completion protects
// resource reuse. Unsupported/reset/error states disable this path safely.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../vendor/uevr/backend-api.h"
#include "render-utils.h"
#include "hud_route_scene.h"
#include "openxr_handoff.h"
#include "hud_submission_layout.h"
#include <unordered_set>
#include <fstream>
#include <intrin.h>
#include <cstdio>
#include <sstream>

namespace hud_renderer {
using Microsoft::WRL::ComPtr;
const UEVR_PluginInitializeParam* api=nullptr;
std::unique_ptr<hud_route_glow::Renderer> renderer;
std::unique_ptr<hud_route_scene::Renderer> scene_renderer;
openxr_handoff::Bridge bridge;
ComPtr<ID3D12Resource> ui;
ComPtr<ID3D12Fence> fence;
ComPtr<ID3D12Device> device;
std::recursive_mutex mutex;
std::unordered_set<ID3D12CommandList*> recorded,known_direct_lists;
std::filesystem::path assets_path;







uint64_t fence_value=0,scene_consumer=0,generation_frame=0,generation_fence=0;
std::atomic<bool> active{false},overlay{false};
std::atomic<bool> refresh_pending{false};
std::atomic<bool> flight_allowed{false};
std::atomic<bool> cover_active{false};
std::atomic<const char*> refresh_reason{"glow_activation"};
bool failed=false,bridge_installed=false,late_session_recovered=false;
bool glow_requested=true;
bool try_portable_bridge();

bool startup_reset_pending=false;
bool reset_pending=false;
ULONGLONG next_resource_retry=0;
uint64_t next_bridge_retry=0;
std::atomic<bool> brightness_report_requested{true};
bool reported_brightness=false;
const char* reported_state="";
const char* failure_reason="none";
const char* state="waiting_for_flight";


thread_local bool nested=false;
struct NestedScope {bool saved;NestedScope():saved(nested){nested=true;}~NestedScope(){nested=saved;}};
struct CopyRecord {
    acquired_images::Token token{};ComPtr<ID3D12Resource> resource;
    ID3D12CommandList* list=nullptr;uint64_t frame=0,ticket=0;
    D3D12_RESOURCE_STATES final_state=D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_RECT area{};bool hud=false,generated=false,valid=true;
};
std::vector<CopyRecord> copies;
using CopyFn=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12Resource*,ID3D12Resource*);
using RegionFn=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,const D3D12_TEXTURE_COPY_LOCATION*,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION*,const D3D12_BOX*);
using BarrierFn=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*);
using ExecuteFn=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*);
using ResetFn=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*);
CopyFn original_copy=nullptr;RegionFn original_region=nullptr;BarrierFn original_barrier=nullptr;ExecuteFn original_execute=nullptr;ResetFn original_reset=nullptr;

ID3D12Resource* safe_ui(){
    __try{auto* texture=api->sdk->stereo_hook->get_ui_render_target();return texture?static_cast<ID3D12Resource*>(api->sdk->frhitexture2d->get_native_resource(texture)):nullptr;}
    __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void stop(const char* reason){active=false;if(renderer)renderer->policy({});state=reason;
    if(failed&&std::strcmp(reason,"left_flight_off")&&std::strcmp(reason,"manual_off"))failure_reason=reason;
    }
bool enabled(){return active&&!failed&&!cover_active;}
hud_policy::PacketCache policy_packets;
void lifecycle(const char* event,const char* data){
    


    // Loading owns the black surround even if an old flight HUD survives.
    // The control already exists; no new poll, render pass or shader is needed.
    if(event&&data&&!std::strcmp(event,"AC8_TransitionCover")){
        float opacity=0;unsigned loading=0;
        if(sscanf_s(data,"%f|%u",&opacity,&loading)!=2||!std::isfinite(opacity)||opacity<0||opacity>1||loading>1)return;
        const bool covered=opacity>0;
        std::lock_guard<std::recursive_mutex> guard(mutex);
        const bool previous=cover_active.exchange(covered);
        if(previous!=covered){
            copies.clear();generation_frame=generation_fence=0;
            if(covered)stop("loading_cover_off");
            else if(flight_allowed&&glow_requested&&!failed&&!reset_pending){active=true;refresh_reason="cover_released";refresh_pending=true;state="waiting_current_scene_pair";}
        }
        return;
    }


    if(event&&!std::strcmp(event,"AC8_GlowFixQuery")){brightness_report_requested=true;return;}
    if(event&&data&&!std::strcmp(event,"AC8_HUD_PolicyV50")){std::lock_guard<std::recursive_mutex> guard(mutex);const auto policy=policy_packets.update(data,GetTickCount64());if(renderer)renderer->policy(policy);return;}
    if(!event||!data||std::strcmp(event,"AC8_HUD_GlowLifecycle"))return;
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if(!std::strcmp(data,"toggle")){
        glow_requested=!glow_requested;
        if(!glow_requested)stop("manual_off");
        else if(flight_allowed&&!failed&&!reset_pending){active=true;copies.clear();generation_frame=generation_fence=0;refresh_reason="manual_on";refresh_pending=true;state="waiting_current_scene_pair";}
        return;
    }
    if(!std::strcmp(data,"mission_entry")||!std::strcmp(data,"flight")){
        if(!std::strcmp(data,"mission_entry")){glow_requested=true;overlay=false;}
        else flight_allowed=true;
        if(!flight_allowed||!glow_requested||failed||reset_pending)return;
        active=true;copies.clear();generation_frame=generation_fence=0;
        refresh_reason="glow_activation";refresh_pending=true;
        state="waiting_current_scene_pair";return;
    }
    if(!std::strcmp(data,"outside_flight")){
        flight_allowed=false;copies.clear();generation_frame=generation_fence=0;
        stop("left_flight_off");return;
    }
    if(!std::strcmp(data,"pause_enter")||!std::strcmp(data,"pause_exit")){
        
        copies.clear();generation_frame=generation_fence=0;
        refresh_reason=!std::strcmp(data,"pause_enter")?"pause_enter":"pause_exit";
        refresh_pending=true;
        if(active)state="waiting_fresh_pause_frame";
    }
}

void engine_tick(UEVR_UGameEngineHandle,float){
    static_cast<void>(0);
    // F9 is owned by the native renderer. Send its actual requested flight state
    // on the game thread so Lua can pair reversible widget opacity with glow.
    // Ordinary resource resets retain the comparison choice; menus/fatal errors
    // restore the user's source appearance. Never invoke Lua under our lock.
    bool brightness=false;
    {std::lock_guard<std::recursive_mutex> guard(mutex);brightness=flight_allowed&&glow_requested&&!failed;}
    if(brightness_report_requested.exchange(false)||reported_brightness!=brightness){
        reported_brightness=brightness;
        if(api->functions->dispatch_lua_event)
            api->functions->dispatch_lua_event("AC8_GlowFixBrightness",brightness?"on":"off");
    }
    if(refresh_pending.exchange(false)&&api->functions->dispatch_lua_event)
        api->functions->dispatch_lua_event("AC8_HUD_Refresh",refresh_reason.load());

}
// Bridge observation occurs BEFORE the plugin lock. XR callbacks lock in the
// opposite direction (bridge -> plugin), so never call observed_image under it.
void observe_copy(ID3D12GraphicsCommandList* list,ID3D12Resource* dst,ID3D12Resource* src,const D3D12_RECT& area,bool full){
    acquired_images::Token token{};uint64_t frame=0;openxr_handoff::Binding binding;
    const bool observed=active.load()&&!overlay.load()&&bridge.observed_image(dst,token,frame,binding);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!enabled()||!renderer||dst==src)return;

    std::wstring name;if(!known_direct_lists.count(list)){static_cast<void>(0);return;}
    if(!destination::isolated_list(list,name)){
        static_cast<void>(0);
        return;
    }
    bool generated=false;
    if(full&&src==ui.Get()){
        if(recorded.size()>=64&&!recorded.count(list)){stop("recording_capacity_off");return;}
        // No change to the host's sharp source. Scene mode only runs generation.
        nested=true;try{generated=renderer->record(list,dst,src,overlay);if(generated){recorded.insert(list);static_cast<void>(0);state=overlay?"black_overlay_comparison_active":"waiting_current_scene_pair";}}
        catch(...){failed=true;stop("record_failed_off");}nested=false;
    }
    if(overlay||!observed||binding.device.Get()!=device.Get())return;
    if(!hud_route_scene::Renderer::supported(dst)){return;}
    
    // Every recognized scene copy must precede its final observed RT barrier.
    // Current tokens pin the actual acquired image; reset records are discarded.
    copies.erase(std::remove_if(copies.begin(),copies.end(),[&](const CopyRecord& r){return r.frame!=frame;}),copies.end());
    if(copies.size()>=64){stop("copy_capacity_off");return;}
    copies.push_back({token,dst,list,frame,0,D3D12_RESOURCE_STATE_COPY_DEST,area,full&&src==ui.Get(),generated,true});
}
void STDMETHODCALLTYPE copy(ID3D12GraphicsCommandList* list,ID3D12Resource* dst,ID3D12Resource* src){
    original_copy(list,dst,src);if(nested||!dst||!src||!active.load())return;
    const auto d=dst->GetDesc();if(d.Width>LONG_MAX||d.Height>LONG_MAX)return;
    observe_copy(list,dst,src,{0,0,static_cast<LONG>(d.Width),static_cast<LONG>(d.Height)},true);
}
void STDMETHODCALLTYPE region(ID3D12GraphicsCommandList* list,const D3D12_TEXTURE_COPY_LOCATION* dst,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION* src,const D3D12_BOX* box){
    original_region(list,dst,x,y,z,src,box);if(nested||!active.load()||!dst||!src||!dst->pResource||!src->pResource||z||dst->Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||src->Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||dst->SubresourceIndex||src->SubresourceIndex)return;
    const auto s=src->pResource->GetDesc(),d=dst->pResource->GetDesc();
    const UINT64 w=box?box->right-box->left:s.Width;const UINT64 h=box?box->bottom-box->top:s.Height;
    if(!w||!h||uint64_t(x)+w>d.Width||uint64_t(y)+h>d.Height||uint64_t(x)+w>LONG_MAX||uint64_t(y)+h>LONG_MAX||(box&&(box->front||box->back!=1)))return;
    const bool full=!x&&!y&&w==d.Width&&h==d.Height&&s.Width==d.Width&&s.Height==d.Height&&(!box||(!box->left&&!box->top));
    observe_copy(list,dst->pResource,src->pResource,{static_cast<LONG>(x),static_cast<LONG>(y),static_cast<LONG>(x+w),static_cast<LONG>(y+h)},full);
}
void STDMETHODCALLTYPE barriers(ID3D12GraphicsCommandList* list,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    original_barrier(list,count,barriers);if(nested||!barriers)return;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for(UINT i=0;i<count;++i){const auto& b=barriers[i];if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)continue;
        for(auto& r:copies)if(!r.ticket&&r.list==list&&r.token.resource==b.Transition.pResource){
            if(b.Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE||(b.Transition.Subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES&&b.Transition.Subresource!=0)||r.final_state!=b.Transition.StateBefore)r.valid=false;
            r.final_state=b.Transition.StateAfter;
        }
    }
}
void STDMETHODCALLTYPE execute(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists){
    if(nested){original_execute(queue,count,lists);return;}
    std::lock_guard<std::recursive_mutex> lock(mutex);bool uses=false;ComPtr<ID3D12Device> actual_device;
    const bool checked=queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT&&SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&actual_device)))&&actual_device.Get()==device.Get();
    for(UINT i=0;i<count&&lists;++i){uses=uses||recorded.count(lists[i])!=0;for(const auto& r:copies)uses=uses||(!r.ticket&&r.list==lists[i]);if(checked&&known_direct_lists.size()<64)known_direct_lists.insert(lists[i]);}
    if(uses&&(!checked||(fence_value&&FAILED(queue->Wait(fence.Get(),fence_value))))){failed=true;stop("queue_wait_failed_off");}
    original_execute(queue,count,lists);
    if(uses){const auto next=fence_value+1;const bool signaled=checked&&SUCCEEDED(queue->Signal(fence.Get(),next));
        for(UINT i=0;i<count&&lists;++i){recorded.erase(lists[i]);for(auto& r:copies)if(!r.ticket&&r.list==lists[i]){if(queue!=api->renderer->command_queue)r.valid=false;if(signaled){r.ticket=next;if(r.generated){generation_frame=r.frame;generation_fence=next;}}else r.valid=false;}}
        for(UINT i=0;i<count&&lists;++i)static_cast<void>(0);
        if(signaled)fence_value=next;else{failed=true;stop("queue_signal_failed_off");}
    }
}
HRESULT STDMETHODCALLTYPE reset(ID3D12GraphicsCommandList* list,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pipeline){
    const auto result=original_reset(list,allocator,pipeline);if(SUCCEEDED(result)&&!nested){static_cast<void>(0);std::lock_guard<std::recursive_mutex> lock(mutex);recorded.erase(list);copies.erase(std::remove_if(copies.begin(),copies.end(),[&](const CopyRecord& r){return !r.ticket&&r.list==list;}),copies.end());}return result;
}


bool can_hold(const acquired_images::Token& token,uint64_t frame,void*){
    std::lock_guard<std::recursive_mutex> lock(mutex);if(!enabled()||overlay)return false;
    for(const auto& r:copies)if(r.frame==frame&&r.token==token&&r.ticket&&r.valid&&r.final_state==D3D12_RESOURCE_STATE_RENDER_TARGET){return true;}return false;
}
bool coverage(const acquired_images::Token& token,uint64_t frame,const xr_read::Rect& rect,bool require_hud){
    // Capacity matches observation's existing 64-copy gate. Validate every
    // matching record before taking the full-rectangle fast path.
    std::array<LONG,130> xs{},ys{};std::array<D3D12_RECT,64> areas{};
    size_t nx=2,ny=2,na=0;bool full=false;
    const LONG left=rect.offset.x,top=rect.offset.y,right=rect.offset.x+rect.extent.width,bottom=rect.offset.y+rect.extent.height;
    xs[0]=left;xs[1]=right;ys[0]=top;ys[1]=bottom;
    for(const auto& r:copies)if(r.token==token&&r.frame==frame){
        if(!r.valid||!r.ticket||r.final_state!=D3D12_RESOURCE_STATE_RENDER_TARGET)return false;
        if(require_hud&&!r.hud)continue;
        auto a=r.area;a.left=std::max(a.left,left);a.top=std::max(a.top,top);a.right=std::min(a.right,right);a.bottom=std::min(a.bottom,bottom);
        if(a.left<a.right&&a.top<a.bottom){
            if(na==areas.size())return false;
            areas[na++]=a;xs[nx++]=a.left;xs[nx++]=a.right;ys[ny++]=a.top;ys[ny++]=a.bottom;
            full=full||(a.left==left&&a.top==top&&a.right==right&&a.bottom==bottom);
        }
    }
    if(full)return true;
    std::sort(xs.begin(),xs.begin()+nx);std::sort(ys.begin(),ys.begin()+ny);
    nx=static_cast<size_t>(std::unique(xs.begin(),xs.begin()+nx)-xs.begin());
    ny=static_cast<size_t>(std::unique(ys.begin(),ys.begin()+ny)-ys.begin());
    for(size_t x=1;x<nx;++x)for(size_t y=1;y<ny;++y){
        bool found=false;for(size_t i=0;i<na;++i){const auto& a=areas[i];if(a.left<=xs[x-1]&&a.right>=xs[x]&&a.top<=ys[y-1]&&a.bottom>=ys[y]){found=true;break;}}
        if(!found)return false;
    }
    return na!=0;
}
bool ensure_scene_resources(ID3D12Resource* scene){
    if(scene_renderer&&scene_renderer->compatible(scene))return true;
    if(fence->GetCompletedValue()<scene_consumer){state="waiting_for_scene_resize_fence";return false;}
    if(GetTickCount64()<next_resource_retry)return false;
    auto replacement=std::make_unique<hud_route_scene::Renderer>(device.Get(),assets_path,*renderer,scene);
    scene_renderer=std::move(replacement);
    return true;
}
openxr_handoff::FrameOutcome frame_sink(openxr_handoff::Session session,const xr_read::EndInfo* info,const openxr_handoff::Held* held,size_t count,uint64_t frame,ID3D12Device* actual_device,ID3D12CommandQueue* queue,void*){
    using Outcome=openxr_handoff::FrameOutcome;std::lock_guard<std::recursive_mutex> lock(mutex);
    
    if(!enabled()||overlay||actual_device!=device.Get()||!queue)return Outcome::unchanged;
    if(!hud_submission_layout::supported(info)){state="unsupported_layer_layout_skipped";static_cast<void>(0);return Outcome::unchanged;}
    const openxr_handoff::Held *hud=nullptr,*scene=nullptr;
    for(size_t i=0;i<count;++i){bool is_hud=false;for(const auto& r:copies)if(r.token==held[i].token&&r.frame==frame&&r.hud)is_hud=true;
        if(is_hud){current_geometry::Snapshot candidate{};for(size_t j=0;j<count;++j)if(i!=j&&current_geometry::read_layers(info,held[j].token.chain,held[i].token.chain,&candidate)){if(hud||scene){state="ambiguous_layers_skipped";static_cast<void>(0);return Outcome::unchanged;}hud=&held[i];scene=&held[j];}}
    }
    if(!hud||!scene){state="waiting_current_scene_pair";return Outcome::unchanged;}
    if(!hud_submission_layout::independent_quads(info,scene->token.chain,hud->token.chain)){state="shared_auxiliary_texture_skipped";static_cast<void>(0);return Outcome::unchanged;}
    if(generation_frame!=frame||!generation_fence||!recorded.empty()||!bridge.pending_exact(hud->token)||!bridge.pending_exact(scene->token)){state="current_generation_gate_skipped";static_cast<void>(0);return Outcome::unchanged;}
    const auto sd=scene->resource->GetDesc(),hd=hud->resource->GetDesc();
    current_geometry::Request request{};request.frame=frame;request.layout=1;request.scene_chain=scene->token.chain;request.hud_chain=hud->token.chain;request.scene_width=static_cast<uint32_t>(sd.Width);request.scene_height=sd.Height;request.hud_width=static_cast<uint32_t>(hd.Width);request.hud_height=hd.Height;
    current_geometry::Snapshot raw{},geometry{};if(!current_geometry::read_layers(info,request.scene_chain,request.hud_chain,&raw)){static_cast<void>(0);return Outcome::unchanged;}
    // These are presentation-local poses from THIS call, not a claim about the
    // earlier engine frame-selection hook. Both layers must share actual space.
    request.selected.valid=true;request.selected.session=reinterpret_cast<uint64_t>(session);request.selected.space=raw.space;request.selected.frame=static_cast<uint32_t>(frame);
    for(unsigned i=0;i<2;++i){request.selected.poses[i]=raw.eyes[i].pose;request.selected.fovs[i]=raw.eyes[i].fov;}
    const bool geometry_ok=current_geometry::read(session,info,request,geometry);
    const auto last_hud_flags=geometry_ok?current_geometry::hud_flags(geometry):0;
    const bool flags_ok=geometry_ok&&(last_hud_flags&2)&&!(last_hud_flags&~uint64_t(3));
    const bool hud_covered=geometry_ok&&coverage(hud->token,frame,current_geometry::hud_image(geometry).imageRect,true);
    const bool scene_covered=geometry_ok&&coverage(scene->token,frame,geometry.eyes[0].subImage.imageRect,false)&&coverage(scene->token,frame,geometry.eyes[1].subImage.imageRect,false);
    if(!geometry_ok||!flags_ok||!hud_covered||!scene_covered){
        state="geometry_state_or_copy_coverage_skipped";static_cast<void>(0);return Outcome::unchanged;}
    if(fence->GetCompletedValue()==UINT64_MAX){failed=true;stop("device_removed_off");return Outcome::unchanged;}
    bool recorded_scene=false;
    try{
        if(!ensure_scene_resources(scene->resource.Get()))return Outcome::unchanged;
        recorded_scene=scene_renderer->record(*renderer,scene->resource.Get(),hud->resource.Get(),geometry,fence.Get());
    }catch(const std::exception& e){
        next_resource_retry=GetTickCount64()+2000;state="retrying_scene_resources";
        if(api->functions->log_error)api->functions->log_error("[AC8 GlowFix 2] Scene resources will retry: %s",e.what());
    }nested=false;
    if(!recorded_scene){if(scene_renderer&&scene_renderer->busy()){state="scene_consumer_busy_skipped";}else{static_cast<void>(0);}return Outcome::unchanged;}
    if(FAILED(queue->Wait(fence.Get(),fence_value))){failed=true;stop("scene_queue_wait_failed_off");return Outcome::unchanged;}
    auto* commands=scene_renderer->commands();static_cast<void>(0);nested=true;original_execute(queue,1,&commands);nested=false;
    const auto next=fence_value+1;if(FAILED(queue->Signal(fence.Get(),next))){static_cast<void>(0);failed=true;stop("scene_queue_signal_failed_off");return Outcome::fatal;}
    static_cast<void>(0);
    fence_value=next;scene_consumer=next;scene_renderer->submitted(next);
    static_cast<void>(0);state="native_scene_per_eye_active";return Outcome::submitted;
}
// Normal resolution/fullscreen resets pause generation, retain resources and
// automatically resume after the actual submitted fence completes. Never free
// textures referenced by an unsubmitted host command list or reuse old copies.
bool reset_can_resume(bool same_device,bool healthy,bool direct_queue,uint64_t completed){
    return reset_pending&&!failed&&same_device&&healthy&&direct_queue&&recorded.empty()&&
        completed!=UINT64_MAX&&completed>=fence_value;
}
void present(){
    if(!bridge_installed&&!failed)try_portable_bridge();
    std::lock_guard<std::recursive_mutex> lock(mutex);ui=safe_ui();
    if(failed||!api||!renderer||!fence)return;
    const auto* current_device=static_cast<ID3D12Device*>(api->renderer->device);
    auto* queue=static_cast<ID3D12CommandQueue*>(api->renderer->command_queue);
    ComPtr<ID3D12Device> actual;
    if(!queue||current_device!=device.Get()||FAILED(queue->GetDevice(IID_PPV_ARGS(&actual)))||actual.Get()!=device.Get()){
        state="waiting_for_original_device";return;
    }
    const auto completed=fence->GetCompletedValue();
    if(completed==UINT64_MAX||device->GetDeviceRemovedReason()!=S_OK){failed=true;stop("device_removed_off");return;}
    if(reset_pending&&!reset_can_resume(true,true,queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT,completed)){
        state="waiting_for_reset_fence";return;
    }
    if(!ui){state="waiting_for_hud_texture";return;}
    const auto wanted=ui->GetDesc(),current=renderer->input(0)->GetDesc();
    if(!surface_formats::texture(wanted)){state="unsupported_hud_surface";return;}
    const bool change=!surface_formats::compatible(wanted,current);
    if(!reset_pending&&!change)return;
    if(!recorded.empty()||completed<fence_value){state="waiting_for_resize_fence";return;}
    if(GetTickCount64()<next_resource_retry)return;
    try{
        // Keep the old graph alive until the replacement is fully constructed.
        auto replacement=std::make_unique<hud_route_glow::Renderer>(device.Get(),assets_path,
            static_cast<UINT>(wanted.Width),wanted.Height,wanted.Format);
        scene_renderer.reset();renderer=std::move(replacement);
        copies.clear();known_direct_lists.clear();generation_frame=generation_fence=scene_consumer=0;
        reset_pending=startup_reset_pending=false;active=flight_allowed&&glow_requested;
        failure_reason="none";
        refresh_reason=change?"hud_size_changed":"device_reset_recovered";refresh_pending=true;
        state=active?"waiting_current_scene_pair":"waiting_for_flight";
    }catch(const std::exception& e){
        next_resource_retry=GetTickCount64()+2000;state="retrying_hud_resources";
        if(api->functions->log_error)api->functions->log_error("[AC8 GlowFix 2] HUD resources will retry: %s",e.what());
    }
}
void device_reset(){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(failed)return;
    reset_pending=startup_reset_pending=true;active=false;
    copies.clear();generation_frame=generation_fence=0;policy_packets=hud_policy::PacketCache{};
    state="waiting_for_reset_fence";
}
bool try_portable_bridge(){
    const auto now=GetTickCount64();if(bridge_installed||failed||now<next_bridge_retry)return bridge_installed;
    next_bridge_retry=now+1000;
    const bool connected=bridge.attach(api,{can_hold,frame_sink,nullptr});
    const auto xr=bridge.status();
    if(connected&&api->openxr&&api->openxr->get_xr_session)
        late_session_recovered=bridge.adopt_existing_session(reinterpret_cast<void*>(api->openxr->get_xr_session()),device.Get(),static_cast<ID3D12CommandQueue*>(api->renderer->command_queue));
    std::lock_guard<std::recursive_mutex> lock(mutex);
    bridge_installed=connected;
    if(!connected){
        active=false;state="waiting_for_portable_openxr";
        if(xr.disabled||xr.hook_failure!=99){failed=true;failure_reason="portable_hook_install_failed";}
    }else{
        active=flight_allowed&&glow_requested&&!reset_pending;
        state=active?"waiting_current_scene_pair":"waiting_for_flight";
    }
    return connected;
}
bool initialize(const UEVR_PluginInitializeParam* params){
    api=params;device=static_cast<ID3D12Device*>(api->renderer->device);HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&present),&module);
    wchar_t path[32768]{};if(!module||!GetModuleFileNameW(module,path,32768))return false;const auto directory=std::filesystem::path(path).parent_path();assets_path=directory/L"ac8-hud-assets";
    renderer=std::make_unique<hud_route_glow::Renderer>(device.Get(),assets_path);native_generation::check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;native_generation::check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));native_generation::check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));list->Close();
    auto** methods=*reinterpret_cast<void***>(list.Get());auto** queue_methods=*reinterpret_cast<void***>(api->renderer->command_queue);
    const int a=api->functions->register_inline_hook(methods[17],reinterpret_cast<void*>(&copy),reinterpret_cast<void**>(&original_copy));
    const int b=api->functions->register_inline_hook(queue_methods[10],reinterpret_cast<void*>(&execute),reinterpret_cast<void**>(&original_execute));
    const int c=api->functions->register_inline_hook(methods[10],reinterpret_cast<void*>(&reset),reinterpret_cast<void**>(&original_reset));
    const int d=api->functions->register_inline_hook(methods[16],reinterpret_cast<void*>(&region),reinterpret_cast<void**>(&original_region));
    const int e=api->functions->register_inline_hook(methods[26],reinterpret_cast<void*>(&barriers),reinterpret_cast<void**>(&original_barrier));
    if(a<0||b<0||c<0||d<0||e<0||!original_copy||!original_execute||!original_reset||!original_region||!original_barrier){failed=true;state="hook_failed_off";return false;}
    try_portable_bridge();
    if(!api->callbacks->on_present(present)){failed=true;state="callback_failed_off";}if(api->callbacks->on_device_reset)api->callbacks->on_device_reset(device_reset);return !failed;
}
}
extern "C"  void uevr_plugin_required_version(UEVR_PluginVersion* v){if(v)*v={UEVR_PLUGIN_VERSION_MAJOR,UEVR_PLUGIN_VERSION_MINOR,UEVR_PLUGIN_VERSION_PATCH};}
extern "C"  bool uevr_plugin_initialize(const UEVR_PluginInitializeParam* p){
    using namespace hud_renderer;
    if(!p||!p->functions||!p->functions->log_info)return false;
    api=p;
    static_cast<void>(0);
    if(p->sdk&&p->sdk->callbacks&&p->sdk->callbacks->on_pre_engine_tick)p->sdk->callbacks->on_pre_engine_tick(engine_tick);
    if(p->callbacks&&p->callbacks->on_custom_event)p->callbacks->on_custom_event(lifecycle);
    if(!p->renderer||!p->sdk||!p->sdk->stereo_hook||!p->sdk->frhitexture2d||!p->callbacks||!p->sdk->stereo_hook->get_ui_render_target||!p->sdk->frhitexture2d->get_native_resource||!p->callbacks->on_present||!p->functions->register_inline_hook||p->renderer->renderer_type!=UEVR_RENDERER_D3D12||!p->renderer->device||!p->renderer->command_queue){
        failed=true;state=failure_reason="required_d3d12_api_unavailable";static_cast<void>(0);return false;}
    if(!p->vr||!p->vr->is_openxr||!p->vr->is_openxr()){
        failed=true;state=failure_reason="openvr_not_supported";static_cast<void>(0);return false;}
    try{return initialize(p);}catch(const std::exception& e){failed=true;state=failure_reason="initialization_failed_off";if(p->functions->log_error)p->functions->log_error("[AC8 Glow] Initialization failed: %s",e.what());return false;}catch(...){failed=true;state=failure_reason="initialization_failed_off";return false;}
}
