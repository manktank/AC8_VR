#pragma once
// TEMP Plan C: observed production lineage for PRIVATE immutable snapshots. XR image-write
// permission still comes exclusively from the accepted acquired-image/fence path.
#include <cstdint>
#include <vector>
#include <unordered_map>
namespace selective_glow_split_proof {
enum class Image {ui,owned,consumer}; // consumer marks the actual UI-copy position
struct Op {Image image;uint32_t before=0,after=0;};
struct Stream {
    static constexpr uint32_t unknown=~uint32_t{0},render_target=4,copy_source=0x800;
    uint32_t ui_state=unknown,owned_state=unknown;
    uint64_t ui_epoch=1,owned_epoch=0,pending_owned_epoch=0;
    uint64_t probe_epoch=0,owned_serial=0;
    bool valid=true,pending_owned=false,owned_copy_source_verified=false;
    const char* reason="waiting_owned_production";
    struct Mismatch {bool recorded=false;Image image=Image::ui;uint32_t expected=unknown,before=unknown,after=unknown;uint64_t epoch=0,serial=0;} mismatch;
    uint64_t ui_boundary_resyncs=0;
    Mismatch first_ui_boundary;
    uint64_t host_boundary_discards=0;
    // Observe the host-owned source only at its complete, known copy/clear
    // transaction. A gap invalidates every private key; it grants no copy and
    // does not insert, omit or modify an actual GPU transition.
    bool discard_host_boundary(const std::vector<Op>& ops,bool pinned_host){
        if(!valid||!pinned_host||ui_state!=render_target||!owned_copy_source_verified||
           owned_state!=copy_source||pending_owned||ops.size()!=5)return false;
        const Op expected[]{{Image::ui,0xc0,copy_source},{Image::consumer,0,0},
            {Image::ui,copy_source,0xc0},{Image::ui,0xc0,render_target},{Image::ui,render_target,0xc0}};
        for(unsigned i=0;i<5;++i)if(ops[i].image!=expected[i].image||ops[i].before!=expected[i].before||ops[i].after!=expected[i].after)return false;
        ++host_boundary_discards;++ui_epoch;ui_state=0xc0;owned_epoch=0;pending_owned_epoch=0;
        reason="host_ui_boundary_discard_waiting_fresh_private_production";return true;
    }
    // The host copies/clears the original UI on a different queue. Its visible
    // transitions are not a complete game-write history (capture1793580150).
    // At this exact pinned-game-queue terminal-RT boundary, discard ALL prior
    // private lineage. This grants no copy: only a newly observed private RTV
    // -> COPY_SOURCE production in the new epoch can restore admission.
    bool resync_external_ui(const Op& op,bool distinct_pinned_producer){
        if(!valid||!distinct_pinned_producer||op.image!=Image::ui||ui_state!=0xc0||op.before!=render_target||op.after!=0xc0||
           !owned_copy_source_verified||owned_state!=copy_source)return false;
        if(!first_ui_boundary.recorded)first_ui_boundary={true,Image::ui,ui_state,op.before,op.after,ui_epoch,owned_serial};
        ++ui_boundary_resyncs;++ui_epoch;ui_state=op.after;owned_epoch=0;pending_owned=false;pending_owned_epoch=0;
        reason="native_ui_boundary_waiting_fresh_private_production";return true;
    }
    void apply(const Op& op){
        if(!valid)return;
        if(op.image==Image::consumer)return;
        auto& state=op.image==Image::ui?ui_state:owned_state;
        if(state!=unknown&&state!=op.before){mismatch={true,op.image,state,op.before,op.after,ui_epoch,owned_serial};valid=false;reason="producer_state_disagreement";return;}
        state=op.after;
        if(op.image==Image::ui){
            // A post-copy clear invalidates only FUTURE consumers. Account
            // conservatively for every non-read state (also COPY_DEST/UAV or
            // COMMON), so a later list cannot reuse the old UI lineage.
            if(!read_state(op.after)){++ui_epoch;reason="ui_changed_waiting_owned_production";}
        }else{
            if(op.after==render_target){pending_owned=true;pending_owned_epoch=ui_epoch;}
            if(op.after==copy_source){
                // The native access code is first tested on the owned target.
                // Only its actual D3D12 COPY_SOURCE transition admits UI use.
                if(!owned_copy_source_verified)probe_epoch=ui_epoch;
                owned_copy_source_verified=true;
                if(pending_owned){++owned_serial;owned_epoch=pending_owned_epoch;pending_owned=false;reason="owned_production_submitted";}
            }
        }
    }
    bool paired()const{return valid&&owned_copy_source_verified&&owned_epoch>probe_epoch&&owned_epoch==ui_epoch&&ui_state==copy_source&&owned_state==copy_source;}
    static bool read_state(uint32_t state){return state&&!(state&~uint32_t{0x8c0});}
    bool snapshot_ready()const{return valid&&owned_copy_source_verified&&owned_epoch>probe_epoch&&owned_epoch==ui_epoch&&read_state(ui_state)&&owned_state==copy_source;}
};
struct Recorder {
    Stream submitted;
    std::unordered_map<uintptr_t,std::vector<Op>> pending;
    bool valid=true;const char* reason="waiting_submission";
    void record(uintptr_t list,Op op,bool whole_unsplit=true){
        if(!valid)return;
        if(!list||!whole_unsplit){valid=false;reason="unsupported_producer_barrier";return;}
        if(!pending.count(list)&&pending.size()>=64){valid=false;reason="producer_list_capacity";return;}
        auto& ops=pending[list];if(ops.size()>=512){valid=false;reason="producer_barrier_capacity";return;}ops.push_back(op);
    }
    void reset(uintptr_t list){pending.erase(list);}
    // Call in the real ExecuteCommandLists order. A consumer's owned snapshot
    // list, if admitted by paired_before(), is inserted immediately BEFORE it.
    bool paired_before(bool accepted_queue)const{return valid&&accepted_queue&&submitted.paired();}
    void submitted_list(uintptr_t list,bool accepted_queue){
        auto it=pending.find(list);if(it==pending.end())return;
        if(!accepted_queue){valid=false;reason="producer_queue_disagreement";pending.erase(it);return;}
        for(const auto& op:it->second)submitted.apply(op);pending.erase(it);
        if(!submitted.valid){valid=false;reason=submitted.reason;}
    }
};
}
