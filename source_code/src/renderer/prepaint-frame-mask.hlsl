// Same mask coverage and MAX blend as hud_policy_pass.hlsl. A lease-owned
// constant buffer permits selection of the *matched producer frame* immediately
// before submission, after the consumer command list has already been recorded.
// One instanced quad draw replaces the existing per-region mask draws.
struct Region {float4 bounds;float4 flags;};
cbuffer FrameRegions : register(b3) {Region regions[48];};
struct Vertex {float4 pos:SV_Position;float2 uv:TEXCOORD0;nointerpolation float4 bounds:TEXCOORD1;nointerpolation float4 flags:TEXCOORD2;};
Vertex FrameMaskVS(uint id:SV_VertexID,uint instance:SV_InstanceID){
    const float2 corners[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    const Region r=regions[instance];Vertex v;
    float2 uv=lerp(r.bounds.xy-1.0/float2(2560,1338),r.bounds.zw+1.0/float2(2560,1338),corners[id]);
    if(!any(r.flags))uv=0; // inactive instances are degenerate, no fragments
    v.uv=uv;v.pos=float4(uv.x*2-1,1-uv.y*2,0,1);v.bounds=r.bounds;v.flags=r.flags;return v;
}
float4 FrameMaskPS(Vertex i):SV_Target{
    float2 edge=min(i.uv-i.bounds.xy,i.bounds.zw-i.uv)*float2(2560,1338);
    return i.flags*saturate(min(edge.x,edge.y)+0.5);
}
