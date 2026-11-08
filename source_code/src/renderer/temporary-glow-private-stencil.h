#pragma once
// One independently owned engine depth/stencil attachment. Never release or
// initialize through a copied game reference; retain ownership through shutdown.
namespace selective_glow_private_stencil {
using Initialize=void(*)(void*);
inline Initialize allocate=nullptr;
inline bool ready=false,attempted=false;
inline uintptr_t rhi=0;
inline UINT width=0,height=0;
inline Microsoft::WRL::ComPtr<ID3D12Device> owner_device;
inline Microsoft::WRL::ComPtr<ID3D12Resource> native;
inline const char* status="not_initialized";
inline std::array<unsigned char,256> storage{};
inline bool bind(std::array<unsigned char,256>& context,UINT w,UINT h,ID3D12Device* expected){
    if(!ready||!allocate||!w||!h||!expected)return false;
    if(!attempted){
        attempted=true;width=w;height=h;storage.fill(0);
        std::memcpy(storage.data()+0xc8,&w,4);std::memcpy(storage.data()+0xcc,&h,4);storage[0xd8]=1;
        // Native allocator reads +c8/+cc/+d8 and transfers its owning RHI ref to
        // +98. Its initial release sees null, never the game's attachment.
        allocate(storage.data());std::memcpy(&rhi,storage.data()+0x98,8);
        auto* resource=rhi?selective_glow_target::native_resource(rhi):nullptr;
        if(!resource){status="private_stencil_allocation_failed";return false;}
        const auto desc=resource->GetDesc();Microsoft::WRL::ComPtr<ID3D12Device> device;
        if(FAILED(resource->GetDevice(IID_PPV_ARGS(&device)))||device.Get()!=expected||desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=w||desc.Height!=h||desc.DepthOrArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1||!(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)){
            status="private_stencil_resource_refused";return false;
        }
        native=resource;owner_device=device;status="private_stencil_ready";
    }
    if(!native||w!=width||h!=height||expected!=owner_device.Get())return false;
    uintptr_t current=0;std::memcpy(&current,storage.data()+0x98,8);if(current!=rhi)return false;
    std::memcpy(context.data()+0x98,&rhi,8);context[0xd8]=1;return true;
}
inline void initialize(){
    // The reviewed native caller independently locates the E9 alias and the
    // complete allocator body. Validate that relation, then call the verified
    // body directly. A five-byte jump alone cannot uniquely identify a routine.
    constexpr std::array<unsigned char,26> head{{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x57,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0x00,0x01,0x00,0x00}};
    ready=selective_glow_target::guards_ready&&selective_glow_retained_clips::layout_ready&&selective_glow_target::guard(selective_glow_clip_guards::stencil_reset_rva,selective_glow_clip_guards::stencil_reset)&&selective_glow_target::guard(selective_glow_clip_guards::stencil_quad_rva,selective_glow_clip_guards::stencil_quad)&&ac8_game_compatibility::tail_alias(radio_paint_marker::image_base,0x37e79a0,0x11e7b8a0)&&selective_glow_target::guard(0x11e7b8a0,head);
    if(ready)allocate=reinterpret_cast<Initialize>(radio_paint_marker::image_base+ac8_game_compatibility::rva<0x11e7b8a0>());
    status=ready?"private_stencil_guards_ready":"private_stencil_guard_refused";
}
}
