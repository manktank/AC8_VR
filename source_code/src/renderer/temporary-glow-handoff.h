#pragma once
// TEMP Plan C. Produce immutable copies on the actual producer queue. VR waits
// for those copies; the producer NEVER waits for a VR consumer. Pool reuse needs
// producer completion AND release of every consumer reference. Caller serializes
// submit/prepare/wait using the renderer lock. No CPU fence waits in this code.
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <memory>
#include <mutex>
#include <ostream>
#include "surface_formats.h"
#include "prepaint-frame-policy.h"
namespace selective_glow_handoff {
using Microsoft::WRL::ComPtr;
#ifdef AC8_GLOW_HANDOFF_FIXTURE
inline bool fail_signal=false,fail_wait=false;
#endif
inline HRESULT signal_queue(ID3D12CommandQueue* queue,ID3D12Fence* fence,uint64_t value){
#ifdef AC8_GLOW_HANDOFF_FIXTURE
    if(fail_signal)return E_FAIL;
#endif
    return queue->Signal(fence,value);
}
inline HRESULT wait_queue(ID3D12CommandQueue* queue,ID3D12Fence* fence,uint64_t value){
#ifdef AC8_GLOW_HANDOFF_FIXTURE
    if(fail_wait)return E_FAIL;
#endif
    return queue->Wait(fence,value);
}
inline ComPtr<IUnknown> identity(IUnknown* object){ComPtr<IUnknown> result;if(object)object->QueryInterface(IID_PPV_ARGS(&result));return result;}
struct Snapshot {
    std::shared_ptr<const prepaint_frame_policy::Frame> policy;
    ComPtr<ID3D12Resource> image,source,sharp_source;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    uint64_t ticket=0,revision=0,epoch=0,serial=0,sharp_serial=0,allocation_bytes=0;
    bool poisoned=false;
};
struct Key {uint64_t revision=0,epoch=0,serial=0,sharp_serial=0;};
class Handoff {
    mutable std::mutex mutex_;
    ComPtr<ID3D12Device> device_;
    ComPtr<IUnknown> device_identity_,producer_identity_;
    ComPtr<ID3D12CommandQueue> producer_;
    ComPtr<ID3D12Fence> fence_;
    std::array<std::shared_ptr<Snapshot>,8> pool_{};
    std::shared_ptr<Snapshot> latest_;
    uint64_t ticket_=0,signals_=0,snapshots_=0,waits_=0,misses_=0,reuses_=0,queue_changes_=0;
    uintptr_t expected_raw_=0,expected_identity_=0,last_consumer_=0;
    bool failed_=false;
    const char* reason_="waiting_producer_queue";
    void fail(const char* reason){failed_=true;reason_=reason;latest_.reset();}
    bool same_device(ID3D12CommandQueue* queue,ID3D12Device* expected){
        ComPtr<ID3D12Device> actual;
        if(!queue||!expected||expected->GetNodeCount()!=1)return false; // cross-node visibility needs a separate proof
        const auto desc=queue->GetDesc();
        if(desc.Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||desc.NodeMask>1||FAILED(queue->GetDevice(IID_PPV_ARGS(&actual))))return false;
        auto a=identity(actual.Get()),b=identity(expected);return a&&b&&a.Get()==b.Get();
    }
    static void transition(ID3D12GraphicsCommandList* list,ID3D12Resource* image,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
        D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition={image,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};list->ResourceBarrier(1,&b);
    }
public:
    bool distinct_producer(ID3D12CommandQueue* queue)const{
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_||!producer_identity_||!queue)return false;
        const auto canonical=identity(queue);
        return canonical&&canonical.Get()==producer_identity_.Get()&&reinterpret_cast<uintptr_t>(canonical.Get())!=expected_identity_;
    }
    bool accept_producer(ID3D12CommandQueue* queue,ID3D12Device* expected,ID3D12CommandQueue* renderer_queue){
        std::lock_guard<std::mutex> lock(mutex_);
        if(!same_device(queue,expected)){fail("snapshot_producer_device_or_type_refused");return false;}
        auto canonical=identity(queue);if(!canonical){fail("snapshot_producer_identity_refused");return false;}
        if(producer_identity_&&canonical.Get()!=producer_identity_.Get()){
            // Even on a refusal, an already recorded private write must follow
            // the last source read. This wait is solely producer -> producer;
            // it never references a consumer fence or an unsignaled value.
            ++queue_changes_;
            if(ticket_&&FAILED(wait_queue(queue,fence_.Get(),ticket_)))fail("snapshot_changed_producer_cleanup_wait_failed");
            else fail("snapshot_producer_queue_changed");
            return false;
        }
        if(failed_)return false;
        if(!producer_){
            if(FAILED(expected->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)))){fail("snapshot_fence_creation_failed");return false;}
            producer_=queue;producer_identity_=canonical;device_=expected;device_identity_=identity(expected);
            expected_raw_=reinterpret_cast<uintptr_t>(renderer_queue);auto expected_id=identity(renderer_queue);expected_identity_=reinterpret_cast<uintptr_t>(expected_id.Get());
            reason_="producer_queue_pinned";
        }else if(identity(expected).Get()!=device_identity_.Get()){fail("snapshot_device_changed");return false;}
        return true;
    }
    std::shared_ptr<Snapshot> prepare(ID3D12Resource* source_image,Key key,ID3D12Resource* sharp_image=nullptr,std::shared_ptr<const prepaint_frame_policy::Frame> policy={}){
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_||!producer_||!source_image)return{};
        const auto completed=fence_->GetCompletedValue();if(completed==UINT64_MAX){fail("snapshot_device_removed");return{};}
        ComPtr<ID3D12Device> source_device;
        if(FAILED(source_image->GetDevice(IID_PPV_ARGS(&source_device)))||identity(source_device.Get()).Get()!=device_identity_.Get()){fail("snapshot_source_device_refused");return{};}
        const auto desc=source_image->GetDesc();if(!surface_formats::texture(desc)){fail("snapshot_source_extent_refused");return{};}
        if(bool(sharp_image)!=(key.sharp_serial!=0)){fail("snapshot_pair_key_refused");return{};}
        if(sharp_image){ComPtr<ID3D12Device> sharp_device;
            if(source_image==sharp_image||!surface_formats::compatible(desc,sharp_image->GetDesc())||desc.Width>8192||
               FAILED(sharp_image->GetDevice(IID_PPV_ARGS(&sharp_device)))||identity(sharp_device.Get()).Get()!=device_identity_.Get()){fail("snapshot_sharp_pair_identity_refused");return{};}}
        auto atlas_desc=desc;if(sharp_image)atlas_desc.Width*=2;
        std::shared_ptr<Snapshot>* free=nullptr;
        for(auto& slot:pool_)if(!slot||(slot.use_count()==1&&!slot->poisoned&&slot->ticket&&slot->ticket<=completed)){free=&slot;break;}
        if(!free){++misses_;reason_="snapshot_pool_busy_fallback";return{};}
        auto checked=[&](HRESULT hr){if(FAILED(hr)){if(*free)(*free)->poisoned=true;fail("snapshot_recording_failed");return false;}return true;};
        if(!*free){
            *free=std::make_shared<Snapshot>();auto& s=**free;
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;auto image_desc=atlas_desc;image_desc.Flags=D3D12_RESOURCE_FLAG_NONE;
            s.allocation_bytes=device_->GetResourceAllocationInfo(0,1,&image_desc).SizeInBytes;
            if(!checked(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&image_desc,D3D12_RESOURCE_STATE_COPY_SOURCE,nullptr,IID_PPV_ARGS(&s.image)))||
               !checked(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.allocator)))||
               !checked(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.allocator.Get(),nullptr,IID_PPV_ARGS(&s.commands))))return{};
        }else{
            auto& s=**free;if(!surface_formats::compatible(atlas_desc,s.image->GetDesc())){fail("snapshot_dimensions_changed");return{};}
            if(!checked(s.allocator->Reset())||!checked(s.commands->Reset(s.allocator.Get(),nullptr)))return{};++reuses_;
        }
        auto& s=**free;s.source=source_image;s.sharp_source=sharp_image;s.sharp_serial=key.sharp_serial;s.revision=key.revision;s.epoch=key.epoch;s.serial=key.serial;s.ticket=0;
        s.policy=std::move(policy);
        transition(s.commands.Get(),s.image.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        if(!sharp_image)s.commands->CopyResource(s.image.Get(),source_image);
        else{D3D12_TEXTURE_COPY_LOCATION to{},from{};to.pResource=s.image.Get();from.pResource=source_image;
            to.Type=from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;s.commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
            from.pResource=sharp_image;s.commands->CopyTextureRegion(&to,static_cast<UINT>(desc.Width),0,0,&from,nullptr);}
        transition(s.commands.Get(),s.image.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
        if(!checked(s.commands->Close()))return{};return *free;
    }
    // Host commands and, if present, the snapshot command list have ALREADY
    // been submitted exactly once on the pinned queue, in that order.
    bool signal(const std::shared_ptr<Snapshot>& snapshot){
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_||!producer_||ticket_==UINT64_MAX-1){if(snapshot)snapshot->poisoned=true;fail("snapshot_signal_unavailable");return false;}
        if(FAILED(signal_queue(producer_.Get(),fence_.Get(),ticket_+1))){if(snapshot){snapshot->poisoned=true;snapshot->ticket=UINT64_MAX;}fail("snapshot_signal_failed");return false;}
        ++ticket_;++signals_;
        if(snapshot){snapshot->ticket=ticket_;latest_=snapshot;++snapshots_;reason_="immutable_snapshot_signaled";}
        return true;
    }
    std::shared_ptr<Snapshot> match(Key key){
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_||!latest_||!latest_->ticket||latest_->poisoned||latest_->revision!=key.revision||latest_->epoch!=key.epoch||latest_->serial!=key.serial||latest_->sharp_serial!=key.sharp_serial)return{};
        return latest_;
    }
    bool wait(ID3D12CommandQueue* queue,const std::shared_ptr<Snapshot>& snapshot){
        std::lock_guard<std::mutex> lock(mutex_);
        if(failed_||!snapshot||snapshot->poisoned||!snapshot->ticket||snapshot->ticket>ticket_||!same_device(queue,device_.Get())){fail("snapshot_consumer_refused");return false;}
        bool member=false;for(const auto& slot:pool_)member=member||slot==snapshot;if(!member){fail("snapshot_consumer_foreign_lease");return false;}
        // Signal was enqueued before this wait. NO consumer completion is ever
        // waited on by the producer; a slow VR queue simply retains its slot.
        if(FAILED(wait_queue(queue,fence_.Get(),snapshot->ticket))){fail("snapshot_consumer_wait_failed");return false;}
        ++waits_;last_consumer_=reinterpret_cast<uintptr_t>(queue);return true;
    }
    void invalidate(const char* reason){std::lock_guard<std::mutex> lock(mutex_);fail(reason);}
    bool started()const{std::lock_guard<std::mutex> lock(mutex_);return bool(producer_);}
    bool failed()const{std::lock_guard<std::mutex> lock(mutex_);return failed_;}
    const char* reason()const{std::lock_guard<std::mutex> lock(mutex_);return reason_;}
    void json(std::ostream& out)const{
        std::lock_guard<std::mutex> lock(mutex_);unsigned allocated=0,retained=0;uint64_t bytes=0;for(const auto& s:pool_)if(s){++allocated;if(s->poisoned)++retained;if(s->allocation_bytes!=UINT64_MAX)bytes+=s->allocation_bytes;}
        out<<"{\"method\":\"producer_queue_immutable_snapshot\",\"lifetime\":\"process_totals\",\"failed\":"<<(failed_?"true":"false")<<",\"reason\":\""<<reason_
           <<"\",\"producerQueue\":"<<reinterpret_cast<uintptr_t>(producer_.Get())<<",\"producerIdentity\":"<<reinterpret_cast<uintptr_t>(producer_identity_.Get())
           <<",\"rendererQueue\":"<<expected_raw_<<",\"rendererIdentity\":"<<expected_identity_<<",\"lastConsumerQueue\":"<<last_consumer_
           <<",\"lastSignaledValue\":"<<ticket_<<",\"signals\":"<<signals_<<",\"snapshots\":"<<snapshots_<<",\"consumerWaits\":"<<waits_
           <<",\"poolAllocated\":"<<allocated<<",\"poolTextureAllocationBytes\":"<<bytes<<",\"poolBusyFallbacks\":"<<misses_<<",\"poolReuses\":"<<reuses_<<",\"retainedAfterFailure\":"<<retained
           <<",\"producerQueueChanges\":"<<queue_changes_<<",\"producerWaitsForConsumer\":0}";
    }
};
}
