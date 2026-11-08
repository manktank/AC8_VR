// Final HUD blend into the current eye image. InverseMap uses current XR layer
// geometry. Full-screen black fade detection and dark-panel coverage are retained;
// there is no broad cyan-pixel recoloring of the flattened scene.

// Same native blending and assets as the proven overlay; real background is
// sampled in destination coordinates. HUD coordinates come from current XR layers.
#include "hud_route_preview.hlsl"
Texture2D<float4> SceneTexture : register(t8);
#include "hud-route-assets/scene_mapping.hlsli"
cbuffer SourceMapping : register(b2) { float4 SourceScaleBias; float4 PanelBounds; };
struct SceneVertex {float4 position:SV_Position;float2 uv:TEXCOORD0;nointerpolation float fade:TEXCOORD1;};
float BlackFadeCoverage(float4 value){
 float peak=max(value.r,max(value.g,value.b));
 return peak<=2.0/255.0 ? saturate(value.a) : 0;
}
SceneVertex SceneVS(uint id:SV_VertexID){
 Vertex base=VS(id);SceneVertex result;result.position=base.position;result.uv=base.uv;
 const float2 points[8]={float2(.03,.03),float2(.97,.03),float2(.03,.97),float2(.97,.97),float2(.16,.15),float2(.84,.15),float2(.16,.85),float2(.84,.85)};
 float coverage=1;
 [unroll]for(int p=0;p<8;++p)coverage=min(coverage,BlackFadeCoverage(Sharp.SampleLevel(BilinearClampSampler,points[p],0)));
 result.fade=coverage;return result;
}
float4 ScenePS(SceneVertex i) : SV_Target {
    float4 scene=SceneTexture.Load(int3(int2(i.position.xy),0));
    float4 map=InverseMap(i.position);
    if(map.z<0.5)return float4(scene.rgb*(1-i.fade),scene.a);
    float2 hudUV=map.xy*SourceScaleBias.xy+SourceScaleBias.zw;
    const float3 luminance=float3(0.298912,0.586611,0.114477);
    float2 uv=hudUV*ViewPortScaler.xy;
    float4 policy=PolicyMask.Sample(BilinearClampSampler,hudUV);
    float4 original=Sharp.Sample(BilinearClampSampler,hudUV);
    float4 combine=CombineColorTexture.Sample(BilinearClampSampler,uv);
    // Only accepted radar geometry selects this special native blend.
    bool inside=policy.b>=0.5; // owned radar geometry selects native alpha branch
    bool radio=false; // no subtitle layout rectangle
    bool alphaBlend=SceneBlendPattern>0||inside;
    bool menu=SceneBlendPattern==3,replay=SceneBlendPattern==2;
    float rate=clamp(1-dot(scene.rgb,luminance),0.2,1);
    // Preserve the accepted retained-HUD light response. Selected sharp
    // coverage is applied after this scene/glow blend without region switches.
    if(alphaBlend)rate=1;
    float4 first=FirstGlowTexture.Sample(BilinearClampSampler,uv)*GlowIntensity*rate;
    first.a=saturate(dot(first.rgb,luminance));
    float4 base=float4(combine.rgb+first.rgb,combine.a+first.a*(1-combine.a));
    float4 secondary=SecondaryGlowTexture.Sample(BilinearClampSampler,uv)*SecondaryGlowIntensity*rate;
    secondary.a=dot(secondary.rgb,luminance);
    float4 additive=AdditionalGlowTexture.Sample(BilinearClampSampler,uv)*rate;
    float3 secondaryRGB=DecoratedSecondary(secondary,uv);
    if(replay)secondaryRGB+=additive.rgb;
    float4 empty=float4(0,0,0,0);
    float4 core=empty*rate,depth=empty*rate,normal=empty*rate*BlendOpacity;
    float4 color=0;
    if((alphaBlend||radio)&&!menu){
        float4 background=float4(scene.rgb,1);if(!inside)background=ScreenBlend(background,core);
        color=background*(1-base.a)+base;
        color.rgb=color.rgb*(1-saturate(dot(secondaryRGB,luminance)))+secondaryRGB;
    }else{
        float neighborhood=GetIntentsityAdjustedByNeighborhood(CombineColorTexture,BilinearClampSampler,uv,float2(1.0/2560,1.0/1440),luminance);
        color=BlendGlowToScene(SoftLightTexture.Sample(BilinearClampSampler,uv),depth,additive,core,base,float4(scene.rgb,1),neighborhood,menu);
        color=color*(1-normal.a)+normal;color.rgb+=secondaryRGB;
    }
    // Sole rectangle policy: remove all generated glow inside the current
    // portrait bounds AFTER blur; composite stored premultiplied sharp UI onto
    // the unmodified eye background. Portrait remains in the glow source.
    // Radar already used its native alpha branch, retaining decorated glow.
    // Only the accepted portrait receives this final sharp-only override.
    float4 noGlow=SharpNoGlowLayer.Sample(BilinearClampSampler,hudUV);
    color.rgb=color.rgb*(1-noGlow.a)+noGlow.rgb;
    color.rgb=lerp(color.rgb,original.rgb+scene.rgb*(1-original.a),policy.g);
    // UNORM targets clamp automatically. Float eye surfaces retain scene HDR
    // headroom instead of clipping the entire eye to SDR during HUD blending.
    return float4(max(color.rgb,0),scene.a);
}
