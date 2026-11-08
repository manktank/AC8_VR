// Native widget identity split. No rectangle or pixel filter selects glow input.
#include "hud_route_preview.hlsl"
Texture2D<float4> IdentityGlowInput : register(t8);
Texture2D<float4> NativeSharpLayer : register(t11);
cbuffer SubmissionChoice : register(b4) {uint IdentitySourceAdmitted;};
float4 IdentityFilterPS(Vertex i) : SV_Target {
    [branch] if(IdentitySourceAdmitted!=0)return IdentityGlowInput.Sample(BilinearClampSampler,i.uv);
    // An unproved/mismatched producer uses the accepted current-renderer path.
    return Sharp.Sample(BilinearClampSampler,i.uv);
}

// Selected sharp pixels bypass every glow kernel and preserve native alpha.
float4 SharpLayerPS(Vertex i):SV_Target{
    [branch] if(IdentitySourceAdmitted==2)return NativeSharpLayer.Sample(BilinearClampSampler,i.uv);
    return 0;
}
// Dual-layer mode keeps selected sharp content out of the common emissive base.
// Legacy single-source fixtures and fallback retain their complete sharp base.
float4 SharpBasePS(Vertex i):SV_Target{
    [branch] if(IdentitySourceAdmitted==2)return IdentityGlowInput.Sample(BilinearClampSampler,i.uv);
    return Sharp.Sample(BilinearClampSampler,i.uv);
}
