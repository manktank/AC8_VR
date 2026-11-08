#pragma once
// TEMP Plan C: one normally constructed, rooted private render target per run.
// No existing game target/property is repurposed. Retain this test target through
// toggles/reset/failure until process exit so queued native work cannot outlive it.
#include "temporary-radio-paint-marker.h"
#include "surface_formats.h"
#include <wrl/client.h>
#include "temporary-glow-target-guards.h"
namespace selective_glow_target {
using Object=UEVR_UObjectHandle;
using Construct=Object(*)(void*);
using Initialize=void(*)(Object);
using Update=void(*)(Object,bool);
using Resource=uintptr_t(*)(Object);
inline Construct construct=nullptr;
inline Initialize initialize_target=nullptr;
inline Update update_target=nullptr;
inline Resource target_resource=nullptr;
inline const UEVR_PluginInitializeParam* api=nullptr;
inline bool guards_ready=false,attempted=false;
inline const char* status="not_initialized";
struct alignas(16) Parameters {std::array<unsigned char,0x90> bytes{};};
template<class T>inline void set(Parameters& p,size_t at,T value){std::memcpy(p.bytes.data()+at,&value,sizeof(value));}
inline Parameters parameters(Object cls,Object outer){
    Parameters p;set(p,0,cls);set(p,8,outer);
    // Exact native register_object1aaef50 converts RF_MarkAsRootSet0x80 into
    // internal RootSet bit30 while registering the object, clearing flag0x80.
    // RF_Transient0x40 prevents persistence. Engine creates the identity/root.
    set(p,0x18,uint32_t{0xc0});return p;
}
struct Owner {
    std::shared_ptr<Owner> sharp; // complementary selected sharp image, same production
    radio_paint_marker::Leaf identity;
    Object klass=nullptr,outer=nullptr;
    uintptr_t resource=0,rhi=0;
    Microsoft::WRL::ComPtr<ID3D12Resource> native;
    UINT width=0,height=0;
    bool initialized=false,published=false;
};
inline std::shared_ptr<Owner> retained,retained_sharp;
inline bool attempted_sharp=false;
inline std::shared_ptr<Owner> published_owner;
inline std::shared_ptr<Owner> get(){return std::atomic_load(&published_owner);}
inline bool valid(const Owner& o){
    using selective_glow_inventory::read;
    std::array<unsigned char,40> header{};uintptr_t object=0;int32_t serial=0;uint32_t flags=0;
    return o.identity.object&&o.identity.serial>0&&read(o.identity.object,header)&&header==o.identity.header&&
        radio_paint_marker::pointer_field(o.identity.registry_slot,object)&&object==reinterpret_cast<uintptr_t>(o.identity.object)&&
        read(reinterpret_cast<void*>(o.identity.registry_slot+16),serial)&&serial==o.identity.serial&&
        read(reinterpret_cast<void*>(o.identity.registry_slot+8),flags)&&(flags&0x40000000u)!=0&&(flags&0x30000000u)==0;
}
inline bool bind_identity(Owner& o){
    using selective_glow_inventory::read;
    auto& leaf=o.identity;int32_t index=-1,count=0;uintptr_t chunks=0,chunk=0,table=0,klass=0,outer=0;
    if(!selective_glow_inventory::live(api,leaf.object)||!read(leaf.object,leaf.header)||
       !read(reinterpret_cast<const unsigned char*>(leaf.object)+12,index)||index<0||
       !radio_paint_marker::pointer_field(reinterpret_cast<uintptr_t>(leaf.object),table)||table!=radio_paint_marker::image_base+ac8_game_compatibility::rva<0xbea17e0>()||
       !radio_paint_marker::pointer_field(reinterpret_cast<uintptr_t>(leaf.object)+16,klass)||klass!=reinterpret_cast<uintptr_t>(o.klass)||
       !radio_paint_marker::pointer_field(reinterpret_cast<uintptr_t>(leaf.object)+32,outer)||outer!=reinterpret_cast<uintptr_t>(o.outer)||
       !radio_paint_marker::pointer_field(radio_paint_marker::image_base+ac8_game_compatibility::rva<0xdc60ce0>(),chunks)||
       !read(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::rva<0xdc60cf4>()),count)||count>1000000||index>=count||
       !radio_paint_marker::pointer_field(chunks+static_cast<uintptr_t>(index/65536)*8,chunk))return false;
    leaf.registry_slot=chunk+static_cast<uintptr_t>(index%65536)*24;
    return radio_paint_marker::ensure_serial(leaf,index)&&valid(o);
}
inline bool property(Owner& o,const wchar_t* name,int offset){
    if(!api->sdk->ustruct||!api->sdk->fproperty||!api->sdk->ustruct->find_property||!api->sdk->fproperty->get_offset)return false;
    const auto p=api->sdk->ustruct->find_property(reinterpret_cast<UEVR_UStructHandle>(o.klass),name);
    return p&&api->sdk->fproperty->get_offset(p)==offset&&
        api->sdk->uobject->get_property_data(o.identity.object,name)==reinterpret_cast<unsigned char*>(o.identity.object)+offset;
}
inline bool configure(Owner& o){
    if(!valid(o)||!property(o,L"SizeX",0x140)||!property(o,L"SizeY",0x144)||!property(o,L"ClearColor",0x148)||
       !property(o,L"RenderTargetFormat",0x15b)||!property(o,L"bAutoGenerateMips",0x15c)||!api->sdk->uobject->set_bool_property)return false;
    auto* bytes=reinterpret_cast<unsigned char*>(o.identity.object);const int32_t width=static_cast<int32_t>(o.width),height=static_cast<int32_t>(o.height);
    const std::array<float,4> transparent{};const uint8_t rgba8=2;
    std::memcpy(bytes+0x140,&width,4);std::memcpy(bytes+0x144,&height,4);std::memcpy(bytes+0x148,transparent.data(),16);std::memcpy(bytes+0x15b,&rgba8,1);
    api->sdk->uobject->set_bool_property(o.identity.object,L"bAutoGenerateMips",false);
    return valid(o);
}
inline bool create_single(UINT width,UINT height,std::shared_ptr<Owner>& storage,bool& tried){
    if(storage){const bool same=valid(*storage)&&storage->width==width&&storage->height==height;status=same?"private_target_storage":"target_extent_or_lifetime_changed";return same;}
    if(tried||!guards_ready||!surface_formats::dimensions(width,height)){status="target_creation_not_admitted";return false;}
    tried=true;
    auto o=std::make_shared<Owner>();o->width=width;o->height=height;
    o->klass=api->sdk->uobject_array->find_uobject(L"Class /Script/Engine.TextureRenderTarget2D");
    o->outer=api->sdk->uobject_array->find_uobject(L"Package /Engine/Transient");
    if(!selective_glow_inventory::live(api,o->klass)||!selective_glow_inventory::live(api,o->outer)){status="target_class_or_transient_package_unavailable";return false;}
    auto params=parameters(o->klass,o->outer);o->identity.object=construct(params.bytes.data());
    // Keep the rooted result even if a subsequent guard refuses initialization.
    // No teardown/free can race native initialization or queued resource work.
    storage=o;
    if(!bind_identity(*o)||!configure(*o)){status="new_target_identity_or_property_guard_failed";return false;}
    initialize_target(o->identity.object);update_target(o->identity.object,true);o->initialized=true;status="waiting_private_target_resource";return true;
}
inline bool create(UINT width,UINT height,bool paired=false){
    if(!create_single(width,height,retained,attempted))return false;
    if(paired){if(!create_single(width,height,retained_sharp,attempted_sharp))return false;retained->sharp=retained_sharp;}
    return true;
}
__declspec(noinline) inline ID3D12Resource* native_resource(uintptr_t rhi){
    __try{return static_cast<ID3D12Resource*>(api->sdk->frhitexture2d->get_native_resource(reinterpret_cast<UEVR_FRHITexture2DHandle>(rhi)));}
    __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
inline bool publish_single(const std::shared_ptr<Owner>& o,uintptr_t ui_rhi,ID3D12Device* expected_device){
    using selective_glow_inventory::read;
    if(!o||!o->initialized||!valid(*o)){status="private_target_not_live";return false;}
    if(o->published)return true;
    const auto resource=target_resource(o->identity.object);uintptr_t table=0,interface_table=0,deferred_table=0,owner=0,rhi=0,rhi_table=0,ui_table=0;
    const auto base=radio_paint_marker::image_base;
    if(!resource||!radio_paint_marker::pointer_field(resource,table)||table!=base+ac8_game_compatibility::rva<0xbea1670>()||
       !radio_paint_marker::pointer_field(resource+0x50,interface_table)||interface_table!=base+ac8_game_compatibility::rva<0xbea1758>()||
       !radio_paint_marker::pointer_field(resource+0x60,deferred_table)||deferred_table!=base+ac8_game_compatibility::rva<0xbea17d8>()||
       !radio_paint_marker::pointer_field(resource+0x90,owner)||owner!=reinterpret_cast<uintptr_t>(o->identity.object)||
       !radio_paint_marker::pointer_field(resource+0x58,rhi)||!rhi||
       !radio_paint_marker::pointer_field(rhi,rhi_table)||!radio_paint_marker::pointer_field(ui_rhi,ui_table)||rhi_table!=ui_table){status="waiting_private_target_resource";return false;}
    auto* native=native_resource(rhi);if(!native){status="private_target_native_resource_unavailable";return false;}
    const auto desc=native->GetDesc();Microsoft::WRL::ComPtr<ID3D12Device> device;
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=o->width||desc.Height!=o->height||desc.DepthOrArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1||
       surface_formats::family(desc.Format)!=1||FAILED(native->GetDevice(IID_PPV_ARGS(&device)))||device.Get()!=expected_device){status="private_target_format_extent_or_device_refused";return false;}
    o->native=native;o->resource=resource;o->rhi=rhi;o->published=true;status="private_target_ready";return true;
}
inline bool publish(uintptr_t ui_rhi,ID3D12Device* device){
    auto o=retained;if(!publish_single(o,ui_rhi,device))return false;
    if(o->sharp&&!publish_single(o->sharp,ui_rhi,device))return false;
    std::atomic_store(&published_owner,o);return true;
}

template<size_t N>inline bool guard(uintptr_t rva,const std::array<unsigned char,N>& expected){std::array<unsigned char,N> actual{};return ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::translate(rva)),actual,expected)&&actual==expected;}
inline void initialize(const UEVR_PluginInitializeParam* host){
    api=host;using namespace selective_glow_target_guards;
    if(!radio_paint_marker::ready||!api||!api->sdk||!api->sdk->frhitexture2d||!api->sdk->frhitexture2d->get_native_resource){status="target_api_unavailable";return;}
    if(!guard(construct_rva,selective_glow_target_guards::construct)||!guard(register_object_rva,register_object)||
       !guard(target_initialize_rva,target_initialize)||!guard(target_update_rva,selective_glow_target_guards::target_update)||!guard(target_resource_rva,selective_glow_target_guards::target_resource)){
        status="target_native_guard_failed";return;
    }
    const auto base=radio_paint_marker::image_base;
    construct=reinterpret_cast<Construct>(base+ac8_game_compatibility::rva<construct_rva>());initialize_target=reinterpret_cast<Initialize>(base+ac8_game_compatibility::rva<target_initialize_rva>());
    update_target=reinterpret_cast<Update>(base+ac8_game_compatibility::rva<target_update_rva>());target_resource=reinterpret_cast<Resource>(base+ac8_game_compatibility::rva<target_resource_rva>());
    guards_ready=true;status="target_guards_ready";
}
}
