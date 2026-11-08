#pragma once
// TEMP Plan C: opt-in radio identity split and bounded comparison. The original
// sharp pass always runs unchanged. No game tick/paint is repeated and no pixel
// threshold/rectangle selects membership in the private glow input.
#include "temporary-glow-batch-capture.h"
#include "temporary-glow-batch-plan.h"
#include "temporary-glow-target.h"
#include "temporary-glow-split-proof.h"
#include "temporary-glow-native-barriers.h"
#include "temporary-glow-execution.h"
#include "temporary-glow-native-state.h"
#include "temporary-glow-handoff.h"
#include "temporary-glow-retained-clips.h"
#include "temporary-glow-private-stencil.h"
#include <unordered_set>
namespace selective_glow_split {
namespace batch=selective_glow_batch_capture;
namespace paint=radio_paint_marker;
namespace perf=selective_glow_performance;
namespace proof_types=selective_glow_split_proof;
namespace ownership=selective_glow_ownership;
using Object=UEVR_UObjectHandle;
inline const UEVR_PluginInitializeParam* api=nullptr;
inline bool ready=false;
inline std::atomic<int> phase{-1}; // -1 finished/off;0 current;1 split requested.
inline std::atomic<bool> copy_source_verified{false},notify_pending{false},probe_queued{false},probe_observation_armed{false};
inline std::atomic<bool> consumer_copy_admitted{false};
inline std::atomic<const char*> last_reason{"not_started"},stop_reason{nullptr},end_reason{"not_started"};
inline std::filesystem::path directory;
inline constexpr const char* diagnostic_build="selective-glow-scan-radar-preserve-20261105";
struct SourceBinding {uintptr_t rhi=0;Microsoft::WRL::ComPtr<ID3D12Resource> native;UINT width=0,height=0;};
inline std::shared_ptr<SourceBinding> source_binding;
struct Measurements {
    perf::Samples engine,present,original_pass_cpu,extra_pass_cpu,planning_cpu,ownership_cpu,handoff_cpu,routing_cpu,generation_cpu,clean_base_cpu;
    uint64_t clean_base_recordings=0,clean_base_allocation_bytes=0;
    uint64_t snapshots=0,consumer_waits=0;
    uint64_t wall_ticks=0,planned_passes=0,planned_draws=0,excluded_draws=0,refused_plans=0,admitted_copies=0,fallback_copies=0;
};
struct CommandSnapshot {bool readable=false;std::array<uintptr_t,3> words{};};
struct ProbeEvidence {CommandSnapshot before,after_pass,after_transition;LONG rhi_before=0,rhi_after=0,policy_before=0,policy_after=0;bool refs_read=false;uintptr_t entry=0;};
inline CommandSnapshot command_snapshot(uintptr_t command_list){
    CommandSnapshot result;std::array<unsigned char,0xa0> bytes{};result.readable=selective_glow_inventory::read(reinterpret_cast<void*>(command_list),bytes);
    if(result.readable){std::memcpy(&result.words[0],bytes.data(),8);std::memcpy(&result.words[1],bytes.data()+8,8);std::memcpy(&result.words[2],bytes.data()+0x98,8);}return result;
}
struct Comparison {
    std::vector<std::shared_ptr<paint::Session>> selections;
    std::mutex mutex;std::string id,identity_stop_detail;std::array<Measurements,2> phases;
    std::string last_consumer_reason="not_observed";std::unordered_map<std::string,uint64_t> consumer_reasons;
    std::unordered_map<std::string,uint64_t> plan_reasons;
    struct PlanWitness {std::string reason;uint64_t generation=0;uintptr_t window=0,data=0;unsigned excluded=0,uncertain=0;int32_t row=-1;unsigned mask=0;bool verified=false;batch::Raw raw{};};
    std::vector<PlanWitness> plan_witnesses; // first example per reason, never overwritten by paint events
    uint64_t partially_filtered_passes=0,uncertain_retained_draws=0;
    std::unordered_map<std::string,selective_glow_retained_clips::Stats> clip_witnesses;
    uint64_t phase_begin=0,last_engine=0,last_present=0;bool finished=false;
    std::array<uint64_t,18> gates{};std::array<uintptr_t,12> first_ui_args{};
    bool sampled_ui=false,batch_armed_before_stop=false,capture_present_before_stop=false;
    ProbeEvidence probe;
};
inline std::shared_ptr<Comparison> comparison;
inline std::mutex request_mutex,proof_mutex;
inline int pending_action=0;inline std::string pending_id,ack_id;
inline proof_types::Recorder proof;
inline selective_glow_handoff::Handoff handoff;
// These touches survive comparison resets/off while already queued native work drains.
inline std::unordered_set<uintptr_t> owned_touches;
inline uint64_t proof_revision=0,last_roll_frame=0;
struct QueueFailure {bool recorded=false,device_matches=false;uintptr_t actual=0,expected=0,list=0;uint32_t ui_ops=0,owned_ops=0;};
inline QueueFailure queue_failure;
struct StateEvent {uintptr_t queue=0,list=0;uint32_t ordinal=0;proof_types::Op op{};bool ui_boundary_resync=false;};
struct StateHistory {
    std::array<StateEvent,32> events{};size_t cursor=0,count=0;bool frozen=false;
    void add(StateEvent event){if(frozen)return;events[cursor]=event;cursor=(cursor+1)%events.size();count=std::min(count+1,events.size());}
};
inline StateHistory state_history,sharp_state_history;
inline proof_types::Recorder sharp_proof;
inline QueueFailure sharp_queue_failure;
inline bool paired_layers(){auto t=selective_glow_target::get();return t&&t->sharp&&t->sharp->published;}

inline unsigned preparing_ticks=0;
inline std::atomic<uintptr_t> admitted_context{0},admitted_view{0};
struct TransitionInfo {std::array<unsigned char,56> bytes{};};
struct alignas(16) TransitionSpan {const TransitionInfo* data=nullptr;int32_t count=1,padding=0;};
static_assert(sizeof(TransitionSpan)==16&&alignof(TransitionSpan)==16,"native span uses aligned16-byte load");
using Transition=void(*)(uintptr_t,const TransitionSpan*);
inline Transition transition=nullptr;
inline TransitionInfo copy_source_info(uintptr_t texture){
    TransitionInfo v;const int32_t all=-1;const uint8_t type=1;const uint32_t after=0x40;
    std::memcpy(v.bytes.data(),&all,4);std::memcpy(v.bytes.data()+4,&all,4);std::memcpy(v.bytes.data()+8,&all,4);
    std::memcpy(v.bytes.data()+0x10,&texture,8);std::memcpy(v.bytes.data()+0x18,&type,1);std::memcpy(v.bytes.data()+0x20,&after,4);return v;
}
inline void to_copy_source(uintptr_t command_list,uintptr_t texture){const auto info=copy_source_info(texture);const TransitionSpan span{&info,1,0};transition(command_list,&span);}
inline void control(const char* data){
    if(!data)return;std::string value=data;int action=0;std::string id;
    if(value.rfind("toggle:",0)==0){action=1;id=value.substr(7);if(id.empty()||id.size()>48||id.find_first_not_of("0123456789-")!=std::string::npos)return;}
    else if(value=="off"||value=="save")action=2;
    else if(value=="status")action=3;else if(value=="snapshot")action=4;else return;
    std::lock_guard<std::mutex> lock(request_mutex);pending_action=action;pending_id=id;
}
template<class F>inline void measure(F work){if(!perf::reporting)return;auto c=std::atomic_load(&comparison);const int index=phase.load();if(c&&index>=0){std::lock_guard<std::mutex> lock(c->mutex);if(!c->finished)work(c->phases[static_cast<size_t>(index)]);}}
enum Gate:unsigned {entered,no_binding,non_ui,matched_ui,no_target,bad_command,bad_extent,bad_secondary,bad_owner,bad_producer,probe_read_failed,probe_sent,probe_wait,planning,private_sent,preparation_failed,ownership_prepared,ownership_refused};
inline constexpr const char* gate_names[]{"entered","noBinding","nonUiPass","matchedUiPass","targetNotPublished","commandListMismatch","extentMismatch","secondaryMismatch","ownerLifetimeRefused","producerChanged","probeReadFailed","ownedClearProbeQueued","waitingProbe","planning","privatePassQueued","preparationFailed","policyOwnershipPrepared","ownershipRefused"};
inline void note(Gate gate,const char* reason=nullptr,const std::array<uintptr_t,12>* args=nullptr){
    if(reason)last_reason=reason;if(!perf::reporting)return;auto c=std::atomic_load(&comparison);if(!c)return;std::lock_guard<std::mutex> lock(c->mutex);++c->gates[gate];
    if(args&&!c->sampled_ui){c->first_ui_args=*args;c->sampled_ui=true;}
}
inline void original_cpu(uintptr_t target_value,uint64_t ticks){auto binding=std::atomic_load(&source_binding);if(binding&&target_value==binding->rhi)measure([&](Measurements& m){m.original_pass_cpu.add(ticks);});}
inline void cadence(bool present){
    if(!perf::reporting)return;
    auto c=std::atomic_load(&comparison);const int index=phase.load();if(!c||index<0)return;const auto now=perf::sample_clock();std::lock_guard<std::mutex> lock(c->mutex);if(c->finished)return;
    auto& last=present?c->last_present:c->last_engine;auto& samples=present?c->phases[static_cast<size_t>(index)].present:c->phases[static_cast<size_t>(index)].engine;if(last)samples.add(now-last);last=now;
}
inline void tracking_off(){batch::armed=false;batch::route_end();std::atomic_store(&batch::current,std::shared_ptr<batch::Capture>{});paint::off("identity_split_tracking_stopped");}
inline bool tracking_on(){
    paint::control("trace");auto admission=std::atomic_load(&paint::current);auto binding=std::atomic_load(&source_binding);
    if(!admission||!admission->trace_only||!binding)return false;
    if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);if(c->selections.size()<32)c->selections.push_back(admission);}
    auto capture=std::make_shared<batch::Capture>();capture->continuous=true;capture->admission=admission;capture->ui_rhi=binding->rhi;capture->ui_native=reinterpret_cast<uintptr_t>(binding->native.Get());
    batch::route_start(capture);std::atomic_store(&batch::current,capture);batch::armed=true;return true;
}
inline void roll(const std::shared_ptr<batch::Capture>& previous,uint64_t frame){
    if(phase.load()!=1||frame==last_roll_frame)return;last_roll_frame=frame;
    // Native window generations, not XR-copy timing, own identity metadata.
    // Two bounded slots/window cover next paint and current batch. Draw consumes
    // a specific upload once; a new native window invalidates the prior batch.
    if(previous->routing_session)batch::route_copy(previous,frame);
}
struct Work {
    std::shared_ptr<const prepaint_frame_policy::Frame> frame_policy;
    std::array<unsigned char,96> data{};std::array<unsigned char,256> context{};identity_batch_plan::Plan plan;selective_glow_retained_clips::Plan clips;const char* reason="batch_data_not_observed";
    std::unique_ptr<Work> sharp;
    Work()=default;Work(const Work&)=delete;Work& operator=(const Work&)=delete;Work(Work&&)=default;Work& operator=(Work&&)=default;
};
inline Work plan(const std::shared_ptr<batch::Capture>& capture,const std::array<uintptr_t,12>& args){
    Work w;
    if(capture->route_reason){w.reason=capture->route_reason;return w;}
    if(capture->generation&&!batch::route_current(capture)){w.reason="batch_generation_replaced_or_session_ended";return w;}
    std::lock_guard<std::mutex> lock(capture->mutex);
    if(capture->done||capture->unresolved_elements||capture->copy_mismatches){w.reason="incomplete_or_changed_element_coverage";return w;}
    const batch::Build* built=nullptr;
    for(auto p=capture->packets.rbegin();p!=capture->packets.rend()&&!built;++p)for(auto b=p->builds.rbegin();b!=p->builds.rend();++b)if(b->data==args[5]){built=&*b;break;}
    if(!built){w.reason="completed_upload_not_observed";return w;}
    capture->frame_policy.upload=built->serial;w.frame_policy=prepaint_frame_policy::freeze(capture->frame_policy,static_cast<UINT>(args[7]),static_cast<UINT>(args[8]));
    if(!batch::read(reinterpret_cast<void*>(args[5]),w.data)){w.reason="batch_header_unreadable";return w;}
    if(!batch::read(reinterpret_cast<void*>(args[3]),w.context)){w.reason="draw_context_unreadable";return w;}
    if(capture->generation&&(!built->header_read||w.data!=built->header)){w.reason="post_upload_header_changed";return w;}
    batch::Array a;std::memcpy(&a,w.data.data(),sizeof(a));if(a.count<0||static_cast<size_t>(a.count)!=built->after.size()||a.count>2048){w.reason="batch_array_changed";return w;}
    std::vector<batch::Raw> records;std::vector<identity_batch_plan::Origin> labels;records.reserve(built->after.size());labels.reserve(built->after.size());
    for(const auto& row:built->after){batch::Raw actual;if(!batch::read(reinterpret_cast<void*>(a.data+static_cast<uintptr_t>(row.index)*144),actual)||actual.bytes!=row.raw.bytes){w.reason="post_build_batch_bytes_changed";return w;}records.push_back(actual);labels.push_back({row.mask,row.verified});}
    int32_t first=-1;std::memcpy(&first,w.data.data()+0x50,4);w.plan=identity_batch_plan::make(records,labels,first,built->vertices,built->indices);
    // A valid current UI generation with no confirmed removals keeps its whole
    // original chain. It must not cancel independent verified exclusions in
    // other generations, or require a selected widget to paint every frame.
    const auto original_clip_summary=w.data[0x5c];
    if(w.plan.ready){
        w.clips=selective_glow_retained_clips::preserve(w.plan,w.data[0x5c]);
        if(!w.clips.ready){w.plan.ready=false;w.plan.reason=w.clips.reason;}
        else w.data[0x5c]=w.clips.stats.stencil?1:0; // copied retained-chain summary; sharp header untouched
    }
    if(w.plan.ready&&paired_layers()){
        w.sharp=std::make_unique<Work>();auto& sharp=*w.sharp;sharp.data=w.data;sharp.context=w.context;sharp.frame_policy=w.frame_policy;
        std::vector<identity_batch_plan::Origin> selected_labels;selected_labels.reserve(labels.size());
        for(const auto& label:labels)selected_labels.push_back({label.verified&&label.mask==batch::selected?batch::other:batch::selected,true});
        sharp.plan=identity_batch_plan::make(records,selected_labels,first,built->vertices,built->indices);
        if(sharp.plan.ready){sharp.clips=selective_glow_retained_clips::preserve(sharp.plan,original_clip_summary);
            if(!sharp.clips.ready){sharp.plan.ready=false;sharp.plan.reason=sharp.clips.reason;}
            else sharp.data[0x5c]=sharp.clips.stats.stencil?1:0;}
        if(!sharp.plan.ready){w.plan.ready=false;w.plan.reason=sharp.plan.reason;}
        sharp.reason=sharp.plan.reason;
    }
    if(w.plan.ready&&capture->admission)for(const auto index:w.plan.excluded)paint::widget_policy::count(capture->admission->planned_excluded,built->after[static_cast<size_t>(index)].roles);
    w.reason=w.plan.reason;return w;
}
inline void prepare_pass(const std::array<uintptr_t,12>& args,ownership::Prepared& prepared){
    prepared.snapshot=paint::pointer_field(args[0],prepared.primary)&&paint::pointer_field(args[11],prepared.secondary);
    if(phase.load()!=1||stop_reason.load()||!prepared.snapshot)return;
    auto binding=std::atomic_load(&source_binding);auto target=selective_glow_target::get();
    if(!binding||!target||prepared.primary!=binding->rhi||prepared.secondary!=binding->rhi||
       args[6]!=paint::image_base+ac8_game_compatibility::rva<0xe2f67a8>()||static_cast<uint32_t>(args[7])!=binding->width||static_cast<uint32_t>(args[8])!=binding->height||!selective_glow_target::valid(*target))return;
    // This owning copy MUST precede the original callee's argument cleanup.
    const auto begin=perf::sample_clock();const bool copied=prepared.policy.copy(args[10])&&(!target->sharp||prepared.sharp_policy.copy(args[10]));const auto elapsed=perf::sample_clock()-begin;
    measure([&](Measurements& m){m.ownership_cpu.add(elapsed);});
    if(copied)note(ownership_prepared);else note(ownership_refused,"native_policy_reference_copy_refused");
}
inline bool private_pass(const std::array<uintptr_t,12>& args,Work& w,const std::shared_ptr<selective_glow_target::Owner>& target,ownership::Prepared& prepared){
    ownership::Rhi first,secondary;
    const auto ownership_begin=perf::sample_clock();const bool references=prepared.policy.owned&&first.copy(target->rhi)&&secondary.copy(target->rhi);const auto ownership_elapsed=perf::sample_clock()-ownership_begin;
    measure([&](Measurements& m){m.ownership_cpu.add(ownership_elapsed);});if(!references)return false;
    {
        batch::Array a{reinterpret_cast<uintptr_t>(w.plan.private_records.data()),static_cast<int32_t>(w.plan.private_records.size()),static_cast<int32_t>(w.plan.private_records.size())};std::memcpy(w.data.data(),&a,sizeof(a));
        std::memcpy(w.data.data()+0x50,&w.plan.first,4);const auto draws=static_cast<int32_t>(w.plan.glow_draws);std::memcpy(w.data.data()+0x58,&draws,4);
    }
    w.context[0xd8]=0;
    if(w.clips.stats.stencil){
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        if(FAILED(target->native->GetDevice(IID_PPV_ARGS(&device)))||!selective_glow_private_stencil::bind(w.context,target->width,target->height,device.Get()))return false;
    }
    const bool sample_probe=!copy_source_verified.load();ProbeEvidence evidence;const auto control=prepared.policy.words[1];
    if(sample_probe){auto primary=selective_glow_target::get();if(!primary)primary=target;
        selective_glow_execution::bind(prepared.primary,primary->rhi,reinterpret_cast<uintptr_t>(primary->native.Get()));
        selective_glow_native_state::bind(primary->rhi,reinterpret_cast<uintptr_t>(primary->native.Get()));
        if(primary->sharp){selective_glow_execution::bind_second(primary->sharp->rhi,reinterpret_cast<uintptr_t>(primary->sharp->native.Get()));
            selective_glow_native_state::bind_second(primary->sharp->rhi,reinterpret_cast<uintptr_t>(primary->sharp->native.Get()));}
        probe_observation_armed=true;
        if(perf::reporting){evidence.before=command_snapshot(args[6]);evidence.entry=reinterpret_cast<uintptr_t>(batch::original_draw_pass);
            evidence.refs_read=ownership::read(target->rhi+8,evidence.rhi_before)&&ownership::read(control+8,evidence.policy_before);}}
    const auto begin=perf::sample_clock();
    // Independent argument storage and owned units. The native callee consumes
    // all three; these RAII wrappers must not release them a second time.
    const auto first_ref=first.transfer(),secondary_ref=secondary.transfer(),policy_ref=prepared.policy.transfer();
    batch::original_draw_pass(first_ref,1,args[2],reinterpret_cast<uintptr_t>(w.context.data()),args[4],reinterpret_cast<uintptr_t>(w.data.data()),args[6],args[7],args[8],args[9],policy_ref,secondary_ref);
    if constexpr(perf::reporting){
        if(sample_probe){evidence.after_pass=command_snapshot(args[6]);evidence.refs_read=evidence.refs_read&&ownership::read(target->rhi+8,evidence.rhi_after)&&ownership::read(control+8,evidence.policy_after);}
    }
    uintptr_t previous_tail=0;ownership::read(args[6]+8,previous_tail);
    prepaint_frame_policy::Scope policy_scope(w.frame_policy);
    to_copy_source(args[6],target->rhi);
    prepaint_frame_policy::queued_commands(args[6],previous_tail,paint::image_base+ac8_game_compatibility::rva<0x36354f0>(),w.frame_policy);
    const auto elapsed=perf::sample_clock()-begin;measure([&](Measurements& m){m.extra_pass_cpu.add(elapsed);});
    if constexpr(perf::reporting){
        if(sample_probe){evidence.after_transition=command_snapshot(args[6]);if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);c->probe=evidence;}}
    }
    return true;
}
inline bool private_layers(const std::array<uintptr_t,12>& args,Work& work,const std::shared_ptr<selective_glow_target::Owner>& target,ownership::Prepared& prepared){
    if(!target->sharp)return private_pass(args,work,target,prepared);
    if(!selective_glow_target::valid(*target->sharp)||!prepared.sharp_policy.owned||
       !ownership::table(target->sharp->rhi,false)||!ownership::positive_count(target->sharp->rhi+8))return false;
    Work probe_sharp;Work* sharp=work.sharp.get();
    if(!sharp){if(copy_source_verified.load())return false;probe_sharp.data=work.data;probe_sharp.context=work.context;probe_sharp.data[0x5c]=0;probe_sharp.plan.first=-1;sharp=&probe_sharp;}
    ownership::Prepared selected;selected.primary=prepared.primary;selected.secondary=prepared.secondary;selected.snapshot=prepared.snapshot;
    selected.policy.words=prepared.sharp_policy.words;selected.policy.owned=prepared.sharp_policy.owned;prepared.sharp_policy.owned=false;
    // Separate native owned arguments and targets. A partial pair cannot pass
    // the two independent submitted-state/epoch proofs or publish an atlas.
    if(!private_pass(args,work,target,prepared))return false;
    return private_pass(args,*sharp,target->sharp,selected);
}
inline void native_pass(const std::shared_ptr<batch::Capture>& capture,const std::array<uintptr_t,12>& args,ownership::Prepared& prepared){
    if(phase.load()!=1||stop_reason.load())return;note(entered);auto binding=std::atomic_load(&source_binding);auto target=selective_glow_target::get();uintptr_t rhi=0;
    if(!binding){note(no_binding,"ui_binding_unavailable");return;}
    rhi=prepared.primary;if(!prepared.snapshot||rhi!=binding->rhi){note(non_ui);return;}
    note(matched_ui,nullptr,&args);
    if(!target){note(no_target,"private_target_not_published");return;}
    if(args[6]!=paint::image_base+ac8_game_compatibility::rva<0xe2f67a8>()){note(bad_command,"native_command_list_mismatch");return;}
    if(static_cast<uint32_t>(args[7])!=binding->width||static_cast<uint32_t>(args[8])!=binding->height){note(bad_extent,"native_pass_extent_mismatch");return;}
    if(prepared.secondary!=binding->rhi){note(bad_secondary,"native_secondary_target_mismatch");return;}
    if(!selective_glow_target::valid(*target)){note(bad_owner,"private_target_lifetime_refused");return;}
    if(!prepared.policy.owned){note(ownership_refused,"native_policy_reference_copy_unavailable");return;}
    try{
        uintptr_t unset=0;admitted_context.compare_exchange_strong(unset,args[3]);unset=0;admitted_view.compare_exchange_strong(unset,args[4]);
        if(admitted_context.load()!=args[3]||admitted_view.load()!=args[4]){note(bad_producer,"ui_producer_context_changed");stop_reason="ui_producer_context_changed";return;}
        // Test native access mapping on the owned target first. No sharp-source
        // transition and no source copy is admitted until D3D12 confirms it.
        if(!copy_source_verified.load()){
            if(!probe_queued.load()){
                Work probe;
                if(!batch::read(reinterpret_cast<void*>(args[5]),probe.data)||!batch::read(reinterpret_cast<void*>(args[3]),probe.context)){note(probe_read_failed,"owned_probe_context_unreadable");return;}
                // A clear-only owned render pass creates an explicit RTV cycle
                // before CopySrc, including when initialization already left the
                // texture in its requested state. No original draw is replayed.
                probe.data[0x5c]=0;probe.plan.first=-1;
                if(private_layers(args,probe,target,prepared)){probe_queued=true;note(probe_sent,"waiting_owned_clear_then_copy_source_probe");}
                else note(ownership_refused,"private_target_reference_copy_refused");
            }else note(probe_wait,"waiting_owned_clear_then_copy_source_probe");
            return;
        }
        // The host owns sharp-UI transitions. Observe its actual copy-source
        // barrier at the normal consumer; never force a new state onto that image.
        note(planning);
        const auto begin=perf::sample_clock();auto work=plan(capture,args);const auto elapsed=perf::sample_clock()-begin;
        measure([&](Measurements& m){m.planning_cpu.add(elapsed);if(work.plan.ready){++m.planned_passes;m.planned_draws+=work.plan.glow_draws;m.excluded_draws+=work.plan.excluded_draws;}else ++m.refused_plans;});last_reason=work.reason;
        if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);
            if(c->plan_reasons.size()<32||c->plan_reasons.count(work.reason))++c->plan_reasons[work.reason];
            if(work.plan.uncertain_retained){++c->partially_filtered_passes;c->uncertain_retained_draws+=work.plan.uncertain_retained;}
            if((!work.plan.ready||work.plan.uncertain_retained)&&c->plan_witnesses.size()<16&&
               std::none_of(c->plan_witnesses.begin(),c->plan_witnesses.end(),[&](const auto& v){return v.reason==work.reason;})){
                Comparison::PlanWitness v;v.reason=work.reason;v.generation=capture->generation;v.window=capture->route_window;v.data=args[5];v.excluded=work.plan.excluded_draws;v.uncertain=work.plan.uncertain_retained;v.row=work.plan.first_uncertain;
                if(v.row>=0)for(const auto& p:capture->packets)for(const auto& b:p.builds)if(b.data==args[5]&&static_cast<size_t>(v.row)<b.after.size()){const auto& r=b.after[static_cast<size_t>(v.row)];v.raw=r.raw;v.mask=r.mask;v.verified=r.verified;}
                c->plan_witnesses.push_back(std::move(v));
            }
            if(work.clips.stats.checked&&c->clip_witnesses.size()<8&&!c->clip_witnesses.count(work.reason))c->clip_witnesses.emplace(work.reason,work.clips.stats);
        }
        if(work.plan.ready){if(private_layers(args,work,target,prepared))note(private_sent);else note(ownership_refused,"private_target_reference_copy_refused");}
    }catch(...){note(preparation_failed,"private_pass_preparation_failed");}
}
inline void record_barriers(uintptr_t list,UINT count,const D3D12_RESOURCE_BARRIER* barriers){
    auto binding=std::atomic_load(&source_binding);auto target=selective_glow_target::get();if(!target||!barriers)return;
    const auto owned=target->native.Get();
    const auto sharp=target->sharp?target->sharp->native.Get():nullptr;
    const auto watched_source=binding?binding->native.Get():nullptr;
    bool relevant=false;
    for(UINT i=0;i<count&&!relevant;++i){
        if(barriers[i].Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)continue;
        const auto resource=barriers[i].Transition.pResource;
        relevant=resource==owned||resource==sharp||(watched_source&&resource==watched_source);
    }
    if(!relevant)return;
    std::lock_guard<std::mutex> lock(proof_mutex);
    for(UINT i=0;i<count;++i){const auto& b=barriers[i];if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)continue;
        const auto resource=b.Transition.pResource;
        if(resource==target->native.Get()||(target->sharp&&resource==target->sharp->native.Get())){
            if(b.Transition.StateAfter==D3D12_RESOURCE_STATE_COPY_SOURCE)prepaint_frame_policy::producer(list);
            if(owned_touches.size()>=64&&!owned_touches.count(list)){handoff.invalidate("snapshot_touch_capacity");stop_reason="snapshot_touch_capacity";}
            else owned_touches.insert(list);
        }
        if(phase.load()<0||!probe_observation_armed.load()||!binding)continue;
        if(resource!=binding->native.Get()&&resource!=target->native.Get()&&(!target->sharp||resource!=target->sharp->native.Get()))continue;
        // Stage1 validates ONLY the private target's access mapping/submission.
        // UI events cannot supply or invalidate that probe. Stage2 adds UI
        // lineage after the owned COPY_SOURCE probe has actually been submitted.
        if(resource==binding->native.Get()&&!copy_source_verified.load())continue;
        const bool whole=b.Flags==D3D12_RESOURCE_BARRIER_FLAG_NONE&&(b.Transition.Subresource==D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES||b.Transition.Subresource==0);
        const proof_types::Op op{resource==binding->native.Get()?proof_types::Image::ui:proof_types::Image::owned,static_cast<uint32_t>(b.Transition.StateBefore),static_cast<uint32_t>(b.Transition.StateAfter)};
        if(resource==binding->native.Get()||resource==target->native.Get())proof.record(list,op,whole);
        if(target->sharp&&(resource==binding->native.Get()||resource==target->sharp->native.Get()))sharp_proof.record(list,op,whole);
    }
}
inline bool has_owned_work(UINT count,ID3D12CommandList* const* lists){
    std::lock_guard<std::mutex> lock(proof_mutex);for(UINT i=0;lists&&i<count;++i)if(owned_touches.count(reinterpret_cast<uintptr_t>(lists[i])))return true;return false;
}
inline void retire_owned_work(UINT count,ID3D12CommandList* const* lists){
    std::lock_guard<std::mutex> lock(proof_mutex);for(UINT i=0;lists&&i<count;++i)owned_touches.erase(reinterpret_cast<uintptr_t>(lists[i]));
}
struct ConsumerWitness {uintptr_t list=0;proof_types::Stream entry;std::vector<proof_types::Op> ops;size_t total=0;};
inline std::unordered_map<std::string,ConsumerWitness> consumer_witnesses; // first example per reason, bounded below
struct Submission {std::vector<selective_glow_handoff::Key> keys;proof_types::Stream after;StateHistory history;uint64_t revision=0;bool valid=false;const char* reason="waiting_submission";QueueFailure failure;std::vector<uintptr_t> lists;std::vector<bool> choices;std::vector<const char*> choice_reasons;std::shared_ptr<Submission> sharp;};
inline void mark_consumer(uintptr_t list){
    if(phase.load()!=1)return;std::lock_guard<std::mutex> lock(proof_mutex);proof.record(list,{proof_types::Image::consumer,0,0});if(paired_layers())sharp_proof.record(list,{proof_types::Image::consumer,0,0});
}
inline Submission preview_one(const proof_types::Recorder& recorder,const StateHistory& prior_history,const QueueFailure& prior_failure,bool retain_witness,UINT count,ID3D12CommandList* const* lists,bool accepted_queue,uintptr_t actual_queue=0,uintptr_t expected_queue=0,bool device_matches=false,bool distinct_pinned_producer=false){
    Submission result;result.after=recorder.submitted;result.history=prior_history;result.revision=proof_revision;result.valid=recorder.valid;result.reason=recorder.reason;result.failure=prior_failure;result.lists.reserve(count);result.choices.reserve(count);
    for(UINT i=0;i<count;++i){const auto key=reinterpret_cast<uintptr_t>(lists[i]);result.lists.push_back(key);
        const auto entry=result.after;bool prefix_stable=true,has_marker=false,markers_ready=true;
        result.keys.push_back({result.revision,entry.ui_epoch,entry.owned_serial});
        const auto found=recorder.pending.find(key);if(found!=recorder.pending.end()){
            if(!accepted_queue){result.valid=false;result.reason="producer_submitted_on_unaccepted_queue";
                if(!result.failure.recorded){auto& f=result.failure;f.recorded=true;f.device_matches=device_matches;f.actual=actual_queue;f.expected=expected_queue;f.list=key;
                    for(const auto& op:found->second){if(op.image==proof_types::Image::ui)++f.ui_ops;else if(op.image==proof_types::Image::owned)++f.owned_ops;}}
            }else {
                // This exact host copy/clear list is already marked by the
                // isolated original UI-copy observer. Refuse its old snapshot,
                // reconcile observation only, then require fresh production.
                const bool discarded=result.after.discard_host_boundary(found->second,device_matches&&actual_queue&&actual_queue==expected_queue&&!distinct_pinned_producer);
                if(discarded)prefix_stable=false;
                uint32_t ordinal=0;for(const auto& op:found->second){
                const bool resynced=result.after.resync_external_ui(op,distinct_pinned_producer);
                result.history.add({actual_queue,key,ordinal++,op,resynced});
                if(op.image==proof_types::Image::consumer){
                    has_marker=true;
                    markers_ready=markers_ready&&prefix_stable&&result.after.paired()&&result.after.ui_epoch==entry.ui_epoch&&result.after.owned_serial==entry.owned_serial;
                }else if(op.image==proof_types::Image::owned||!proof_types::Stream::read_state(op.before)||!proof_types::Stream::read_state(op.after))prefix_stable=false;
                if(!resynced)result.after.apply(op);
                if(!result.after.valid)result.history.frozen=true;
            }}
        }
        if(!result.after.valid)result.reason=result.after.reason;
        result.valid=result.valid&&result.after.valid;
        // UEVR copies the UI and THEN clears it in this same command list.
        // The immutable snapshot is consumed before that clear; later source
        // writes cannot change its pixels. Validate matching lineage and state
        // at EVERY copy marker. Earlier writes still invalidate the old key.
        // Keep all operations in the proof so the next copy cannot use it stale.
        const bool gate=phase.load()==1&&result.valid&&accepted_queue&&has_marker;
        result.choices.push_back(gate&&entry.snapshot_ready()&&markers_ready);
        const char* reason=phase.load()!=1?"split_not_active":!accepted_queue?"consumer_queue_not_accepted":!result.valid?"source_state_proof_invalid":!has_marker?"ui_copy_position_not_observed":!entry.snapshot_ready()?"matching_private_production_unavailable":!markers_ready?"ui_not_copy_source_or_epoch_changed_at_copy":"eligible_snapshot_lookup";
        result.choice_reasons.push_back(reason);
        if(retain_witness&&has_marker&&consumer_witnesses.size()<12&&!consumer_witnesses.count(reason)){
            ConsumerWitness w;w.list=key;w.entry=entry;w.total=found->second.size();
            w.ops.assign(found->second.begin(),found->second.begin()+std::min(size_t{32},w.total));consumer_witnesses.emplace(reason,std::move(w));
        }
    }return result;
}
inline Submission preview_submission(UINT count,ID3D12CommandList* const* lists,bool accepted_queue,uintptr_t actual_queue=0,uintptr_t expected_queue=0,bool device_matches=false,bool distinct_pinned_producer=false){
    std::lock_guard<std::mutex> lock(proof_mutex);
    auto out=preview_one(proof,state_history,queue_failure,true,count,lists,accepted_queue,actual_queue,expected_queue,device_matches,distinct_pinned_producer);
    if(paired_layers()){
        out.sharp=std::make_shared<Submission>(preview_one(sharp_proof,sharp_state_history,sharp_queue_failure,false,count,lists,accepted_queue,actual_queue,expected_queue,device_matches,distinct_pinned_producer));
        if(!out.sharp->valid){out.valid=false;out.reason=out.sharp->reason;}
        for(size_t i=0;i<out.choices.size();++i){out.keys[i].sharp_serial=out.sharp->keys[i].serial;
            const bool primary=out.choices[i];out.choices[i]=primary&&out.sharp->choices[i]&&out.keys[i].epoch==out.sharp->keys[i].epoch;
            if(primary&&!out.choices[i])out.choice_reasons[i]="sharp_layer_epoch_or_snapshot_unavailable";}
    }return out;
}
inline void commit_submission(const Submission& submitted,bool synchronized){
    std::lock_guard<std::mutex> lock(proof_mutex);if(submitted.revision!=proof_revision)return;
    if(submitted.sharp){sharp_proof.submitted=submitted.sharp->after;sharp_state_history=submitted.sharp->history;
        sharp_proof.valid=submitted.sharp->valid&&synchronized;sharp_proof.reason=synchronized?submitted.sharp->reason:"sharp_snapshot_signal_not_confirmed";sharp_queue_failure=submitted.sharp->failure;
        for(const auto key:submitted.sharp->lists)sharp_proof.pending.erase(key);}
    proof.submitted=submitted.after;state_history=submitted.history;proof.valid=submitted.valid&&synchronized;proof.reason=synchronized?submitted.reason:"snapshot_producer_signal_not_confirmed";queue_failure=submitted.failure;for(const auto key:submitted.lists)proof.pending.erase(key);
    if(!proof.valid){copy_source_verified=false;stop_reason="source_queue_or_state_proof_failed";return;}
    const bool ready_copy=proof.valid&&proof.submitted.owned_copy_source_verified&&(!paired_layers()||sharp_proof.submitted.owned_copy_source_verified);
    if(ready_copy&&!copy_source_verified.exchange(true))notify_pending=true;
}
inline void reset_list(uintptr_t list){std::lock_guard<std::mutex> lock(proof_mutex);proof.reset(list);sharp_proof.reset(list);owned_touches.erase(list);prepaint_frame_policy::reset(list);}
inline void source_choice(bool admitted,const char* reason="not_recorded"){
    if(!perf::reporting){
        if(admitted&&phase.load()==1&&!consumer_copy_admitted.load()&&!consumer_copy_admitted.exchange(true))notify_pending=true;
        return;
    }
    auto c=std::atomic_load(&comparison);const auto index=phase.load();if(!c||index<0)return;std::lock_guard<std::mutex> lock(c->mutex);if(c->finished)return;
    auto& m=c->phases[static_cast<size_t>(index)];if(admitted){if(!m.admitted_copies)notify_pending=true;++m.admitted_copies;}else ++m.fallback_copies;
    c->last_consumer_reason=reason;if(c->consumer_reasons.size()<32||c->consumer_reasons.count(reason))++c->consumer_reasons[reason];
}
// Graphics observation never takes the comparison lock while holding proof_mutex.
inline void diagnostics_json(std::ostream& out,const Comparison& c){
    auto target=selective_glow_target::get();auto binding=std::atomic_load(&source_binding);
    out<<"{\"widgetSelections\":[";for(size_t i=0;i<c.selections.size();++i){if(i)out<<',';paint::widget_policy::json(out,*c.selections[i]);}out<<"],\"gates\":{";for(unsigned i=0;i<c.gates.size();++i){if(i)out<<',';out<<selective_glow_inventory::quote(gate_names[i])<<':'<<c.gates[i];}
    out<<"},\"firstMatchedUiArgs\":[";for(unsigned i=0;i<c.first_ui_args.size();++i){if(i)out<<',';out<<c.first_ui_args[i];}
    out<<"],\"identityStopDetail\":"<<selective_glow_inventory::quote(c.identity_stop_detail)<<",\"batchArmedBeforeStop\":"<<(c.batch_armed_before_stop?"true":"false")<<",\"capturePresentBeforeStop\":"<<(c.capture_present_before_stop?"true":"false")
       <<",\"targetPublished\":"<<(target?"true":"false")<<",\"targetStatus\":"<<selective_glow_inventory::quote(selective_glow_target::status)
       <<",\"privateStencilStatus\":"<<selective_glow_inventory::quote(selective_glow_private_stencil::status)
       <<",\"probeQueued\":"<<(probe_queued.load()?"true":"false")<<",\"sourceWidth\":"<<(binding?binding->width:0)<<",\"sourceHeight\":"<<(binding?binding->height:0)
       <<",\"sourceRhi\":"<<(binding?binding->rhi:0)<<",\"ownedRhi\":"<<(target?target->rhi:0);
    {std::lock_guard<std::mutex> lock(proof_mutex);const auto& s=proof.submitted;size_t pending=0;for(const auto& p:proof.pending)pending+=p.second.size();
     out<<",\"pendingTransitionLists\":"<<proof.pending.size()<<",\"pendingTransitions\":"<<pending<<",\"proofValid\":"<<(proof.valid?"true":"false")
        <<",\"proofReason\":"<<selective_glow_inventory::quote(proof.valid?s.reason:proof.reason)<<",\"submittedUiState\":"<<s.ui_state<<",\"submittedOwnedState\":"<<s.owned_state
        <<",\"uiEpoch\":"<<s.ui_epoch<<",\"ownedEpoch\":"<<s.owned_epoch<<",\"probeEpoch\":"<<s.probe_epoch<<",\"ownedSerial\":"<<s.owned_serial;
     const auto& boundary=s.first_ui_boundary;out<<",\"hostBoundaryDiscardCount\":"<<s.host_boundary_discards<<",\"uiBoundaryResyncCount\":"<<s.ui_boundary_resyncs<<",\"firstUiBoundaryResync\":{\"recorded\":"<<(boundary.recorded?"true":"false")<<",\"expectedBefore\":"<<boundary.expected<<",\"observedBefore\":"<<boundary.before<<",\"observedAfter\":"<<boundary.after<<",\"discardedUiEpoch\":"<<boundary.epoch<<",\"discardedOwnedSerial\":"<<boundary.serial<<'}';
     const auto& mismatch=s.mismatch;out<<",\"stateMismatch\":{\"recorded\":"<<(mismatch.recorded?"true":"false")<<",\"image\":\""<<(mismatch.image==proof_types::Image::ui?"ui":"owned")<<"\",\"expectedBefore\":"<<mismatch.expected<<",\"observedBefore\":"<<mismatch.before<<",\"observedAfter\":"<<mismatch.after<<",\"uiEpoch\":"<<mismatch.epoch<<",\"ownedSerial\":"<<mismatch.serial<<",\"recentSubmissionOps\":[";
     for(size_t i=0;i<state_history.count;++i){if(i)out<<',';const auto& event=state_history.events[(state_history.cursor+state_history.events.size()-state_history.count+i)%state_history.events.size()];
        out<<"{\"queue\":"<<event.queue<<",\"list\":"<<event.list<<",\"ordinal\":"<<event.ordinal<<",\"image\":\""<<(event.op.image==proof_types::Image::ui?"ui":event.op.image==proof_types::Image::owned?"owned":"consumer")<<"\",\"before\":"<<event.op.before<<",\"after\":"<<event.op.after<<",\"uiBoundaryResync\":"<<(event.ui_boundary_resync?"true":"false")<<'}';}out<<"]}";
     const auto& f=queue_failure;out<<",\"queueFailure\":{\"recorded\":"<<(f.recorded?"true":"false")<<",\"actualQueue\":"<<f.actual<<",\"expectedQueue\":"<<f.expected<<",\"deviceMatches\":"<<(f.device_matches?"true":"false")<<",\"list\":"<<f.list<<",\"uiOps\":"<<f.ui_ops<<",\"ownedOps\":"<<f.owned_ops<<'}';
     out<<",\"consumerOrderWitnesses\":[";bool first=true;for(const auto& item:consumer_witnesses){if(!first)out<<',';first=false;const auto& w=item.second;
        out<<"{\"reason\":"<<selective_glow_inventory::quote(item.first)<<",\"list\":"<<w.list<<",\"entryUiState\":"<<w.entry.ui_state<<",\"entryOwnedState\":"<<w.entry.owned_state<<",\"entryUiEpoch\":"<<w.entry.ui_epoch<<",\"entryOwnedEpoch\":"<<w.entry.owned_epoch<<",\"entryOwnedSerial\":"<<w.entry.owned_serial<<",\"totalOps\":"<<w.total<<",\"firstOps\":[";
        for(size_t i=0;i<w.ops.size();++i){if(i)out<<',';const auto& op=w.ops[i];out<<"{\"image\":\""<<(op.image==proof_types::Image::ui?"ui":op.image==proof_types::Image::owned?"owned":"consumer")<<"\",\"before\":"<<op.before<<",\"after\":"<<op.after<<'}';}out<<"]}";
     }out<<']';
    }out<<",\"lastConsumerReason\":"<<selective_glow_inventory::quote(c.last_consumer_reason)<<",\"consumerReasons\":{";bool first_reason=true;for(const auto& item:c.consumer_reasons){if(!first_reason)out<<',';first_reason=false;out<<selective_glow_inventory::quote(item.first)<<':'<<item.second;}out<<"},\"nativeRouting\":";batch::route_json(out);out<<",\"nativeBarrierObserver\":";selective_glow_native_barriers::json(out);
    out<<",\"planningReasons\":{";bool first_plan=true;for(const auto& p:c.plan_reasons){if(!first_plan)out<<',';first_plan=false;out<<selective_glow_inventory::quote(p.first)<<':'<<p.second;}out<<"},\"clipLayoutVerified\":"<<(selective_glow_retained_clips::layout_ready?"true":"false")<<",\"clippingWitnesses\":{";first_plan=true;for(const auto& p:c.clip_witnesses){if(!first_plan)out<<',';first_plan=false;out<<selective_glow_inventory::quote(p.first)<<':';selective_glow_retained_clips::json(out,p.second);}out<<'}';
    {std::lock_guard<std::mutex> lock(proof_mutex);
    out<<",\"pairedSharpLayer\":"<<(paired_layers()?"true":"false")<<",\"sharpProof\":{\"valid\":"<<(sharp_proof.valid?"true":"false")
       <<",\"reason\":"<<selective_glow_inventory::quote(sharp_proof.reason)<<",\"uiEpoch\":"<<sharp_proof.submitted.ui_epoch<<",\"sharpEpoch\":"<<sharp_proof.submitted.owned_epoch
       <<",\"serial\":"<<sharp_proof.submitted.owned_serial<<",\"state\":"<<sharp_proof.submitted.owned_state
       <<",\"hostBoundaryDiscardCount\":"<<sharp_proof.submitted.host_boundary_discards<<",\"uiBoundaryResyncCount\":"<<sharp_proof.submitted.ui_boundary_resyncs
       <<",\"mismatchRecorded\":"<<(sharp_proof.submitted.mismatch.recorded?"true":"false")<<",\"expectedBefore\":"<<sharp_proof.submitted.mismatch.expected
       <<",\"observedBefore\":"<<sharp_proof.submitted.mismatch.before<<",\"observedAfter\":"<<sharp_proof.submitted.mismatch.after<<'}';}
    out<<",\"nativeExecutionObserver\":";selective_glow_execution::json(out);
    out<<",\"identityConstructorCalls\":"<<batch::identity_calls.load()<<",\"identityConstructorCpuUsTotal\":"<<batch::identity_ticks.load()*1000000.0/perf::frequency()
       <<",\"partiallyFilteredPasses\":"<<c.partially_filtered_passes<<",\"uncertainRetainedDraws\":"<<c.uncertain_retained_draws<<",\"firstPlanningWitnesses\":[";
    for(size_t i=0;i<c.plan_witnesses.size();++i){if(i)out<<',';const auto& w=c.plan_witnesses[i];
        out<<"{\"reason\":"<<selective_glow_inventory::quote(w.reason)<<",\"generation\":"<<w.generation<<",\"window\":"<<w.window<<",\"data\":"<<w.data<<",\"excluded\":"<<w.excluded<<",\"uncertainRetained\":"<<w.uncertain<<",\"row\":"<<w.row<<",\"mask\":"<<w.mask<<",\"verified\":"<<(w.verified?"true":"false")<<",\"rowBytes\":[";
        for(size_t j=0;j<w.raw.bytes.size();++j){if(j)out<<',';out<<unsigned(w.raw.bytes[j]);}out<<"]}";
    }out<<']';
    out<<",\"nativeExplicitState\":";selective_glow_native_state::json(out);out<<",\"snapshotHandoff\":";handoff.json(out);
    const auto& p=c.probe;out<<",\"probeCpuEvidence\":{\"entry\":"<<p.entry<<",\"refsReadable\":"<<(p.refs_read?"true":"false")<<",\"ownedRefsBefore\":"<<p.rhi_before<<",\"ownedRefsAfter\":"<<p.rhi_after<<",\"policyStrongBefore\":"<<p.policy_before<<",\"policyStrongAfter\":"<<p.policy_after<<",\"commandSnapshots\":[";
    bool first_snapshot=true;for(const auto* s:{&p.before,&p.after_pass,&p.after_transition}){if(!first_snapshot)out<<',';first_snapshot=false;out<<"{\"readable\":"<<(s->readable?"true":"false")<<",\"rootLinkArena\":["<<s->words[0]<<','<<s->words[1]<<','<<s->words[2]<<"]}";}out<<"]}}";
}
inline void save_metrics(const char* reason){
    if(!perf::reporting)return;
    auto c=std::atomic_load(&comparison);if(!c)return;std::lock_guard<std::mutex> lock(c->mutex);const auto hz=perf::frequency();std::ostringstream out;
    out<<"{\"version\":3,\"diagnosticBuild\":"<<selective_glow_inventory::quote(diagnostic_build)<<",\"processId\":"<<GetCurrentProcessId()<<",\"id\":"<<selective_glow_inventory::quote(c->id)<<",\"reason\":"<<selective_glow_inventory::quote(reason)<<",\"gpuTimingMeasured\":false,\"phases\":[";
    for(unsigned i=0;i<2;++i){if(i)out<<',';const auto& m=c->phases[i];out<<"{\"name\":\""<<(i?"identity_split":"current_rendering")<<"\",\"seconds\":"<<(m.wall_ticks+((static_cast<int>(i)==phase.load()&&!c->finished)?perf::sample_clock()-c->phase_begin:0))/hz<<",\"engineArrivalMs\":";perf::samples_json(out,m.engine,1000/hz);out<<",\"presentArrivalMs\":";perf::samples_json(out,m.present,1000/hz);out<<",\"originalNativeUiPassCpuUs\":";perf::samples_json(out,m.original_pass_cpu,1000000/hz);out<<",\"extraPrivatePassCpuUs\":";perf::samples_json(out,m.extra_pass_cpu,1000000/hz);out<<",\"privatePlanningCpuUs\":";perf::samples_json(out,m.planning_cpu,1000000/hz);out<<",\"snapshotHandoffCpuUs\":";perf::samples_json(out,m.handoff_cpu,1000000/hz);out<<",\"producerSnapshots\":"<<m.snapshots<<",\"snapshotConsumerWaits\":"<<m.consumer_waits;
        out<<",\"drawRoutingCpuUs\":";perf::samples_json(out,m.routing_cpu,1000000/hz);out<<",\"ownershipPreparationCpuUs\":";perf::samples_json(out,m.ownership_cpu,1000000/hz);
        out<<",\"consumerGenerationCpuUs\":";perf::samples_json(out,m.generation_cpu,1000000/hz);out<<",\"cleanBaseCpuUs\":";perf::samples_json(out,m.clean_base_cpu,1000000/hz);
        out<<",\"cleanBasePassesRecorded\":"<<m.clean_base_recordings<<",\"cleanBaseAllocationBytes\":"<<m.clean_base_allocation_bytes;
        out<<",\"plannedPasses\":"<<m.planned_passes<<",\"plannedGlowDraws\":"<<m.planned_draws<<",\"excludedDraws\":"<<m.excluded_draws<<",\"refusedPlans\":"<<m.refused_plans<<",\"admittedConsumerCopies\":"<<m.admitted_copies<<",\"fallbackConsumerCopies\":"<<m.fallback_copies<<'}';}
    out<<"],\"lastPlanReason\":"<<selective_glow_inventory::quote(last_reason.load())<<",\"diagnostics\":";diagnostics_json(out,*c);
    out<<",\"scope\":\"Current/split rendering with this diagnostic payload installed. Current mode disables identity tracking and the extra native pass. QPC native-pass/planning durations are CPU recording time; paired mode records a retained and a selected native pass, including two owned clear-only readiness probes; per-pass CPU samples are not per-frame totals. Engine/present arrivals are pacing proxies, not headset FPS or GPU duration. Startup, scene/radio differences and instrumentation affect comparisons. First8192 samples per series/phase. PageDown switches phases and continues recording; only Finish/reset/outside-flight stops and saves.\"}\n";
    std::ofstream(directory/("AC8_VR-SelectiveGlowSplitPerformance-"+c->id+".json"),std::ios::binary)<<out.str();
}
inline void state(){
    if(!perf::reporting){
        // Lua needs this acknowledgement to start and recover automatic glow.
        const int current_phase=phase.load();
        std::ofstream file(directory/"AC8_VR-SelectiveGlowSplit-state.json",std::ios::binary);
        file<<"{\"version\":3,\"ready\":"<<(ready?"true":"false")
            <<",\"phase\":"<<current_phase<<",\"recording\":"<<(current_phase>=0?"true":"false")
            <<",\"admittedCopies\":"<<(consumer_copy_admitted.load()?1:0)
            <<",\"reason\":"<<selective_glow_inventory::quote(last_reason.load())
            <<",\"endReason\":"<<selective_glow_inventory::quote(end_reason.load())<<"}\n";
        file.close();
        if(api&&api->functions&&api->functions->dispatch_lua_event)api->functions->dispatch_lua_event("AC8_SelectiveGlowSplitState","updated");
        return;
    }
    auto c=std::atomic_load(&comparison);uint64_t admitted=0,fallback=0,excluded=0;if(c){std::lock_guard<std::mutex> lock(c->mutex);admitted=c->phases[1].admitted_copies;fallback=c->phases[1].fallback_copies;excluded=c->phases[1].excluded_draws;}
    std::ostringstream out;out<<"{\"version\":3,\"diagnosticBuild\":"<<selective_glow_inventory::quote(diagnostic_build)<<",\"processId\":"<<GetCurrentProcessId()<<",\"ackId\":"<<selective_glow_inventory::quote(ack_id)<<",\"comparisonId\":"<<selective_glow_inventory::quote(c?c->id:std::string{})<<",\"ready\":"<<(ready?"true":"false")<<",\"phase\":"<<phase.load()<<",\"recording\":"<<(phase.load()>=0?"true":"false")<<",\"targetStatus\":"<<selective_glow_inventory::quote(selective_glow_target::status)<<",\"copySourceVerified\":"<<(copy_source_verified.load()?"true":"false")<<",\"admittedCopies\":"<<admitted<<",\"fallbackCopies\":"<<fallback<<",\"excludedDraws\":"<<excluded<<",\"reason\":"<<selective_glow_inventory::quote(last_reason.load())<<",\"endReason\":"<<selective_glow_inventory::quote(end_reason.load())<<",\"gpuTimingMeasured\":false";
    out<<",\"nativeExplicitStateReady\":"<<(selective_glow_native_state::ready.load()?"true":"false")<<",\"nativeExplicitStateStatus\":"<<selective_glow_inventory::quote(selective_glow_native_state::status.load());
    if(c){std::lock_guard<std::mutex> lock(c->mutex);out<<",\"diagnostics\":";diagnostics_json(out,*c);}out<<"}\n";
    {std::ofstream file(directory/"AC8_VR-SelectiveGlowSplit-state.json",std::ios::binary);file<<out.str();}
    if(api&&api->functions&&api->functions->dispatch_lua_event)api->functions->dispatch_lua_event("AC8_SelectiveGlowSplitState","updated");
}
inline void change_phase(int next){
    auto c=std::atomic_load(&comparison);const int old=phase.load();if(c){std::lock_guard<std::mutex> lock(c->mutex);const auto now=perf::sample_clock();if(old>=0)c->phases[static_cast<size_t>(old)].wall_ticks+=now-c->phase_begin;c->phase_begin=now;c->last_engine=c->last_present=0;}
    phase=next;selective_glow_native_barriers::active=next>=0;selective_glow_execution::active=next==1;selective_glow_native_state::active=next==1;if(next==1){if(!tracking_on()){phase=0;selective_glow_execution::active=false;selective_glow_native_state::active=false;last_reason="radio_identity_selection_refused";}}
    else tracking_off();notify_pending=true;
}
inline void stop(const char* reason){
    if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);c->batch_armed_before_stop=batch::armed.load();c->capture_present_before_stop=!!std::atomic_load(&batch::current);}
    const int old=phase.load();if(old>=0)change_phase(-1);batch::comparison_timing=false;end_reason=reason;
    if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);c->finished=true;}
    save_metrics(reason);notify_pending=true;
}
inline void engine_tick(bool allowed){
    cadence(false);int action=0;std::string id;{std::lock_guard<std::mutex> lock(request_mutex);action=pending_action;pending_action=0;id.swap(pending_id);}if(!id.empty())ack_id=id;
    if(const auto reason=stop_reason.exchange(nullptr))if(phase.load()>=0)stop(reason);
    if(!allowed&&phase.load()>=0)stop("outside_admitted_glow_flight");
    if(action==2){if(phase.load()>=0)stop("finished_by_user");}
    else if(action==3)notify_pending=true;
    else if(action==4){save_metrics("HOME_snapshot");notify_pending=true;}
    else if(action==1){
        if(handoff.failed()){last_reason=handoff.reason();notify_pending=true;}
        else if(!ready){last_reason="native_split_or_ownership_guards_unavailable";notify_pending=true;}
        else if(!allowed){last_reason="enter_glow_enabled_flight_first";notify_pending=true;}
        else if(phase.load()>=0)change_phase(phase.load()==1?0:1);
        else{
            auto binding=std::make_shared<SourceBinding>();const auto rhi=api->sdk->stereo_hook->get_ui_render_target();binding->rhi=reinterpret_cast<uintptr_t>(rhi);auto* native=rhi?static_cast<ID3D12Resource*>(api->sdk->frhitexture2d->get_native_resource(rhi)):nullptr;
            if(!native){last_reason="current_ui_source_unavailable";notify_pending=true;}
            else{
                const auto desc=native->GetDesc();binding->native=native;binding->width=static_cast<UINT>(desc.Width);binding->height=desc.Height;std::atomic_store(&source_binding,binding);
                {std::lock_guard<std::mutex> lock(proof_mutex);proof={};queue_failure={};state_history={};sharp_proof={};sharp_queue_failure={};sharp_state_history={};consumer_witnesses.clear();copy_source_verified=false;probe_observation_armed=false;++proof_revision;}probe_queued=false;admitted_context=0;admitted_view=0;last_roll_frame=0;preparing_ticks=0;
                consumer_copy_admitted=false;
                if(perf::reporting){auto c=std::make_shared<Comparison>();c->id=id;c->phase_begin=perf::sample_clock();std::atomic_store(&comparison,c);batch::identity_calls=0;batch::identity_ticks=0;batch::comparison_timing=true;}
                last_reason="preparing_private_target";end_reason="recording";selective_glow_native_barriers::session();selective_glow_execution::session();selective_glow_native_state::session();
                if(!selective_glow_target::create(binding->width,binding->height,true)){last_reason=selective_glow_target::status;change_phase(0);}else change_phase(1);
            }
        }
    }
    if(phase.load()>=0){
        if(selective_glow_native_barriers::service()){
            // The first probe can predate endpoint attachment. Discard that
            // incomplete observation and request one new OWNED clear cycle.
            {std::lock_guard<std::mutex> lock(proof_mutex);proof={};queue_failure={};state_history={};sharp_proof={};sharp_queue_failure={};sharp_state_history={};consumer_witnesses.clear();copy_source_verified=false;probe_observation_armed=false;++proof_revision;}
            copy_source_verified=false;probe_queued=false;preparing_ticks=0;notify_pending=true;
        }
        if(selective_glow_native_barriers::failed.load()){stop("native_barrier_observer_failed");}
        if(selective_glow_execution::failed.load()){stop("native_execution_observer_failed");}
        if(selective_glow_native_state::failed.load()){stop(selective_glow_native_state::status.load());}
        auto binding=std::atomic_load(&source_binding);if(binding){const auto rhi=api->sdk->stereo_hook->get_ui_render_target();
            if(reinterpret_cast<uintptr_t>(rhi)!=binding->rhi){stop("ui_source_changed");}
            else if(!selective_glow_target::get()&&selective_glow_target::publish(binding->rhi,static_cast<ID3D12Device*>(api->renderer->device))){notify_pending=true;}
        }
        if(phase.load()==1&&!copy_source_verified.load()&&++preparing_ticks>=240)stop("private_target_or_copy_source_probe_not_confirmed");
    }
    if(notify_pending.exchange(false))state();
}
inline void initialize(const UEVR_PluginInitializeParam* host){
    api=host;directory=paint::status_file.parent_path();selective_glow_target::initialize(host);
    ownership::initialize();
    selective_glow_retained_clips::initialize();selective_glow_private_stencil::initialize();selective_glow_retained_clips::stencil_ready=selective_glow_private_stencil::ready;
    selective_glow_native_barriers::observe=record_barriers;selective_glow_native_barriers::initialize(host);
    selective_glow_execution::initialize(host);
    selective_glow_native_state::initialize();
    selective_glow_execution::after_transition=selective_glow_native_state::apply;
    selective_glow_execution::needs_transition=[](){return selective_glow_native_state::active.load()&&selective_glow_native_state::ready.load()&&!selective_glow_native_state::failed.load()&&!selective_glow_execution::failed.load();};
    using namespace selective_glow_target_guards;
    ready=batch::ready&&selective_glow_target::guards_ready&&ownership::ready&&selective_glow_native_barriers::ready&&selective_glow_execution::ready&&selective_glow_native_state::ready&&selective_glow_target::guard(transition_rva,selective_glow_target_guards::transition);
    if(!ready){last_reason="identity_split_native_guards_unavailable";return;}
    transition=reinterpret_cast<Transition>(paint::image_base+ac8_game_compatibility::rva<transition_rva>());batch::continuous_prepare=prepare_pass;batch::continuous_pass=native_pass;batch::continuous_ui_copy=roll;
    batch::continuous_stopped=[](const char* reason){
        if(auto c=std::atomic_load(&comparison)){std::lock_guard<std::mutex> lock(c->mutex);c->identity_stop_detail=reason?reason:"unknown";}
        stop_reason="identity_tracking_lifetime_or_capacity_stop";
    };batch::original_pass_cpu=original_cpu;batch::draw_routing_cpu=[](uint64_t ticks){measure([&](Measurements& m){m.routing_cpu.add(ticks);});};
}
}
