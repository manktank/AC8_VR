#pragma once
#include "temporary-glow-pass-ownership.h"
// TEMP Plan C: explicitly armed bounded CPU routing evidence. Every original
// forwards once. No pixel tests, batch mutation, GPU commands, extra paint/tick.
#include "temporary-radio-paint-marker.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <intrin.h>
#include "temporary-glow-origin-guards.h"
namespace selective_glow_batch_capture {
using radio_paint_marker::pointer_field;
using selective_glow_inventory::read;
using selective_glow_inventory::quote;
constexpr unsigned selected=1,other=2,unknown=4;
struct Array {uintptr_t data=0;int32_t count=0,capacity=0;};
inline bool array(uintptr_t p,Array& a,unsigned limit=4096){return read(reinterpret_cast<void*>(p),a)&&a.count>=0&&a.capacity>=a.count&&a.capacity<=1000000&&static_cast<unsigned>(a.count)<=limit&&(!a.count||a.data);}
struct Raw {std::array<unsigned char,144> bytes{};};
template<class T>inline T field(const Raw& r,size_t offset){T v{};std::memcpy(&v,r.bytes.data()+offset,sizeof(v));return v;}
// Zero stride means unsupported/unverified, never an inferred classification.
inline constexpr std::array<size_t,14> strides{{0xa0,0xa0,0xd0,0x98,0x98,0,0x80,0x70,0xa0,0x60,0x88,0x80,0,0}};
struct Tag {uintptr_t window=0,container=0,owner=0;int32_t index=0;uint32_t layer=0;uint8_t type=0;unsigned roles=0,scope=0;bool snapshot=false;std::array<unsigned char,208> contents{};};
struct Node {unsigned packet=0,mask=unknown,roles=0;uintptr_t container=0;int32_t index=0;bool verified=false;};
struct Row {Raw raw;int32_t index=0;unsigned mask=unknown,roles=0;bool verified=false;};
struct SourceIdentity {uintptr_t container=0,data=0;int32_t count=0;uint8_t type=0;bool valid=false;std::vector<unsigned> roles;std::vector<unsigned char> contents;};
struct Build {uint64_t serial=0;bool consumed=false,header_read=false;std::array<unsigned char,96> header{};uintptr_t policy=0,graph=0,data=0,vertex_buffer=0,index_buffer=0;int32_t vertices=0,indices=0;DWORD thread=0;std::vector<Row> before,after;};
struct Packet {unsigned id=0,roles=0;uintptr_t window=0,batcher=0,main=0,hdr=0;DWORD thread=0;bool window_finished=false;std::vector<Build> builds;std::vector<SourceIdentity> sources;};
struct Merge {unsigned packet=0;uintptr_t data=0;int32_t first=-1,second=-1;unsigned first_mask=unknown,second_mask=unknown,result_mask=unknown;};
struct Pass {std::array<uintptr_t,12> args{};uintptr_t target_ref=0,secondary_ref=0,policy_ref=0;uint64_t qpc=0;bool selected_data=false;};
struct ConstructorSample {uintptr_t caller=0,container=0,result=0,element=0,source_array=0;std::array<uintptr_t,8> registers{};unsigned roles=0,source_mask=0;int32_t group_begin=-1,group_end=-1;bool unwind_ok=false,cached=false,ambiguous=false,group=false,verified_origin=false;};
struct Capture {
    prepaint_frame_policy::Frame frame_policy;
    std::shared_ptr<radio_paint_marker::Session> admission;
    std::shared_ptr<const radio_paint_marker::Roster> selection;
    std::string id,reason="waiting_for_radio_batch";
    std::mutex mutex;
    std::vector<Tag> tags;
    std::vector<Node> nodes;
    std::vector<Packet> packets;
    std::vector<Merge> merges;
    std::vector<Pass> passes;
    std::vector<ConstructorSample> constructors;
    unsigned constructor_calls=0,unwind_failures=0;
    bool continuous=false,routing_session=false;
    uint64_t generation=0;uintptr_t route_window=0;
    std::weak_ptr<Capture> route_root;
    std::shared_ptr<Capture> draw_source;
    const char* route_reason=nullptr;
    std::atomic<unsigned> paint_scopes{0};
    unsigned prevented_merges=0,prevented_groups=0;
    bool selected_ui_pass=false;
    unsigned selected_ui_roles=0;
    uintptr_t ui_rhi=0,ui_native=0,copy_list=0,copy_source=0;
    uint64_t copy_frame=0,copy_qpc=0;
    bool selected_ready=false;
    std::vector<uintptr_t> owned_windows;
    unsigned paint_sequence=0;
    unsigned ticks=0,init_calls=0,lookup_calls=0,copied=0,copy_mismatches=0,unresolved_elements=0,selected_builds=0;
    std::atomic<bool> done{false};bool overflow=false;
    std::atomic<unsigned> in_flight{0};
    explicit Capture(bool reserve_storage=true){if(reserve_storage){tags.reserve(1024);nodes.reserve(4096);packets.reserve(8);merges.reserve(1024);}}
    ~Capture(){if(draw_source)draw_source->in_flight.fetch_sub(1);}
};
inline std::shared_ptr<Capture> current;
inline std::atomic<bool> armed{false};
inline bool ready=false;
inline bool origin_guards_ready=false;
inline std::atomic<bool> split_filter_enabled{false};
inline void(*continuous_prepare)(const std::array<uintptr_t,12>&,selective_glow_ownership::Prepared&)=nullptr;
inline void(*continuous_pass)(const std::shared_ptr<Capture>&,const std::array<uintptr_t,12>&,selective_glow_ownership::Prepared&)=nullptr;
inline std::atomic<bool> comparison_timing{false};
inline bool comparisonReporting(){
#ifdef AC8_DISABLE_GLOW_REPORTING
    return false;
#else
    return comparison_timing.load(std::memory_order_relaxed);
#endif
}
inline std::atomic<uint64_t> identity_ticks{0},identity_calls{0};
struct IdentityTiming {
    uint64_t begin=comparisonReporting()?selective_glow_performance::clock():0;
    ~IdentityTiming(){if(begin){identity_ticks.fetch_add(selective_glow_performance::clock()-begin,std::memory_order_relaxed);identity_calls.fetch_add(1,std::memory_order_relaxed);}}
};
inline void(*original_pass_cpu)(uintptr_t,uint64_t)=nullptr;
inline void(*draw_routing_cpu)(uint64_t)=nullptr;
inline void(*continuous_ui_copy)(const std::shared_ptr<Capture>&,uint64_t)=nullptr;
inline void(*continuous_stopped)(const char*)=nullptr;
inline const char* readiness="not_initialized";
inline std::filesystem::path directory;
inline const UEVR_PluginInitializeParam* api=nullptr;
struct PaintContext {uintptr_t owner=0,window=0;unsigned scope=0;std::shared_ptr<Capture> capture;};
inline thread_local std::array<PaintContext,16> paint_stack{};
inline thread_local unsigned paint_depth=0;
inline thread_local unsigned packet_context=0;
inline thread_local uintptr_t build_context=0;
inline thread_local std::shared_ptr<Capture> packet_capture,build_capture;
struct Flight {std::shared_ptr<Capture> s;explicit Flight(std::shared_ptr<Capture> value):s(std::move(value)){if(s)s->in_flight.fetch_add(1);}~Flight(){if(s)s->in_flight.fetch_sub(1);}};
inline unsigned roles(const Capture& s,uintptr_t owner){return radio_paint_marker::widget_policy::roles(*s.admission,s.selection?s.selection:std::atomic_load(&s.admission->roster),owner);}
inline void capacity(Capture& s){s.overflow=s.done=true;s.reason="bounded_capacity_stop";}
inline Packet* packet(Capture& s,unsigned id){return id&&id<=s.packets.size()?&s.packets[id-1]:nullptr;}
inline Node* node(Capture& s,unsigned p,uintptr_t container,int32_t index){for(auto& n:s.nodes)if(n.packet==p&&n.container==container&&n.index==index)return &n;return nullptr;}
inline void tag_node(Capture& s,unsigned p,uintptr_t container,int32_t index,unsigned mask,unsigned body_roles,bool verified=false){
    if(auto* n=node(s,p,container,index)){n->mask|=mask;n->roles|=body_roles;n->verified=n->verified&&verified;return;}
    if(s.nodes.size()==4096){capacity(s);return;}s.nodes.push_back({p,mask,body_roles,container,index,verified});
}
inline int32_t index_of(uintptr_t container,uintptr_t value,size_t stride){Array a;if(!array(container,a)||value<a.data||!stride)return -1;const auto delta=value-a.data;return delta%stride==0&&delta/stride<static_cast<uintptr_t>(a.count)?static_cast<int32_t>(delta/stride):-1;}
#include "temporary-glow-routing.h"
inline void paint_enter(uintptr_t owner,uintptr_t window){
    if(paint_depth<paint_stack.size()){
        auto root=std::atomic_load(&current);auto s=root&&root->routing_session?route_paint(root,window):root;auto& context=paint_stack[paint_depth];context={};context.owner=owner;context.window=window;
        if(s){std::lock_guard<std::mutex> lock(s->mutex);if(!s->done){context.capture=s;context.scope=++s->paint_sequence;s->in_flight.fetch_add(1);s->paint_scopes.fetch_add(1);
            if(std::find(s->owned_windows.begin(),s->owned_windows.end(),window)==s->owned_windows.end()){if(s->owned_windows.size()<16)s->owned_windows.push_back(window);else capacity(*s);}
        }}
    }++paint_depth;
}
inline void paint_leave(){
    if(!paint_depth)return;--paint_depth;if(paint_depth>=paint_stack.size())return;
    auto& context=paint_stack[paint_depth];auto s=context.capture;
    if(s){
        {std::lock_guard<std::mutex> lock(s->mutex);if(!s->done)for(auto& t:s->tags)if(t.scope==context.scope){
            Array a;if(array(t.container,a)&&t.index<a.count){SIZE_T got=0;const auto address=a.data+static_cast<uintptr_t>(t.index)*strides[t.type];
                t.snapshot=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),t.contents.data(),strides[t.type],&got)&&got==strides[t.type];
            }
        }}s->paint_scopes.fetch_sub(1);s->in_flight.fetch_sub(1);
    }context={};
}
inline void radar_geometry(uintptr_t,const prepaint_frame_policy::Rect& rect,uintptr_t window){
    if(!paint_depth||paint_depth>paint_stack.size())return;
    const auto& context=paint_stack[paint_depth-1];auto s=context.capture;if(!s||context.window!=window)return;
    std::lock_guard<std::mutex> guard(s->mutex);prepaint_frame_policy::add(s->frame_policy,rect);
}
inline void observe_element(Capture& s,uintptr_t element,uintptr_t window,uint8_t type,uint32_t layer){
    if(!paint_depth||paint_depth>paint_stack.size()||paint_stack[paint_depth-1].window!=window||type>=14)return;
    const auto owner=paint_stack[paint_depth-1].owner;const auto body_roles=roles(s,owner);if(!body_roles)return;
    radio_paint_marker::widget_policy::count(s.admission->elements,body_roles);
    if(!strides[type]){++s.unresolved_elements;return;}
    Tag t;t.window=window;t.owner=owner;t.roles=body_roles;t.type=type;t.layer=layer;t.scope=paint_stack[paint_depth-1].scope;
    t.container=window+0x168+static_cast<uintptr_t>(type)*16;
    t.index=index_of(t.container,element,strides[type]);
    if(t.index<0){
        int32_t count=0;uintptr_t heap=0;
        if(read(reinterpret_cast<void*>(window+0x890),count)&&count>0&&count<=128&&pointer_field(window+0x888,heap)){
            const auto top=(heap?heap:window+0x248)+static_cast<uintptr_t>(count-1)*32;
            uintptr_t cached=0,control=0;int32_t strong=0;
            if(pointer_field(top,cached)&&pointer_field(top+8,control)&&read(reinterpret_cast<void*>(control+8),strong)&&strong>0){
                t.container=cached+static_cast<uintptr_t>(type)*16;t.index=index_of(t.container,element,strides[type]);
            }
        }
    }
    if(t.index<0){++s.unresolved_elements;return;}
    // Reuse/reset of a typed slot invalidates that prior slot's metadata.
    s.tags.erase(std::remove_if(s.tags.begin(),s.tags.end(),[&](const Tag& old){return old.container==t.container&&old.index>=t.index;}),s.tags.end());
    if(s.tags.size()==1024){capacity(s);return;}s.tags.push_back(t);
}
inline unsigned element_roles(const Capture& s,uintptr_t element,bool* unresolved=nullptr){
    uint8_t type=0;uint32_t layer=0;std::array<unsigned char,208> contents{};SIZE_T got=0;
    if(!read(reinterpret_cast<void*>(element+0x46),type)||!read(reinterpret_cast<void*>(element+0x28),layer)||type>=strides.size()||!strides[type]||
       !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(element),contents.data(),strides[type],&got)||got!=strides[type])return 0;
    unsigned result=0;for(const auto& t:s.tags)if(t.snapshot&&t.type==type&&t.layer==layer&&std::equal(contents.begin(),contents.begin()+strides[type],t.contents.begin())){
        // Sorting may move records. Match their full native bytes, and reject
        // duplicate records instead of guessing which owner's copy this is.
        Array a;unsigned matches=0;if(!array(t.container,a))continue;
        for(int32_t i=0;i<a.count;++i){std::array<unsigned char,208> candidate{};got=0;
            if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a.data+static_cast<uintptr_t>(i)*strides[type]),candidate.data(),strides[type],&got)&&got==strides[type]&&
               std::equal(contents.begin(),contents.begin()+strides[type],candidate.begin()))++matches;
        }
        if(matches==1)result|=t.roles;else if(unresolved)*unresolved=true;
    }
    return result;
}
using Init=void(*)(void*,void*,uint8_t,uint32_t,const void*,uint8_t);
using Window=void(*)(void*,void*);
using Lookup=void*(*)(void*,void*,uint32_t,const void*,void*,uint8_t,uint8_t,uint8_t,uint16_t,const void*);
using Copy=void(*)(void*,void*,void*);
using Combine=void(*)(void*,void*,void*,void*,void*);
using Buffers=void(*)(void*,void*,void*);
using DrawPass=void(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline Init original_init=nullptr;inline Window original_window=nullptr;inline Lookup original_lookup=nullptr;
inline Copy original_copy=nullptr;inline Combine original_combine=nullptr;inline Buffers original_buffers=nullptr;
inline DrawPass original_draw_pass=nullptr;
using Constructor=void*(*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline Constructor original_fresh=nullptr,original_cached=nullptr;
// Inlined text batching calls the constructors directly. Recover its preserved
// registers using standard Windows unwind metadata; never attach a debugger or
// alter the real thread context. Failed/ambiguous reconstruction stays unknown.
__declspec(noinline) inline bool caller_context(uintptr_t wanted,CONTEXT& context){
    RtlCaptureContext(&context);
    __try{for(unsigned i=0;i<8;++i){
        if(context.Rip==wanted)return true;
        DWORD64 base=0;const auto entry=RtlLookupFunctionEntry(context.Rip,&base,nullptr);
        if(entry){PVOID handler=nullptr;DWORD64 establisher=0;RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,entry,&context,&handler,&establisher,nullptr);}
        else{uintptr_t next=0;if(!pointer_field(context.Rsp,next))return false;context.Rip=next;context.Rsp+=8;}
    }}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
inline bool origin_array(const Capture& s,const ConstructorSample& sample,uintptr_t object,uintptr_t source_array,unsigned type){
    const auto* p=packet_context&&packet_context<=s.packets.size()?&s.packets[packet_context-1]:nullptr;
    if(!p)return false;
    if(sample.cached){
        uintptr_t parent=0;Array lists;
        if(source_array!=object+static_cast<uintptr_t>(type)*16||!pointer_field(object+0xf8,parent)||!parent||!array(parent+0x38,lists,128))return false;
        for(int32_t i=0;i<lists.count;++i){uintptr_t candidate=0,control=0;int32_t strong=0;const auto ref=lists.data+static_cast<uintptr_t>(i)*16;
            if(pointer_field(ref,candidate)&&candidate==object&&pointer_field(ref+8,control)&&control&&read(reinterpret_cast<void*>(control+8),strong)&&strong>0)return true;
        }return false;
    }
    return (object==p->main||object==p->hdr)&&source_array==p->window+0x168+static_cast<uintptr_t>(type)*16;
}
inline SourceIdentity* source_identity(Capture& s,uintptr_t container,uint8_t type){
    auto* p=packet(s,packet_context);if(!p||type>=strides.size()||!strides[type])return nullptr;
    Array a;if(!array(container,a))return nullptr;
    for(auto& v:p->sources)if(v.container==container&&v.type==type)return v.valid&&v.data==a.data&&v.count==a.count?&v:nullptr;
    if(p->sources.size()==28)return nullptr;
    SourceIdentity v;v.container=container;v.type=type;v.data=a.data;v.count=a.count;v.roles.resize(static_cast<size_t>(a.count));
    v.contents.resize(static_cast<size_t>(a.count)*strides[type]);SIZE_T got=0;
    bool okay=!v.contents.empty()&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a.data),v.contents.data(),v.contents.size(),&got)&&got==v.contents.size();
    for(const auto& tag:s.tags)if(tag.container==container&&tag.type==type){
        if(!tag.snapshot){okay=false;break;}
        unsigned matches=0;int32_t found=-1;
        for(int32_t i=0;okay&&i<a.count;++i)if(std::equal(tag.contents.begin(),tag.contents.begin()+strides[type],v.contents.begin()+static_cast<size_t>(i)*strides[type])){++matches;found=i;}
        if(matches!=1){okay=false;break;}v.roles[static_cast<size_t>(found)]|=tag.roles;
    }
    v.valid=okay;p->sources.push_back(std::move(v));return p->sources.back().valid?&p->sources.back():nullptr;
}
// This exact consumer pre-groups boxes BEFORE constructing a batch. Read the
// real range and every member; layer/material/position/color never select it.
inline void box_group_origin(Capture& s,ConstructorSample& sample,const CONTEXT& context,uintptr_t object){
    sample.group=true;sample.source_mask=unknown;sample.source_array=context.R13;
    Array a;int32_t begin=-1;const auto end=context.R14;
    if(!origin_guards_ready||!origin_array(s,sample,object,context.R13,0)||!array(context.R13,a)||
       context.R15!=end||end>static_cast<uint64_t>(a.count)||!end||
       !read(reinterpret_cast<void*>(context.Rsp+0x74),begin)||begin<0||static_cast<uint64_t>(begin)>=end)return;
    sample.group_begin=begin;sample.group_end=static_cast<int32_t>(end);sample.source_mask=0;
    if(s.continuous){
        auto* native=source_identity(s,context.R13,0);if(!native){sample.source_mask=unknown;return;}
        for(int32_t i=begin;i<sample.group_end;++i){const auto found=native->roles[static_cast<size_t>(i)];sample.roles|=found;sample.source_mask|=found?selected:other;}
        sample.verified_origin=true;return;
    }
    for(int32_t i=begin;i<sample.group_end;++i){
        const auto element=a.data+static_cast<uintptr_t>(i)*strides[0];std::array<unsigned char,0xa0> value{};
        if(!read(reinterpret_cast<void*>(element),value)||value[0x46]!=0){sample.source_mask|=unknown;return;}
        bool ambiguous=false;const auto found=element_roles(s,element,&ambiguous);
        sample.roles|=found;sample.ambiguous=sample.ambiguous||ambiguous;
        sample.source_mask|=(found?selected:other)|(ambiguous?unknown:0);
        if(found)sample.element=element;
    }
    sample.verified_origin=true;
}
inline void text_origin(Capture& s,ConstructorSample& sample,const CONTEXT& context,uintptr_t object){
    sample.source_mask=unknown;sample.element=context.R12;
    const auto* p=packet_context&&packet_context<=s.packets.size()?&s.packets[packet_context-1]:nullptr;
    if(!p||!origin_guards_ready)return;
    sample.source_array=sample.cached?object+3*16:p->window+0x168+3*16;
    if(!origin_array(s,sample,object,sample.source_array,3))return;
    std::array<unsigned char,0x98> value{};
    if(!read(reinterpret_cast<void*>(sample.element),value)||value[0x46]!=3)return;
    if(s.continuous){
        auto* native=source_identity(s,sample.source_array,3);if(!native)return;
        unsigned matches=0;int32_t index=-1;
        for(int32_t i=0;i<native->count;++i)if(std::equal(value.begin(),value.end(),native->contents.begin()+static_cast<size_t>(i)*strides[3])){++matches;index=i;}
        if(matches!=1)return;sample.roles=native->roles[static_cast<size_t>(index)];sample.source_mask=sample.roles?selected:other;sample.verified_origin=true;return;
    }
    if(index_of(sample.source_array,sample.element,strides[3])<0)return;
    sample.roles=element_roles(s,sample.element,&sample.ambiguous);
    sample.source_mask=(sample.roles?selected:other)|(sample.ambiguous?unknown:0);sample.verified_origin=true;
}
inline ConstructorSample constructor_origin(const std::shared_ptr<Capture>& s,uintptr_t pc,bool cached,uintptr_t object){
    ConstructorSample result;result.caller=pc;result.cached=cached;CONTEXT context{};
    result.unwind_ok=caller_context(pc,context);
    if(!s||!result.unwind_ok)return result;
    result.registers={context.Rbx,context.Rbp,context.Rsi,context.Rdi,context.R12,context.R13,context.R14,context.R15};
    std::lock_guard<std::mutex> lock(s->mutex);if(s->done)return result;
    if(pc==radio_paint_marker::image_base+(cached?ac8_game_compatibility::rva<0x1c211f2>():ac8_game_compatibility::rva<0x1c211fc>())){box_group_origin(*s,result,context,object);return result;}
    if(pc==radio_paint_marker::image_base+(cached?ac8_game_compatibility::rva<0x1c21e4e>():ac8_game_compatibility::rva<0x1c21e72>())){text_origin(*s,result,context,object);return result;}
    for(size_t i=0;i<result.registers.size();++i){const auto value=result.registers[i];if(value<0x10000)continue;
        bool duplicate=false;for(size_t j=0;j<i;++j)duplicate=duplicate||result.registers[j]==value;if(duplicate)continue;
        bool ambiguous=false;const auto found=element_roles(*s,value,&ambiguous);result.ambiguous=result.ambiguous||ambiguous;
        if(found){result.roles|=found;result.element=value;}
    }return result;
}
inline void constructor_result(Capture& s,ConstructorSample sample,uintptr_t object,void* value){
    ++s.constructor_calls;if(!sample.unwind_ok)++s.unwind_failures;sample.result=reinterpret_cast<uintptr_t>(value);sample.container=object;
    if(sample.cached){uintptr_t owner=0;pointer_field(object+0xf0,owner);
        prepaint_frame_policy::Rect radar;
        if(s.admission&&radio_paint_marker::widget_policy::live_cached_radar(*s.admission,owner,radar))prepaint_frame_policy::add(s.frame_policy,radar);
        // An outer cache owner is not every element's owner. Preserve the exact
        // typed-array proof (including mixed groups) instead of overwriting it.
        if(s.continuous){const auto owned=roles(s,owner);
            if(owned&&sample.source_mask!=(selected|other)){sample.roles|=owned;sample.source_mask=selected;sample.verified_origin=true;}
            else if(!sample.verified_origin){sample.source_mask|=unknown;sample.verified_origin=false;}}
        else if(!sample.source_mask)sample.roles|=roles(s,owner);
        if(!pointer_field(object+0xf8,sample.container))return;}
    const auto index=index_of(sample.container,sample.result,144);
    if(index>=0){const auto mask=sample.source_mask?sample.source_mask:(sample.roles?selected:((!sample.unwind_ok||sample.ambiguous||s.unresolved_elements)?unknown:other));
        tag_node(s,packet_context,sample.container,index,mask,sample.roles,sample.verified_origin);if(auto* p=packet(s,packet_context))p->roles|=sample.roles;
    }
    if(!s.continuous&&s.constructors.size()<512)s.constructors.push_back(sample);
}
__declspec(noinline) inline void* fresh(uintptr_t a1,uintptr_t a2,uintptr_t a3,uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7,uintptr_t a8,uintptr_t a9){
    if(!armed.load(std::memory_order_relaxed)||!packet_context)return original_fresh(a1,a2,a3,a4,a5,a6,a7,a8,a9);
    Flight f(packet_capture?packet_capture:std::atomic_load(&current));ConstructorSample origin;
    {IdentityTiming timing;origin=constructor_origin(f.s,reinterpret_cast<uintptr_t>(_ReturnAddress()),false,a1);}
    auto* result=original_fresh(a1,a2,a3,a4,a5,a6,a7,a8,a9);
    if(f.s){std::lock_guard<std::mutex> lock(f.s->mutex);if(!f.s->done)constructor_result(*f.s,origin,a1,result);}return result;
}
__declspec(noinline) inline void* cached(uintptr_t a1,uintptr_t a2,uintptr_t a3,uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7,uintptr_t a8,uintptr_t a9){
    if(!armed.load(std::memory_order_relaxed)||!packet_context)return original_cached(a1,a2,a3,a4,a5,a6,a7,a8,a9);
    Flight f(packet_capture?packet_capture:std::atomic_load(&current));ConstructorSample origin;
    {IdentityTiming timing;origin=constructor_origin(f.s,reinterpret_cast<uintptr_t>(_ReturnAddress()),true,a1);}
    auto* result=original_cached(a1,a2,a3,a4,a5,a6,a7,a8,a9);
    if(f.s){std::lock_guard<std::mutex> lock(f.s->mutex);if(!f.s->done)constructor_result(*f.s,origin,a1,result);}return result;
}
inline void init(void* e,void* list,uint8_t type,uint32_t layer,const void* paint_geometry,uint8_t effects){
    original_init(e,list,type,layer,paint_geometry,effects);
    if(!armed.load(std::memory_order_relaxed)||!paint_depth||paint_depth>paint_stack.size())return;Flight f(paint_stack[paint_depth-1].capture);if(!f.s)return;
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done)return;++f.s->init_calls;observe_element(*f.s,reinterpret_cast<uintptr_t>(e),reinterpret_cast<uintptr_t>(list),type,layer);
}
inline bool cached_window(const Capture& s,uintptr_t window){
    uintptr_t heap=0;int32_t count=0;
    if(!pointer_field(window+0x8b8,heap)||!read(reinterpret_cast<void*>(window+0x8c0),count)||count<0||count>32||(!heap&&count>4))return false;
    for(int32_t i=0;i<count;++i){uintptr_t parent=0;if(!pointer_field((heap?heap:window+0x898)+static_cast<uintptr_t>(i)*8,parent))continue;
        Array lists;if(!array(parent+0x38,lists,512))continue;
        for(int32_t j=0;j<lists.count;++j){uintptr_t cached=0,control=0,owner=0;int32_t strong=0;
            const auto ref=lists.data+static_cast<uintptr_t>(j)*16;
            if(pointer_field(ref,cached)&&pointer_field(ref+8,control)&&read(reinterpret_cast<void*>(control+8),strong)&&strong>0&&pointer_field(cached+0xf0,owner)&&roles(s,owner))return true;
        }
    }return false;
}
inline void window(void* batcher,void* list){
    if(!armed.load(std::memory_order_relaxed)){original_window(batcher,list);return;}
    auto root=std::atomic_load(&current);const auto native_window=reinterpret_cast<uintptr_t>(list);
    const bool routed=root&&root->routing_session;
    const bool cached_selected=routed&&!route_has_paint(root,native_window)&&cached_window(*root,native_window);
    Flight f(routed?route_window(root,native_window,cached_selected):root);unsigned id=0;
    if(f.s){std::lock_guard<std::mutex> lock(f.s->mutex);if(!f.s->done&&(f.s->continuous||std::find(f.s->owned_windows.begin(),f.s->owned_windows.end(),reinterpret_cast<uintptr_t>(list))!=f.s->owned_windows.end()||cached_window(*f.s,reinterpret_cast<uintptr_t>(list)))){
        if(f.s->packets.size()==8)capacity(*f.s);else{Packet p;p.id=static_cast<unsigned>(f.s->packets.size()+1);p.window=reinterpret_cast<uintptr_t>(list);p.batcher=reinterpret_cast<uintptr_t>(batcher);p.main=p.window+0x48;p.hdr=p.window+0xa8;p.thread=GetCurrentThreadId();id=p.id;f.s->packets.push_back(std::move(p));}
    }}
    const auto previous=packet_context;auto previous_capture=packet_capture;packet_context=id;packet_capture=f.s;original_window(batcher,list);packet_context=previous;packet_capture=std::move(previous_capture);
    if(f.s&&id){std::lock_guard<std::mutex> lock(f.s->mutex);if(auto* p=packet(*f.s,id))p->window_finished=true;}
}
inline void* lookup(void* batcher,void* data,uint32_t layer,const void* params,void* resource,uint8_t primitive,uint8_t shader,uint8_t effects,uint16_t flags,const void* element){
    auto* result=original_lookup(batcher,data,layer,params,resource,primitive,shader,effects,flags,element);
    if(!armed.load(std::memory_order_relaxed)||!packet_context)return result;Flight f(packet_capture?packet_capture:std::atomic_load(&current));if(!f.s)return result;
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done)return result;++f.s->lookup_calls;
    auto* p=packet(*f.s,packet_context);if(!p||p->batcher!=reinterpret_cast<uintptr_t>(batcher))return result;
    bool ambiguous=false;auto body_roles=element_roles(*f.s,reinterpret_cast<uintptr_t>(element),&ambiguous);uintptr_t cached=0,container=reinterpret_cast<uintptr_t>(data),owner=0;
    pointer_field(reinterpret_cast<uintptr_t>(batcher)+0x10,cached);
    if(cached){pointer_field(cached+0xf0,owner);body_roles|=roles(*f.s,owner);if(!pointer_field(cached+0xf8,container))return result;}
    const auto index=index_of(container,reinterpret_cast<uintptr_t>(result),144);
    if(index>=0){tag_node(*f.s,p->id,container,index,body_roles?selected:((ambiguous||f.s->unresolved_elements)?unknown:other),body_roles);p->roles|=body_roles;}
    return result;
}
struct Source {int32_t index=0;Raw raw;unsigned mask=unknown,roles=0;bool verified=false;};
inline std::vector<Source> cache_sources(Capture& s,unsigned p,uintptr_t parent){
    Array slots,lists;if(!array(parent,slots,2048)||!array(parent+0x38,lists,512))return {};
    uintptr_t heap=0;int32_t bits=0;if(!pointer_field(parent+0x20,heap)||!read(reinterpret_cast<void*>(parent+0x28),bits)||bits<slots.count||bits>1000000||(!heap&&bits>128))return {};
    std::vector<Source> values;
    for(int32_t i=0;i<slots.count;++i){uint32_t word=0;
        if(!read(reinterpret_cast<void*>((heap?heap:parent+0x10)+static_cast<uintptr_t>(i/32)*4),word)||!(word&(1u<<(i%32))))continue;
        Source v;v.index=i;if(!read(reinterpret_cast<void*>(slots.data+static_cast<uintptr_t>(i)*144),v.raw))continue;
        if(auto* n=node(s,p,parent,i)){v.mask=n->mask;v.roles=n->roles;v.verified=n->verified;}
        unsigned owner_masks=0,owner_roles=0;
        for(int32_t j=0;j<lists.count;++j){uintptr_t cached=0,control=0,owner=0,actual_parent=0;int32_t strong=0;Array indexes;
            const auto ref=lists.data+static_cast<uintptr_t>(j)*16;
            if(!pointer_field(ref,cached)||!pointer_field(ref+8,control)||!read(reinterpret_cast<void*>(control+8),strong)||strong<=0||
               !pointer_field(cached+0xf8,actual_parent)||actual_parent!=parent||!pointer_field(cached+0xf0,owner)||!array(cached+0xe0,indexes,2048))continue;
            for(int32_t k=0;k<indexes.count;++k){int32_t slot=-1;if(read(reinterpret_cast<void*>(indexes.data+static_cast<uintptr_t>(k)*4),slot)&&slot==i){const auto r=roles(s,owner);owner_roles|=r;owner_masks|=r?selected:other;break;}}
        }
        if(owner_masks==selected&&v.mask!=(selected|other)){v.roles|=owner_roles;v.mask=selected;v.verified=true;}
        else if(!v.verified&&owner_masks){v.roles|=owner_roles;v.mask=v.mask==unknown?owner_masks:v.mask|owner_masks;}
        // A verified constructor identity remains valid through this exact
        // sparse-slot copy even when the outer cache owner is unselected.
        // Never promote an absent/unselected owner into a verified identity.
        values.push_back(v);
    }
    return values;
}
inline void copy(void* a,void* b,void* source_data){
    if(!armed.load(std::memory_order_relaxed)||!packet_context){original_copy(a,b,source_data);return;}
    Flight f(packet_capture?packet_capture:std::atomic_load(&current));Array before_a,before_b;std::vector<Source> sources;
    const auto main=reinterpret_cast<uintptr_t>(a),hdr=reinterpret_cast<uintptr_t>(b),parent=reinterpret_cast<uintptr_t>(source_data);
    if(f.s){std::lock_guard<std::mutex> lock(f.s->mutex);if(!f.s->done&&array(main,before_a)&&array(hdr,before_b))sources=cache_sources(*f.s,packet_context,parent);}
    original_copy(a,b,source_data);
    if(!f.s||sources.empty())return;
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done)return;Array after_a,after_b;
    if(!array(main,after_a)||!array(hdr,after_b))return;
    int32_t na=before_a.count,nb=before_b.count;
    for(const auto& v:sources){const bool is_hdr=(v.raw.bytes[0x8b]&1)!=0;const auto container=is_hdr?hdr:main;const auto& out=is_hdr?after_b:after_a;auto& next=is_hdr?nb:na;Raw actual;
        if(next>=out.count||!read(reinterpret_cast<void*>(out.data+static_cast<uintptr_t>(next)*144),actual)||actual.bytes!=v.raw.bytes){++f.s->copy_mismatches;++next;continue;}
        tag_node(*f.s,packet_context,container,next++,v.mask,v.roles,v.verified);++f.s->copied;if(auto* p=packet(*f.s,packet_context))p->roles|=v.roles;
    }
}
inline void combine(void* opaque,void* first,void* second,void* vertices,void* indices){
    auto s=armed.load(std::memory_order_relaxed)?(build_capture?build_capture:std::atomic_load(&current)):nullptr;Flight f(s);Merge m;bool observed=false;
    if(s&&build_context){std::lock_guard<std::mutex> lock(s->mutex);if(!s->done){
        for(const auto& p:s->packets)if(p.main==build_context||p.hdr==build_context){m.packet=p.id;break;}
        m.data=build_context;m.first=index_of(m.data,reinterpret_cast<uintptr_t>(first),144);m.second=index_of(m.data,reinterpret_cast<uintptr_t>(second),144);
        if(m.packet&&m.first>=0&&m.second>=0){const auto* a=node(*s,m.packet,m.data,m.first);const auto* b=node(*s,m.packet,m.data,m.second);m.first_mask=a?a->mask:unknown;m.second_mask=b?b->mask:unknown;m.result_mask=m.first_mask|m.second_mask;observed=true;}
    }}
    original_combine(opaque,first,second,vertices,indices);
    if(observed){std::lock_guard<std::mutex> lock(s->mutex);if(s->done)return;unsigned body_roles=0;bool verified=true;
        if(auto* n=node(*s,m.packet,m.data,m.first)){body_roles|=n->roles;verified=verified&&n->verified;}else verified=false;
        if(auto* n=node(*s,m.packet,m.data,m.second)){body_roles|=n->roles;verified=verified&&n->verified;}else verified=false;
        if(auto* n=node(*s,m.packet,m.data,m.first)){n->mask=m.result_mask;n->roles=body_roles;n->verified=verified;}
        else tag_node(*s,m.packet,m.data,m.first,m.result_mask,body_roles,verified);
        if(s->merges.size()==1024)capacity(*s);else s->merges.push_back(m);}
}
inline std::vector<Row> rows(Capture& s,unsigned p,uintptr_t data){
    Array a;std::vector<Row> result;if(!array(data,a,2048))return result;result.reserve(static_cast<size_t>(a.count));
    for(int32_t i=0;i<a.count;++i){Row r;r.index=i;if(!read(reinterpret_cast<void*>(a.data+static_cast<uintptr_t>(i)*144),r.raw))return {};
        if(const auto* n=node(s,p,data,i)){r.mask=n->mask;r.roles=n->roles;r.verified=n->verified;}result.push_back(r);}
    return result;
}
inline void buffers(void* policy,void* graph,void* data){
    if(!armed.load(std::memory_order_relaxed)){original_buffers(policy,graph,data);return;}
    auto root=std::atomic_load(&current);Flight f(root&&root->routing_session?route_upload(root,reinterpret_cast<uintptr_t>(data)):root);Build b;unsigned id=0;
    b.data=reinterpret_cast<uintptr_t>(data);b.policy=reinterpret_cast<uintptr_t>(policy);b.graph=reinterpret_cast<uintptr_t>(graph);b.thread=GetCurrentThreadId();
    if(f.s){std::lock_guard<std::mutex> lock(f.s->mutex);if(!f.s->done)for(auto it=f.s->packets.rbegin();it!=f.s->packets.rend();++it)if(it->window_finished&&(it->main==b.data||it->hdr==b.data)){id=it->id;b.before=rows(*f.s,id,b.data);break;}}
    const auto prior=build_context;auto prior_capture=build_capture;build_context=id?b.data:0;build_capture=f.s;original_buffers(policy,graph,data);build_context=prior;build_capture=std::move(prior_capture);
    if(!f.s||!id)return;
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done)return;
    b.serial=++route_build_serial;b.header_read=read(reinterpret_cast<void*>(b.data),b.header);b.after=rows(*f.s,id,b.data);read(reinterpret_cast<void*>(b.data+0x38),b.vertices);read(reinterpret_cast<void*>(b.data+0x48),b.indices);
    pointer_field(b.policy+0x30,b.vertex_buffer);pointer_field(b.policy+0x58,b.index_buffer);
    bool has_selected=false;for(const auto& r:b.before)has_selected=has_selected||((r.mask&selected)!=0&&field<int32_t>(r.raw,0x74)>0&&field<int32_t>(r.raw,0x78)>0);
    has_selected=has_selected&&b.vertices>0&&b.indices>0;
    if(auto* p=packet(*f.s,id)){if(p->builds.size()==4){capacity(*f.s);return;}p->builds.push_back(std::move(b));
        if(has_selected){++f.s->selected_builds;if(p->roles==((1u<<f.s->admission->count)-1)||f.s->selected_builds>=2)f.s->selected_ready=true;}
    }
}
inline void draw_pass(uintptr_t a1,uintptr_t a2,uintptr_t a3,uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7,uintptr_t a8,uintptr_t a9,uintptr_t a10,uintptr_t a11,uintptr_t a12){
    const std::array<uintptr_t,12> args{{a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12}};
    const auto route_begin=comparisonReporting()?selective_glow_performance::clock():0;
    auto root=armed.load(std::memory_order_relaxed)?std::atomic_load(&current):std::shared_ptr<Capture>{};
    if(root&&root->routing_session)route_observe_ui(root,a1,a6);
    Flight f(root&&root->routing_session?route_draw(root,a6):root);
    if(route_begin&&draw_routing_cpu)draw_routing_cpu(selective_glow_performance::clock()-route_begin);
    selective_glow_ownership::Prepared prepared;uintptr_t target_value=0,secondary_value=0,policy_value=0;
    if(f.s||comparisonReporting())pointer_field(a1,target_value);
    if(f.s){pointer_field(a12,secondary_value);pointer_field(a11,policy_value);}
    if(f.s&&f.s->continuous&&continuous_prepare){try{continuous_prepare(args,prepared);}catch(...){prepared.policy.reset();}}
    const auto begin=comparisonReporting()?selective_glow_performance::clock():0;
    original_draw_pass(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12);
    if(begin&&original_pass_cpu)original_pass_cpu(target_value,selective_glow_performance::clock()-begin);
    if(!f.s)return;
    if(f.s->continuous){if(continuous_pass)continuous_pass(f.s,args,prepared);return;}
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done)return;bool matches=false;
    for(const auto& p:f.s->packets)matches=matches||p.main==a6||p.hdr==a6;
    if(!matches)return;if(f.s->passes.size()==16){capacity(*f.s);return;}
    Pass p;p.args=args;p.target_ref=target_value;p.secondary_ref=secondary_value;p.policy_ref=policy_value;
    for(const auto& packet_value:f.s->packets)for(const auto& built:packet_value.builds)if(built.data==a6)for(const auto& row:built.before)if((row.mask&selected)&&field<int32_t>(row.raw,0x74)>0&&field<int32_t>(row.raw,0x78)>0)p.selected_data=true;
    if(p.selected_data&&f.s->ui_rhi&&p.target_ref==f.s->ui_rhi){f.s->selected_ui_pass=true;
        for(const auto& packet_value:f.s->packets)for(const auto& built:packet_value.builds)if(built.data==a6)for(const auto& row:built.before)
            if((row.mask&selected)&&field<int32_t>(row.raw,0x74)>0&&field<int32_t>(row.raw,0x78)>0)f.s->selected_ui_roles|=row.roles;
    }
    p.qpc=selective_glow_performance::clock();f.s->passes.push_back(p);
}
inline void hud_copy(uintptr_t list,uintptr_t native_source,uint64_t xr_frame){
    if(!armed.load(std::memory_order_relaxed))return;Flight f(std::atomic_load(&current));if(!f.s)return;
    if(f.s->continuous){if(continuous_ui_copy&&native_source==f.s->ui_native)continuous_ui_copy(f.s,xr_frame);return;}
    std::lock_guard<std::mutex> lock(f.s->mutex);if(f.s->done||!f.s->selected_ready||!f.s->selected_ui_pass||f.s->selected_ui_roles!=((1u<<f.s->admission->count)-1))return;
    if(f.s->ui_native&&f.s->ui_native!=native_source){f.s->done=true;f.s->reason="ui_resource_changed_during_capture";return;}
    f.s->copy_list=list;f.s->copy_source=native_source;f.s->copy_frame=xr_frame;f.s->copy_qpc=selective_glow_performance::clock();
    f.s->done=true;f.s->reason="native_batch_and_ui_copy_saved";
}
inline void ui_at_arm(Capture& s){
    // Same SDK texture path used by the accepted renderer; engine-thread request
    // only, never an SDK call from the batch/RDG/D3D observers.
    if(!api||!api->sdk||!api->sdk->stereo_hook||!api->sdk->frhitexture2d||!api->sdk->stereo_hook->get_ui_render_target||!api->sdk->frhitexture2d->get_native_resource)return;
    __try{auto texture=api->sdk->stereo_hook->get_ui_render_target();s.ui_rhi=reinterpret_cast<uintptr_t>(texture);
        if(texture)s.ui_native=reinterpret_cast<uintptr_t>(api->sdk->frhitexture2d->get_native_resource(texture));
    }__except(EXCEPTION_EXECUTE_HANDLER){s.ui_rhi=s.ui_native=0;}
}
inline std::string hex(const Raw& r){std::ostringstream out;out<<std::hex<<std::setfill('0');for(const auto b:r.bytes)out<<std::setw(2)<<unsigned(b);return out.str();}
inline void row_json(std::ostream& out,const Row& r){out<<"{\"index\":"<<r.index<<",\"ownerMask\":"<<r.mask<<",\"radioRoles\":"<<r.roles<<",\"nativeLayer68\":"<<field<int32_t>(r.raw,0x68)<<",\"nativeVertexCount74\":"<<field<int32_t>(r.raw,0x74)<<",\"nativeIndexCount78\":"<<field<int32_t>(r.raw,0x78)<<",\"raw144\":"<<quote(hex(r.raw))<<'}';}
inline std::string report(const Capture& s){
    std::ostringstream out;out<<"{\"version\":1,\"id\":"<<quote(s.id)<<",\"reason\":"<<quote(s.reason)<<",\"readOnlyBatchObservation\":true,\"separateRenderingImplemented\":false,\"gpuCommandsAdded\":false,\"colorOrShapeSelection\":false,\"ticks\":"<<s.ticks<<",\"overflow\":"<<(s.overflow?"true":"false")<<",\"ownedElementInitializations\":"<<s.init_calls<<",\"unresolvedOwnedElements\":"<<s.unresolved_elements<<",\"batchLookups\":"<<s.lookup_calls<<",\"verifiedCopiedBatches\":"<<s.copied<<",\"copyMismatches\":"<<s.copy_mismatches<<",\"selectedBuilds\":"<<s.selected_builds<<",\"inFlightAtSave\":"<<s.in_flight.load()<<",\"radioBodies\":[";
    for(unsigned i=0;i<s.admission->count;++i){if(i)out<<',';out<<"{\"object\":"<<quote(s.admission->bodies[i].name)<<",\"slate\":"<<s.admission->bodies[i].slate<<",\"serial\":"<<s.admission->bodies[i].serial<<'}';}
    out<<"],\"packets\":[";bool first=true;for(const auto& p:s.packets){if(!first)out<<',';first=false;
        out<<"{\"id\":"<<p.id<<",\"window\":"<<p.window<<",\"batcher\":"<<p.batcher<<",\"radioRoles\":"<<p.roles<<",\"thread\":"<<p.thread<<",\"builds\":[";
        bool fb=true;for(const auto& b:p.builds){if(!fb)out<<',';fb=false;out<<"{\"data\":"<<b.data<<",\"policy\":"<<b.policy<<",\"graph\":"<<b.graph<<",\"thread\":"<<b.thread<<",\"vertices\":"<<b.vertices<<",\"indices\":"<<b.indices<<",\"vertexBuffer\":"<<b.vertex_buffer<<",\"indexBuffer\":"<<b.index_buffer<<",\"before\":[";
            for(size_t i=0;i<b.before.size();++i){if(i)out<<',';row_json(out,b.before[i]);}out<<"],\"after\":[";for(size_t i=0;i<b.after.size();++i){if(i)out<<',';row_json(out,b.after[i]);}out<<"]}";
        }out<<"]}";
    }out<<"],\"merges\":[";first=true;for(const auto& m:s.merges){if(!first)out<<',';first=false;out<<"{\"packet\":"<<m.packet<<",\"data\":"<<m.data<<",\"first\":"<<m.first<<",\"second\":"<<m.second<<",\"firstMask\":"<<m.first_mask<<",\"secondMask\":"<<m.second_mask<<",\"resultMask\":"<<m.result_mask<<'}';}
    out<<"],\"uiRhiAtArm\":"<<s.ui_rhi<<",\"uiNativeAtArm\":"<<s.ui_native<<",\"observedXrUiCopy\":{\"list\":"<<s.copy_list<<",\"source\":"<<s.copy_source<<",\"frame\":"<<s.copy_frame<<",\"qpc\":"<<s.copy_qpc<<"},\"nativePassConstructions\":[";
    first=true;for(const auto& p:s.passes){if(!first)out<<',';first=false;out<<"{\"args\":[";for(size_t i=0;i<p.args.size();++i){if(i)out<<',';out<<p.args[i];}out<<"],\"firstTargetRefValue\":"<<p.target_ref<<",\"secondaryRefValue\":"<<p.secondary_ref<<",\"policyRefValue\":"<<p.policy_ref<<",\"selectedData\":"<<(p.selected_data?"true":"false")<<",\"matchesUiRhiAtArm\":"<<(s.ui_rhi&&p.target_ref==s.ui_rhi?"true":"false")<<",\"qpc\":"<<p.qpc<<'}';}
    out<<"],\"selectedRadioRolesInUiPass\":"<<s.selected_ui_roles<<",\"constructorCalls\":"<<s.constructor_calls<<",\"unwindFailures\":"<<s.unwind_failures<<",\"constructorSamples\":[";first=true;
    for(const auto& v:s.constructors){if(!first)out<<',';first=false;out<<"{\"caller\":"<<v.caller<<",\"container\":"<<v.container<<",\"result\":"<<v.result<<",\"element\":"<<v.element<<",\"radioRoles\":"<<v.roles<<",\"unwindOk\":"<<(v.unwind_ok?"true":"false")<<",\"ambiguous\":"<<(v.ambiguous?"true":"false")<<",\"cached\":"<<(v.cached?"true":"false")<<",\"registers\":[";
        for(size_t i=0;i<v.registers.size();++i){if(i)out<<',';out<<v.registers[i];}out<<"],\"sourceArray\":"<<v.source_array<<",\"sourceMask\":"<<v.source_mask<<",\"boxGroup\":"<<(v.group?"true":"false")<<",\"verifiedOrigin\":"<<(v.verified_origin?"true":"false")<<",\"groupBegin\":"<<v.group_begin<<",\"groupEnd\":"<<v.group_end<<'}';}
    out<<"],\"ownedElementRecords\":[";first=true;for(const auto& t:s.tags){if(!first)out<<',';first=false;out<<"{\"window\":"<<t.window<<",\"container\":"<<t.container<<",\"index\":"<<t.index<<",\"owner\":"<<t.owner<<",\"type\":"<<unsigned(t.type)<<",\"layer\":"<<t.layer<<",\"radioRoles\":"<<t.roles<<",\"snapshot\":"<<(t.snapshot?"true":"false")<<",\"record\":\"";
        out<<std::hex<<std::setfill('0');for(size_t i=0;i<strides[t.type];++i)out<<std::setw(2)<<unsigned(t.contents[i]);out<<std::dec<<"\"}";}
    out<<"],\"maskMeaning\":\"1 selected radio evidence,2 no selected origin found,4 unresolved; bitwise combination exposes mixed/unknown batches.\",\"limits\":\"CPU identity trace and existing XR UI-copy observation only. Native pass construction is not GPU execution. Selection may request a normal engine weak identity. Record-byte comparison tracks copies, not pixel color/shape masks. Same capture/resource observations do not prove exact CPU-to-XR-frame lineage or GPU completion. No split renderer, complete cached descendant proof or A/B performance result.\"}\n";return out.str();
}
inline void ack(const std::string& id,bool accepted,const char* reason){if(directory.empty())return;std::ofstream out(directory/L"AC8_VR-SelectiveGlowBatchCapture-state.json",std::ios::binary);out<<"{\"id\":"<<quote(id)<<",\"accepted\":"<<(accepted?"true":"false")<<",\"reason\":"<<quote(reason)<<",\"hookReady\":"<<(ready?"true":"false")<<"}\n";}
inline void request(const char* value){
    if(!value)return;const std::string id=value;if(id.empty()||id.size()>48||id.find_first_not_of("0123456789-")!=std::string::npos)return;
    if(std::atomic_load(&current)){ack(id,false,"capture_busy");return;}if(!ready){ack(id,false,readiness);return;}
    radio_paint_marker::control("trace");auto admission=std::atomic_load(&radio_paint_marker::current);
    if(!admission||!admission->trace_only){ack(id,false,"radio_selection_not_admitted");return;}
    auto s=std::make_shared<Capture>();s->id=id;s->admission=admission;ui_at_arm(*s);std::atomic_store(&current,s);armed=true;ack(id,true,"waiting_for_radio_batch");
}
inline void cancel(){if(auto s=std::atomic_load(&current)){std::lock_guard<std::mutex> lock(s->mutex);s->done=true;s->reason="cancelled";}}
inline void engine_tick(){
    auto s=std::atomic_load(&current);if(!s)return;bool finished=false;
    {std::lock_guard<std::mutex> lock(s->mutex);++s->ticks;
        if(std::atomic_load(&radio_paint_marker::current)!=s->admission){s->done=true;s->reason="radio_session_ended";}
        if(!s->done)for(unsigned i=0;i<s->admission->count;++i)if(!radio_paint_marker::valid(s->admission->bodies[i],reinterpret_cast<void*>(s->admission->bodies[i].slate))){s->done=true;s->reason="selection_lifetime_changed";break;}
        if(!s->done&&!s->routing_session&&s->ticks>=(s->continuous?240u:120u)){s->done=true;s->reason="bounded_engine_callback_limit";}
        finished=s->done&&s->in_flight.load()==0;
    }
    if(!finished)return;armed=false;std::atomic_store(&current,std::shared_ptr<Capture>{});
    if(s->continuous){if(continuous_stopped)continuous_stopped(s->reason.c_str());return;}
    std::string body;{std::lock_guard<std::mutex> lock(s->mutex);body=report(*s);}
    std::ofstream(directory/("AC8_VR-SelectiveGlowBatchCapture-"+s->id+".json"),std::ios::binary)<<body;
    ack(s->id,true,"saved");if(std::atomic_load(&radio_paint_marker::current)==s->admission)radio_paint_marker::off("batch_capture_complete");
    if(api&&api->functions&&api->functions->dispatch_lua_event)api->functions->dispatch_lua_event("AC8_SelectiveGlowBatchCaptureDone",s->id.c_str());
}
// Exact entry guards generated from the already verified image catalog.
inline void initialize(const UEVR_PluginInitializeParam* host){
    api=host;directory=radio_paint_marker::status_file.parent_path();
    if(!radio_paint_marker::ready){readiness="radio_marker_guard_not_ready";return;}
    {std::array<unsigned char,selective_glow_origin_guards::box.size()> actual{};
     if(!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::rva<selective_glow_origin_guards::box_rva>()),actual,selective_glow_origin_guards::box)||actual!=selective_glow_origin_guards::box){readiness="box_origin_body_guard_failed";return;}}
    {std::array<unsigned char,selective_glow_origin_guards::text.size()> actual{};
     if(!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::rva<selective_glow_origin_guards::text_rva>()),actual,selective_glow_origin_guards::text)||actual!=selective_glow_origin_guards::text){readiness="text_origin_body_guard_failed";return;}}
    origin_guards_ready=true;
    struct Hook {uintptr_t rva;std::array<unsigned char,32> bytes;void* replacement;void** original;};
    const std::array<Hook,8> hooks{{
        {0x1bbf2d0,{{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x80,0xba,0xf0,0x08,0x00,0x00,0x00,0x45,0x0f,0xb6,0xd0,0x48,0x8b,0xf9,0x74,0x13,0x80,0x3d,0x3f,0xa6,0x08,0x0c}},reinterpret_cast<void*>(&init),reinterpret_cast<void**>(&original_init)},
        {0x1bc1510,{{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48,0x8d,0x42,0x48,0x48,0x8b,0xfa,0x48,0x89,0x01,0x48,0x8b,0xf1,0x48,0x8d,0x82,0xa8}},reinterpret_cast<void*>(&window),reinterpret_cast<void**>(&original_window)},
        {0x1bc0b90,{{0x40,0x55,0x56,0x57,0x41,0x55,0x48,0x8b,0xec,0x48,0x83,0xec,0x78,0x48,0x8b,0xfa,0x4d,0x8b,0xe8,0x41,0x8b,0x50,0x08,0x48,0x8b,0xf1,0x41,0x2b,0x50,0x34,0x03,0x51}},reinterpret_cast<void*>(&copy),reinterpret_cast<void**>(&original_copy)},
        {0x1bc0e70,{{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x20,0x8b,0x4a}},reinterpret_cast<void*>(&combine),reinterpret_cast<void**>(&original_combine)},
        {0x37fbc60,{{0x40,0x53,0x56,0x57,0x48,0x83,0xec,0x50,0x48,0x8b,0xf1,0x49,0x8b,0xd8,0x49,0x8b,0xc8,0x48,0x8b,0xfa,0xe8,0x67,0x53,0x3c,0xfe,0x83,0x7b,0x08,0x00,0x0f,0x8e,0x59}},reinterpret_cast<void*>(&buffers),reinterpret_cast<void**>(&original_buffers)},
        {0x37f2040,{{0x4c,0x8b,0xdc,0x55,0x57,0x49,0x8d,0xab,0xf8,0xfd,0xff,0xff,0x48,0x81,0xec,0xf8,0x02,0x00,0x00,0x48,0x8b,0x05,0xa6,0xab,0x43,0x0a,0x48,0x33,0xc4,0x48,0x89,0x85}},reinterpret_cast<void*>(&draw_pass),reinterpret_cast<void**>(&original_draw_pass)},
        {0x1bc0a70,{{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x20,0x48,0x63,0x79,0x08}},reinterpret_cast<void*>(&fresh),reinterpret_cast<void**>(&original_fresh)},
        {0x1bbeac0,{{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x4c,0x89,0x70,0x20,0x55,0x48,0x8d,0x68,0xc9,0x48,0x81,0xec,0xf0,0x00,0x00,0x00,0x0f}},reinterpret_cast<void*>(&cached),reinterpret_cast<void**>(&original_cached)},
    }};
    for(const auto& h:hooks){std::array<unsigned char,32> actual{};if(!ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::translate(h.rva)),actual,h.bytes)||actual!=h.bytes){readiness="batch_entry_guard_failed";return;}}
    for(const auto& h:hooks)if(host->functions->register_inline_hook(reinterpret_cast<void*>(radio_paint_marker::image_base+ac8_game_compatibility::translate(h.rva)),h.replacement,h.original)<0||!*h.original){readiness="batch_hook_registration_failed";return;}
    radio_paint_marker::trace_paint_enter=paint_enter;radio_paint_marker::trace_paint_leave=paint_leave;radio_paint_marker::trace_radar_geometry=radar_geometry;
    ready=true;readiness="ready";
}
}
