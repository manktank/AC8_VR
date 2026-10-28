// Copy-compatible storage and explicit float views for real HUD/eye surfaces.
// Keep the authored glow intermediates at their original format and size.
#pragma once
#include <d3d12.h>
namespace surface_formats {
inline unsigned family(DXGI_FORMAT f) {
    switch(f) {
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return 1;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:return 2;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:case DXGI_FORMAT_R10G10B10A2_UNORM:return 3;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:case DXGI_FORMAT_R16G16B16A16_FLOAT:return 4;
    default:return 0;
    }
}
inline DXGI_FORMAT storage(DXGI_FORMAT f) {
    const DXGI_FORMAT values[]={DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_B8G8R8A8_TYPELESS,
        DXGI_FORMAT_R8G8B8A8_TYPELESS,DXGI_FORMAT_R10G10B10A2_TYPELESS,DXGI_FORMAT_R16G16B16A16_TYPELESS};
    return values[family(f)];
}
// Preserve the existing encoded-color behavior for sRGB swapchains. Their XR
// interpretation is unchanged; an sRGB RTV here would encode the color twice.
inline DXGI_FORMAT view(DXGI_FORMAT f) {
    const DXGI_FORMAT values[]={DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
    return values[family(f)];
}
inline bool dimensions(UINT64 w,UINT h) {
    return w&&h&&w<=D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION&&h<=D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}
inline bool texture(const D3D12_RESOURCE_DESC& d) {
    return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&dimensions(d.Width,d.Height)&&
        d.DepthOrArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&family(d.Format);
}
inline bool render_target(const D3D12_RESOURCE_DESC& d) {
    return texture(d)&&(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)&&!(d.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
}
inline bool compatible(const D3D12_RESOURCE_DESC& a,const D3D12_RESOURCE_DESC& b) {
    return texture(a)&&texture(b)&&a.Width==b.Width&&a.Height==b.Height&&family(a.Format)==family(b.Format);
}
}
