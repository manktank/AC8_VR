// Rasterize portrait output cutout and radar alpha-branch selection.
// Neither channel filters glow input; native draw identity does that.

#include "hud_route_preview.hlsl"
cbuffer RegionParameters : register(b3) {float4 RegionBounds;float4 RegionFlags;};
float4 MaskPS(Vertex i) : SV_Target {
    float2 edge=min(i.uv-RegionBounds.xy,RegionBounds.zw-i.uv)*float2(2560,1338);
    // One source pixel of antialiasing at most. No brightness threshold.
    return RegionFlags*saturate(min(edge.x,edge.y)+0.5);
}
// Rectangles never withhold input. Identity selection happens in native draws.
float4 PrefilterPS(Vertex i) : SV_Target {
    return Sharp.Sample(BilinearClampSampler,i.uv);
}
