#pragma once
#include "hud_policy.h"
#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <cstring>
namespace prepaint_frame_policy {
struct Rect {float left=0,top=0,right=0,bottom=0;};
struct Frame {
    uint64_t owner=0,generation=0,upload=0;
    unsigned count=0;bool authoritative=false;
    std::array<Rect,8> radar{};
};
template<class T>inline bool read(uintptr_t p,T& value)noexcept{
    if(!p)return false;__try{std::memcpy(&value,reinterpret_cast<void*>(p),sizeof(T));return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline std::shared_ptr<const Frame> freeze(const Frame& input,UINT width,UINT height){
    auto out=std::make_shared<Frame>(input);out->count=0;
    if(!width||!height){out->authoritative=false;return out;}
    for(unsigned i=0;i<input.count&&i<input.radar.size();++i){
        const auto& r=input.radar[i];Rect n{r.left/width,r.top/height,r.right/width,r.bottom/height};
        n.left=std::max(0.f,n.left);n.top=std::max(0.f,n.top);n.right=std::min(1.f,n.right);n.bottom=std::min(1.f,n.bottom);
        if(std::isfinite(n.left)&&std::isfinite(n.top)&&std::isfinite(n.right)&&std::isfinite(n.bottom)&&n.left<n.right&&n.top<n.bottom)out->radar[out->count++]=n;
    }return out;
}
inline void add(Frame& frame,const Rect& value){
    for(unsigned i=0;i<frame.count;++i)if(!std::memcmp(&frame.radar[i],&value,sizeof(value)))return;
    if(frame.count<frame.radar.size())frame.radar[frame.count++]=value;
}
inline hud_policy::Snapshot compose(const hud_policy::Snapshot& ordinary,const std::shared_ptr<const Frame>& native){
    if(!native||!native->authoritative)return ordinary;
    hud_policy::Snapshot result{};result.time=ordinary.time;
    for(unsigned i=0;i<ordinary.count&&result.count<result.regions.size();++i)if(ordinary.regions[i].flags[1]==1)result.regions[result.count++]=ordinary.regions[i];
    for(unsigned i=0;i<native->count&&result.count<result.regions.size();++i){
        auto& r=result.regions[result.count++];const auto& b=native->radar[i];r.bounds[0]=b.left;r.bounds[1]=b.top;r.bounds[2]=b.right;r.bounds[3]=b.bottom;r.flags[2]=1;
    }return result;
}
inline thread_local std::shared_ptr<const Frame> executing;
struct Scope {
    std::shared_ptr<const Frame> previous;
    explicit Scope(std::shared_ptr<const Frame> value):previous(std::move(executing)){executing=std::move(value);}
    ~Scope(){executing=std::move(previous);}
};
struct Command {uintptr_t transition=0,list=0;std::shared_ptr<const Frame> frame;};
inline std::mutex mutex;
inline std::unordered_map<uintptr_t,Command> commands;
inline std::unordered_map<uintptr_t,std::shared_ptr<const Frame>> producers;
inline std::atomic<uint64_t> queued{0},executed{0},bound{0},refused{0};
inline void queued_commands(uintptr_t list,uintptr_t previous_tail,uintptr_t execute_method,const std::shared_ptr<const Frame>& frame){
    // The already guarded FRHI transition routine appends through CommandLink
    // at+8. Its native command has vtable+0, Next+8, transition+16. Associate the
    // actual queued command, not a global "latest UI" value or FIFO assumption.
    if(!frame||!frame->authoritative||!previous_tail||!execute_method)return;
    uintptr_t command=0;if(!read(previous_tail,command))return;
    for(unsigned n=0;command&&n<16;++n){
        uintptr_t table=0,method=0,transition=0,next=0;
        if(!read(command,table)||!read(table,method)||!read(command+8,next))break;
        if(method==execute_method&&read(command+16,transition)&&transition){
            std::lock_guard<std::mutex> guard(mutex);
            if(commands.size()<512||commands.count(command)){commands[command]={transition,list,frame};++queued;}else ++refused;
        }command=next;
    }
}
inline std::shared_ptr<const Frame> consume(uintptr_t command,uintptr_t list){
    std::lock_guard<std::mutex> guard(mutex);const auto it=commands.find(command);if(it==commands.end())return{};
    auto value=it->second;commands.erase(it);uintptr_t transition=0;
    // Deferred execution can use the engine's submitted list wrapper. The
    // command's exact transition identity is the lifetime key, not that wrapper.
    static_cast<void>(list);
    if(!read(command+16,transition)||transition!=value.transition){++refused;return{};}
    ++executed;return value.frame;
}
inline void producer(uintptr_t list){
    if(!executing||!executing->authoritative)return;
    std::lock_guard<std::mutex> guard(mutex);if(producers.size()<256||producers.count(list)){producers[list]=executing;++bound;}else ++refused;
}
inline std::shared_ptr<const Frame> for_submission(UINT count,ID3D12CommandList* const* lists){
    std::lock_guard<std::mutex> guard(mutex);std::shared_ptr<const Frame> result;
    for(UINT i=0;lists&&i<count;++i){const auto it=producers.find(reinterpret_cast<uintptr_t>(lists[i]));if(it!=producers.end())result=it->second;}return result;
}
inline void retire(UINT count,ID3D12CommandList* const* lists){
    std::lock_guard<std::mutex> guard(mutex);for(UINT i=0;lists&&i<count;++i)producers.erase(reinterpret_cast<uintptr_t>(lists[i]));
}
inline void reset(uintptr_t list){std::lock_guard<std::mutex> guard(mutex);producers.erase(list);}
} // namespace prepaint_frame_policy

