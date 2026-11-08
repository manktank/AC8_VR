// Own textures/pipelines for the authored glow chain and widget policy mask.
// Record commands only; the host owns submission, resource-state transitions
// and fenced lifetime. Asset bytecode and constants are required runtime inputs.

#pragma once
#include <string>
#include <cstring>
#include <array>
#include <vector>
#include "native/native_generation.h"
#include <memory>
#include "hud_policy.h"
#include "prepaint-frame-policy.h"
#include "surface_formats.h"

namespace hud_route_glow {
using Microsoft::WRL::ComPtr;
// TEMP Plan C: one consumer lease owns commands/allocator and a unique upload
// decision until the existing submission fence completes. Choice is set BEFORE
// submission; rejected sources omit this list entirely and retain current input.
struct SplitLease {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> copy_list;
    ComPtr<ID3D12Resource> source,snapshot,choice;
    std::shared_ptr<void> source_lifetime; // pins immutable producer slot through consumer fence
    uint32_t* choice_data=nullptr;
    ComPtr<ID3D12Resource> mask_policy;
    hud_policy::Region* mask_data=nullptr;
    hud_policy::Snapshot fallback_policy;
    std::shared_ptr<const prepaint_frame_policy::Frame> frame_policy;
    bool submitted=false,paired=false;
    const char* selection_reason="source_selection_not_run";
    ~SplitLease(){if(choice&&choice_data)choice->Unmap(0,nullptr);if(mask_policy&&mask_data)mask_policy->Unmap(0,nullptr);}
    void select(bool allowed){if(!submitted&&choice_data){*choice_data=allowed?(paired?2u:1u):0u;
        const auto policy=allowed?prepaint_frame_policy::compose(fallback_policy,frame_policy):fallback_policy;
        if(mask_data)std::memcpy(mask_data,policy.regions.data(),sizeof(policy.regions));}}
};
class Renderer {
    ComPtr<ID3D12Device> device_;
    native_generation::Recorder native_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12RootSignature> frame_mask_root_;
    ComPtr<ID3D12PipelineState> frame_mask_pso_;
    ComPtr<ID3D12PipelineState> pso_,mask_pso_,filter_pso_,identity_filter_pso_,sharp_layer_pso_,sharp_base_pso_;
    ComPtr<ID3D12DescriptorHeap> srv_,rtv_;
    ComPtr<ID3D12Resource> preview_,sharp_,mask_,filtered_,split_sharp_,clean_filtered_,split_selected_,selected_;
    uint64_t clean_base_cpu_ticks_=0,clean_base_allocation_bytes_=0;
    bool clean_base_recorded_=false;
    hud_policy::Snapshot policy_{};
    std::vector<std::shared_ptr<SplitLease>> idle_leases_;
    unsigned used_regions_=0;
    ComPtr<ID3D12Resource> parameters_,deco_,deco_upload_;
    std::array<unsigned char,288> authored_parameters_{};
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,8> deco_footprints_{};
    std::array<UINT,8> deco_rows_{};
    std::array<UINT64,8> deco_row_bytes_{};
    static std::vector<unsigned char> bytes(const std::filesystem::path& p){
        std::ifstream f(p,std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("Missing HUD route asset");
        const auto n=f.tellg();if(n<=0||n>1024*1024)throw std::runtime_error("Invalid preview shader");
        std::vector<unsigned char> b(static_cast<size_t>(n));f.seekg(0);
        if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("Incomplete preview shader");return b;
    }
    static void transition(ID3D12GraphicsCommandList* l,ID3D12Resource* r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
        D3D12_RESOURCE_BARRIER t{};t.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        t.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};l->ResourceBarrier(1,&t);
    }
public:
    Renderer(ID3D12Device* d,const std::filesystem::path& assets,UINT source_width=2560,UINT source_height=1338,DXGI_FORMAT source_format=DXGI_FORMAT_B8G8R8A8_UNORM,bool identity_trial=false):device_(d),native_(d,assets){
        if(!surface_formats::dimensions(source_width,source_height)||!surface_formats::family(source_format))throw std::runtime_error("Unsupported HUD surface");
        idle_leases_.reserve(8);
        D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=12;
        D3D12_ROOT_PARAMETER parameters[4]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable={1,&range};parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxAnisotropy=1;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC samplers[2]{sampler,sampler};samplers[1].ShaderRegister=1;
        samplers[1].AddressU=samplers[1].AddressV=samplers[1].AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[2].Constants={3,0,8};parameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[3].Descriptor={4,0};parameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=identity_trial?4u:3u;rd.pParameters=parameters;rd.NumStaticSamplers=2;rd.pStaticSamplers=samplers;
        ComPtr<ID3DBlob> code,error;
        native_generation::check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&code,&error));
        native_generation::check(d->CreateRootSignature(0,code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&root_)));
        const auto vs=bytes(assets/L"hud-route-VS.dxbc"),ps=bytes(assets/L"hud-route-PS.dxbc");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.Get();pd.VS={vs.data(),vs.size()};pd.PS={ps.data(),ps.size()};
        pd.SampleMask=UINT_MAX;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;pd.SampleDesc.Count=1;
        pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
        pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        auto& blend=pd.BlendState.RenderTarget[0];blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;
        blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.RenderTargetWriteMask=15;
        native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso_)));
        const auto filter=bytes(assets/L"hud-filter-PS.dxbc"),mask=bytes(assets/L"hud-mask-PS.dxbc");
        pd.PS={filter.data(),filter.size()};native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&filter_pso_)));
        if(identity_trial){
            const auto selected_shader=bytes(assets/L"AC8_VR-sharp-layer-PS.dxbc"),base_shader=bytes(assets/L"AC8_VR-sharp-base-PS.dxbc");
            pd.RTVFormats[0]=surface_formats::view(source_format);
            pd.PS={selected_shader.data(),selected_shader.size()};native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&sharp_layer_pso_)));
            pd.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;
            pd.PS={base_shader.data(),base_shader.size()};native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&sharp_base_pso_)));
            const auto identity_filter=bytes(assets/L"AC8_VR-identity-filter-PS.dxbc");
            pd.PS={identity_filter.data(),identity_filter.size()};native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&identity_filter_pso_)));}
        pd.PS={mask.data(),mask.size()};pd.RTVFormats[0]=DXGI_FORMAT_R16G16B16A16_FLOAT;
        blend.BlendEnable=TRUE;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_MAX;blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ONE;
        native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&mask_pso_)));
        if(identity_trial){
            D3D12_ROOT_PARAMETER frame_parameter{};frame_parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;frame_parameter.Descriptor={3,0};frame_parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
            D3D12_ROOT_SIGNATURE_DESC frame_desc{};frame_desc.NumParameters=1;frame_desc.pParameters=&frame_parameter;
            native_generation::check(D3D12SerializeRootSignature(&frame_desc,D3D_ROOT_SIGNATURE_VERSION_1,&code,&error));
            native_generation::check(d->CreateRootSignature(0,code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&frame_mask_root_)));
            const auto frame_vs=bytes(assets/L"AC8_VR-frame-mask-VS.dxbc"),frame_ps=bytes(assets/L"AC8_VR-frame-mask-PS.dxbc");
            pd.pRootSignature=frame_mask_root_.Get();pd.VS={frame_vs.data(),frame_vs.size()};pd.PS={frame_ps.data(),frame_ps.size()};
            native_generation::check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&frame_mask_pso_)));
        }

        // Original final-shader parameters and the original decoration texture.
        const auto constants=bytes(assets/L"event-3384-PS-root-parameters.bin");
        if(constants.size()!=288)throw std::runtime_error("Final constants size changed");
        std::copy(constants.begin(),constants.end(),authored_parameters_.begin());
        D3D12_HEAP_PROPERTIES upload_heap{};upload_heap.Type=D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=512;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        native_generation::check(d->CreateCommittedResource(&upload_heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&parameters_)));
        void* mapped=nullptr;const D3D12_RANGE no_read{0,0};native_generation::check(parameters_->Map(0,&no_read,&mapped));
        std::memset(mapped,0,512);std::memcpy(mapped,constants.data(),constants.size());parameters_->Unmap(0,nullptr);
        D3D12_RESOURCE_DESC deco_desc{};deco_desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;deco_desc.Width=deco_desc.Height=128;deco_desc.DepthOrArraySize=1;deco_desc.MipLevels=8;deco_desc.Format=DXGI_FORMAT_BC7_TYPELESS;deco_desc.SampleDesc.Count=1;
        D3D12_HEAP_PROPERTIES default_heap{};default_heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        native_generation::check(d->CreateCommittedResource(&default_heap,D3D12_HEAP_FLAG_NONE,&deco_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&deco_)));
        UINT64 total=0;d->GetCopyableFootprints(&deco_desc,0,8,0,deco_footprints_.data(),deco_rows_.data(),deco_row_bytes_.data(),&total);
        const auto decoration=bytes(assets/L"menu-deco-BC7.bin");if(decoration.size()!=21872)throw std::runtime_error("Decoration texture size changed");
        buffer.Width=total;native_generation::check(d->CreateCommittedResource(&upload_heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&deco_upload_)));
        native_generation::check(deco_upload_->Map(0,&no_read,&mapped));std::memset(mapped,0,static_cast<size_t>(total));size_t offset=0;
        for(size_t i=0;i<8;++i)for(UINT row=0;row<deco_rows_[i];++row){const auto count=static_cast<size_t>(deco_row_bytes_[i]);
            if(offset+count>decoration.size())throw std::runtime_error("Decoration upload overflow");
            std::memcpy(static_cast<unsigned char*>(mapped)+deco_footprints_[i].Offset+size_t(row)*deco_footprints_[i].Footprint.RowPitch,decoration.data()+offset,count);offset+=count;}
        deco_upload_->Unmap(0,nullptr);if(offset!=decoration.size())throw std::runtime_error("Decoration upload incomplete");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=12;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        native_generation::check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv_)));
        const UINT stride=d->GetDescriptorHandleIncrementSize(hd.Type);
        D3D12_RESOURCE_DESC td=native_.output(421)->GetDesc();D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
        for(auto* resource:{std::addressof(preview_),std::addressof(filtered_)})native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(resource->ReleaseAndGetAddressOf())));
        if(identity_trial){native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&clean_filtered_)));clean_base_allocation_bytes_=d->GetResourceAllocationInfo(0,1,&td).SizeInBytes;}
        auto sharp_desc=td;sharp_desc.Width=source_width;sharp_desc.Height=source_height;sharp_desc.Format=surface_formats::storage(source_format);
        native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&sharp_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&sharp_)));
        if(identity_trial)native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&sharp_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&split_sharp_)));
        native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&sharp_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&selected_)));
        if(identity_trial)native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&sharp_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&split_selected_)));
        td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;native_generation::check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&mask_)));
        for(UINT i=0;i<12;++i){D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=i==6?DXGI_FORMAT_BC7_UNORM_SRGB:i==9?DXGI_FORMAT_R16G16B16A16_FLOAT:(i==0||i==8||i>=10)?surface_formats::view(source_format):DXGI_FORMAT_R8G8B8A8_UNORM;
            view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=i==6?8:1;
            d->CreateShaderResourceView(i==11?split_selected_.Get():input(i),&view,{srv_->GetCPUDescriptorHandleForHeapStart().ptr+size_t(i)*stride});}
        hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=5;
        native_generation::check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv_)));const UINT rtstride=d->GetDescriptorHandleIncrementSize(hd.Type);
        for(UINT i=0;i<3;++i)d->CreateRenderTargetView(i==0?preview_.Get():i==1?mask_.Get():filtered_.Get(),nullptr,{rtv_->GetCPUDescriptorHandleForHeapStart().ptr+size_t(i)*rtstride});
        D3D12_RENDER_TARGET_VIEW_DESC selected_view{};selected_view.Format=surface_formats::view(source_format);selected_view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        d->CreateRenderTargetView(selected_.Get(),&selected_view,{rtv_->GetCPUDescriptorHandleForHeapStart().ptr+size_t(4)*rtstride});
        if(clean_filtered_)d->CreateRenderTargetView(clean_filtered_.Get(),nullptr,{rtv_->GetCPUDescriptorHandleForHeapStart().ptr+size_t(3)*rtstride});

    }
    ID3D12Resource* input(unsigned i){const int ids[]={421,325,316,340,333,338,0,1088};return i==0?sharp_.Get():i==6?deco_.Get():i==8?split_sharp_.Get():i==9?mask_.Get():i==10?selected_.Get():i<8?native_.output(ids[i]):nullptr;}
    ID3D12Resource* glowInput(){return native_.output(421);}
    DXGI_FORMAT sourceView()const{return surface_formats::view(sharp_->GetDesc().Format);}
    void policy(const hud_policy::Snapshot& value){policy_=value;}
    unsigned usedRegions()const{return used_regions_;}
    bool cleanBaseRecorded()const{return clean_base_recorded_;}
    uint64_t cleanBaseCpuTicks()const{return clean_base_cpu_ticks_;}
    uint64_t cleanBaseAllocationBytes()const{return clean_base_allocation_bytes_;}
    ID3D12Resource* parameters(){return parameters_.Get();}
    const std::array<unsigned char,288>& authoredParameters()const{return authored_parameters_;}
    // Caller must have observed this lease's finite consumer fence complete.
    void recycleSplit(std::shared_ptr<SplitLease>& lease) noexcept{
        if(!lease||lease.use_count()!=1||!lease->submitted||
           lease->snapshot.Get()!=split_sharp_.Get()||
           !lease->choice_data||!lease->mask_data||idle_leases_.size()>=8)return;
        lease->copy_list.Reset();
        lease->allocator.Reset();
        lease->source.Reset();
        lease->source_lifetime.reset();
        lease->frame_policy.reset();
        lease->fallback_policy={};
        lease->submitted=false;
        lease->paired=false;
        lease->selection_reason="source_selection_not_run";
        std::memset(lease->choice_data,0,256);
        std::memset(lease->mask_data,0,sizeof(hud_policy::Region)*48);
        idle_leases_.push_back(std::move(lease));
    }
    std::shared_ptr<SplitLease> splitLease(ID3D12Resource* source=nullptr){
        if(!split_sharp_||!identity_filter_pso_)return{};
        if(!idle_leases_.empty()){
            auto lease=std::move(idle_leases_.back());
            idle_leases_.pop_back();
            if(source&&!prepareSplit(lease,source,{}))return{};
            return lease;
        }
        auto lease=std::make_shared<SplitLease>();lease->snapshot=split_sharp_;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=256;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        native_generation::check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&lease->choice)));
        void* mapped=nullptr;const D3D12_RANGE no_read{0,0};native_generation::check(lease->choice->Map(0,&no_read,&mapped));std::memset(mapped,0,256);lease->choice_data=static_cast<uint32_t*>(mapped);
        buffer.Width=sizeof(hud_policy::Region)*48;
        native_generation::check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&lease->mask_policy)));
        mapped=nullptr;native_generation::check(lease->mask_policy->Map(0,&no_read,&mapped));std::memset(mapped,0,static_cast<size_t>(buffer.Width));lease->mask_data=static_cast<hud_policy::Region*>(mapped);
        if(source&&!prepareSplit(lease,source,{}))return{};return lease;
    }
    bool prepareSplit(const std::shared_ptr<SplitLease>& lease,ID3D12Resource* source,std::shared_ptr<void> lifetime,std::shared_ptr<const prepaint_frame_policy::Frame> frame_policy={}){
        if(!lease||lease->submitted||lease->copy_list||!source||lease->snapshot.Get()!=split_sharp_.Get())return false;
        const auto expected=split_sharp_->GetDesc(),actual=source->GetDesc();auto paired_expected=expected;paired_expected.Width*=2;
        const bool pair=surface_formats::compatible(actual,paired_expected);
        if(!surface_formats::compatible(actual,expected)&&!pair)return false;lease->paired=pair;
        ComPtr<ID3D12Device> source_device;if(FAILED(source->GetDevice(IID_PPV_ARGS(&source_device)))||source_device.Get()!=device_.Get())return false;
        lease->source=source;lease->source_lifetime=std::move(lifetime);
        lease->frame_policy=std::move(frame_policy);
        native_generation::check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&lease->allocator)));
        native_generation::check(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,lease->allocator.Get(),nullptr,IID_PPV_ARGS(&lease->copy_list)));
        transition(lease->copy_list.Get(),split_sharp_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        if(!pair)lease->copy_list->CopyResource(split_sharp_.Get(),source);
        else{
            transition(lease->copy_list.Get(),split_selected_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=source;to.pResource=split_sharp_.Get();from.Type=to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            D3D12_BOX box{0,0,0,static_cast<UINT>(expected.Width),expected.Height,1};lease->copy_list->CopyTextureRegion(&to,0,0,0,&from,&box);
            box.left=box.right;box.right*=2;to.pResource=split_selected_.Get();lease->copy_list->CopyTextureRegion(&to,0,0,0,&from,&box);
            transition(lease->copy_list.Get(),split_selected_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        transition(lease->copy_list.Get(),split_sharp_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        native_generation::check(lease->copy_list->Close());return true;
    }
#ifdef AC8_IDENTITY_SPLIT_FIXTURE
    ID3D12Resource* filteredForFixture(){return filtered_.Get();}
    ID3D12Resource* maskForFixture(){return mask_.Get();}
#endif
    bool record(ID3D12GraphicsCommandList* list,ID3D12Resource* destination,ID3D12Resource* sharp,bool overlay=true,const std::shared_ptr<SplitLease>& split={}){
        clean_base_recorded_=false;clean_base_cpu_ticks_=0;
        if(!list||!destination||!sharp||destination==sharp)return false;
        const auto desc=destination->GetDesc();ComPtr<ID3D12Device> device;
        const auto owned=sharp_->GetDesc();
        if((overlay&&(desc.Width!=2560||desc.Height!=1338||surface_formats::family(desc.Format)!=1))||!surface_formats::compatible(desc,owned)||
           FAILED(destination->GetDevice(IID_PPV_ARGS(&device)))||device.Get()!=device_.Get())return false;
        const auto sourceDesc=sharp->GetDesc();ComPtr<ID3D12Device> sourceDevice;
        if(!surface_formats::compatible(sourceDesc,desc)||
            FAILED(sharp->GetDevice(IID_PPV_ARGS(&sourceDevice)))||sourceDevice.Get()!=device_.Get())return false;
        
        
        {
        // Contract inherited from the exact host UI CopyResource: sharp remains
        // COPY_SOURCE, destination remains COPY_DEST. Owned resources persist.
        transition(list,sharp_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyResource(sharp_.Get(),sharp);transition(list,sharp_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        const auto rtbase=rtv_->GetCPUDescriptorHandleForHeapStart();const UINT rtstride=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        const D3D12_CPU_DESCRIPTOR_HANDLE maskRT{rtbase.ptr+rtstride},filterRT{rtbase.ptr+2*rtstride};
        const D3D12_VIEWPORT policyViewport{0,0,2560,1338,0,1};const D3D12_RECT policyScissor{0,0,2560,1338};
        {
        transition(list,mask_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float zero[4]{};list->ClearRenderTargetView(maskRT,zero,0,nullptr);
        ID3D12DescriptorHeap* policyHeap=srv_.Get();list->SetDescriptorHeaps(1,&policyHeap);list->SetGraphicsRootSignature(root_.Get());list->SetPipelineState(mask_pso_.Get());
        list->SetGraphicsRootDescriptorTable(0,srv_->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootConstantBufferView(1,parameters_->GetGPUVirtualAddress());
        if(split)list->SetGraphicsRootConstantBufferView(3,split->choice->GetGPUVirtualAddress());
        list->RSSetViewports(1,&policyViewport);list->RSSetScissorRects(1,&policyScissor);list->OMSetRenderTargets(1,&maskRT,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        used_regions_=policy_.fresh(GetTickCount64())?policy_.count:0;
        if(split){
            split->fallback_policy=used_regions_?policy_:hud_policy::Snapshot{};split->select(split->choice_data&&*split->choice_data!=0);
            list->SetGraphicsRootSignature(frame_mask_root_.Get());list->SetPipelineState(frame_mask_pso_.Get());list->SetGraphicsRootConstantBufferView(0,split->mask_policy->GetGPUVirtualAddress());list->DrawInstanced(6,48,0,0);
            // Changing root signatures invalidates previous argument bindings.
            list->SetGraphicsRootSignature(root_.Get());list->SetGraphicsRootDescriptorTable(0,srv_->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootConstantBufferView(1,parameters_->GetGPUVirtualAddress());list->SetGraphicsRootConstantBufferView(3,split->choice->GetGPUVirtualAddress());
        }else for(unsigned i=0;i<used_regions_;++i){const auto& region=policy_.regions[i];
            const D3D12_RECT clip{std::max(0L,LONG(std::floor(region.bounds[0]*2560-1))),std::max(0L,LONG(std::floor(region.bounds[1]*1338-1))),std::min(2560L,LONG(std::ceil(region.bounds[2]*2560+1))),std::min(1338L,LONG(std::ceil(region.bounds[3]*1338+1)))};
            list->RSSetScissorRects(1,&clip);list->SetGraphicsRoot32BitConstants(2,8,&region,0);list->DrawInstanced(3,1,0,0);}
        list->RSSetScissorRects(1,&policyScissor);
        transition(list,mask_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        {
        transition(list,filtered_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->SetPipelineState(split?identity_filter_pso_.Get():filter_pso_.Get());list->OMSetRenderTargets(1,&filterRT,FALSE,nullptr);list->DrawInstanced(3,1,0,0);
        transition(list,filtered_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
        }
        const bool generated=native_.record(list,filtered_.Get());
        transition(list,filtered_.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if(!generated)return false;
        if(split){
            // In paired mode the common emissive base contains retained UI
            // only. Selected sharp pixels composite afterward in their own image.
            // Single-source fixtures/fallback preserve the legacy full base.
            LARGE_INTEGER started{},finished{};QueryPerformanceCounter(&started);
            const D3D12_CPU_DESCRIPTOR_HANDLE cleanRT{rtbase.ptr+3*rtstride};
            transition(list,clean_filtered_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
            ID3D12DescriptorHeap* cleanHeap=srv_.Get();list->SetDescriptorHeaps(1,&cleanHeap);list->SetGraphicsRootSignature(root_.Get());list->SetPipelineState(sharp_base_pso_.Get());
            list->SetGraphicsRootConstantBufferView(3,split->choice->GetGPUVirtualAddress());
            list->SetGraphicsRootDescriptorTable(0,srv_->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootConstantBufferView(1,parameters_->GetGPUVirtualAddress());
            list->RSSetViewports(1,&policyViewport);list->RSSetScissorRects(1,&policyScissor);list->OMSetRenderTargets(1,&cleanRT,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
            transition(list,clean_filtered_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
            const bool base_generated=native_.record_clean_base(list,clean_filtered_.Get());
            transition(list,clean_filtered_.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            QueryPerformanceCounter(&finished);clean_base_cpu_ticks_=static_cast<uint64_t>(finished.QuadPart-started.QuadPart);
            if(!base_generated)return false;clean_base_recorded_=true;
        }
        {
            const D3D12_CPU_DESCRIPTOR_HANDLE sharpRT{rtbase.ptr+4*rtstride};
            transition(list,selected_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
            if(!split){const float zero[4]{};list->ClearRenderTargetView(sharpRT,zero,0,nullptr);}
            if(split){ID3D12DescriptorHeap* heap=srv_.Get();list->SetDescriptorHeaps(1,&heap);list->SetGraphicsRootSignature(root_.Get());list->SetPipelineState(sharp_layer_pso_.Get());
                list->SetGraphicsRootDescriptorTable(0,srv_->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootConstantBufferView(3,split->choice->GetGPUVirtualAddress());
                const D3D12_VIEWPORT viewport{0,0,float(owned.Width),float(owned.Height),0,1};const D3D12_RECT scissor{0,0,LONG(owned.Width),LONG(owned.Height)};
                list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&sharpRT,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);}
            transition(list,selected_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        // Re-record upload each time so a cancelled/reset list never leaves a
        // CPU-only 'initialized' flag claiming GPU initialization happened.
        {
        transition(list,deco_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        for(UINT i=0;i<8;++i){D3D12_TEXTURE_COPY_LOCATION source{},target{};source.pResource=deco_upload_.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=deco_footprints_[i];
            target.pResource=deco_.Get();target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;target.SubresourceIndex=i;list->CopyTextureRegion(&target,0,0,0,&source,nullptr);}
        transition(list,deco_.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if(!overlay)return true;
        
        transition(list,preview_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        ID3D12DescriptorHeap* heap=srv_.Get();list->SetDescriptorHeaps(1,&heap);list->SetPipelineState(pso_.Get());list->SetGraphicsRootSignature(root_.Get());
        list->SetGraphicsRootDescriptorTable(0,srv_->GetGPUDescriptorHandleForHeapStart());
        list->SetGraphicsRootConstantBufferView(1,parameters_->GetGPUVirtualAddress());
        const D3D12_VIEWPORT viewport{0,0,2560,1338,0,1};const D3D12_RECT scissor{0,0,2560,1338};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);const auto rtv=rtv_->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
        transition(list,preview_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyResource(destination,preview_.Get());
        transition(list,preview_.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);return true;
    }
};
}
