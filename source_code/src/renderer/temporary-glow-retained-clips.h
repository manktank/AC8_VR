#pragma once
// TEMP native clip preservation, not glow membership. Identity selection is
// already finished before this adapter inspects ONLY the retained draw chain.
#include "temporary-glow-clip-guards.h"
#include <cmath>
namespace selective_glow_retained_clips {
using Clip=std::array<unsigned char,0x38>;
inline bool layout_ready=false,stencil_ready=false;
using Quad=std::array<unsigned char,36>;
struct Witness {int32_t row=-1;uintptr_t clip=0;unsigned mode=0;};
struct Stats {
    bool checked=false;unsigned summary=0,plain=0,scissor=0,stencil=0,unreadable=0,invalid=0;
    std::vector<Witness> unsupported;
};
struct Plan {
    Stats stats;std::vector<Clip> copies,originals;std::vector<std::vector<Quad>> quads;const char* reason="native_clips_preserved";bool ready=false;
    Plan()=default;Plan(const Plan&)=delete;Plan& operator=(const Plan&)=delete;Plan(Plan&&)=default;Plan& operator=(Plan&&)=default;
};
inline Plan preserve(identity_batch_plan::Plan& batches,unsigned summary){
    Plan result;auto& s=result.stats;s.checked=true;s.summary=summary;
    if(summary>1){result.reason="invalid_native_clip_summary";return result;}
    if(summary&&!layout_ready){result.reason="native_clip_layout_unverified";return result;}
    result.copies.reserve(batches.included.size());result.originals.reserve(batches.included.size());result.quads.reserve(batches.included.size());std::unordered_map<uintptr_t,size_t> copied;
    for(const int32_t index:batches.included){
        auto& row=batches.private_records.at(static_cast<size_t>(index));
        const auto pointer=identity_batch_plan::get<uintptr_t>(row,0x38);
        if(!pointer){++s.plain;continue;}
        if(!layout_ready){result.reason="native_clip_layout_unverified";return result;}
        Clip bytes{};unsigned mode=0;bool refused=false;
        if(!selective_glow_batch_capture::read(reinterpret_cast<void*>(pointer),bytes)){++s.unreadable;refused=true;}
        else if((mode=bytes[0x34])==0){++s.stencil;refused=!stencil_ready;}
        else if(mode!=1){++s.invalid;refused=true;}
        else{
            for(size_t at:{size_t{0x10},size_t{0x14},size_t{0x28},size_t{0x2c}}){float value=0;std::memcpy(&value,bytes.data()+at,4);if(!std::isfinite(value)){++s.invalid;refused=true;break;}}
        }
        if(refused){if(s.unsupported.size()<8)s.unsupported.push_back({index,pointer,mode});continue;}
        if(mode==1)++s.scissor;auto found=copied.find(pointer);
        if(found==copied.end()){const auto slot=result.copies.size();const auto original=bytes;std::vector<Quad> vertices;
            if(mode==0){
                uintptr_t data=0;int32_t count=0,capacity=0;std::memcpy(&data,bytes.data(),8);std::memcpy(&count,bytes.data()+8,4);std::memcpy(&capacity,bytes.data()+12,4);
                if(!data||count<1||count>255||capacity<count||capacity>4096){++s.invalid;continue;}
                vertices.resize(static_cast<size_t>(count));bool valid=true;
                for(int32_t q=0;q<count;++q){auto& quad=vertices[static_cast<size_t>(q)];if(!selective_glow_batch_capture::read(reinterpret_cast<void*>(data+static_cast<uintptr_t>(q)*36),quad)){valid=false;break;}
                    for(size_t offset=0;offset<32;offset+=4){float value=0;std::memcpy(&value,quad.data()+offset,4);if(!std::isfinite(value)){valid=false;break;}}
                }
                if(!valid){++s.invalid;continue;}
                const auto owned=reinterpret_cast<uintptr_t>(vertices.data());std::memcpy(bytes.data(),&owned,8);std::memcpy(bytes.data()+12,&count,4);
            }
            result.originals.push_back(original);result.quads.push_back(std::move(vertices));result.copies.push_back(bytes);found=copied.emplace(pointer,slot).first;}
        else {
            bool same=result.originals[found->second]==bytes;
            if(same&&mode==0){uintptr_t data=0;std::memcpy(&data,bytes.data(),8);
                const auto& saved=result.quads[found->second];
                for(size_t q=0;q<saved.size();++q){Quad current{};if(!selective_glow_batch_capture::read(reinterpret_cast<void*>(data+q*36),current)||current!=saved[q]){same=false;break;}}
            }
            if(!same){++s.invalid;continue;}
        }
        // Native37fc5e5..6fd consumes these scalar clip bounds synchronously,
        // emitting scissor integers into the RHI command. No pointer escapes.
        // Copy once per source pointer so the native clip-state cache still
        // sees shared clips as shared. No game clip or attachment is modified.
        const auto owned=reinterpret_cast<uintptr_t>(result.copies[found->second].data());std::memcpy(row.bytes.data()+0x38,&owned,sizeof(owned));
    }
    result.ready=(!s.stencil||stencil_ready)&&!s.unreadable&&!s.invalid;
    result.reason=s.unreadable?"retained_clip_unreadable":s.invalid?"retained_clip_invalid_or_changed":s.stencil&&!stencil_ready?"retained_stencil_clip_requires_private_attachment":"native_clips_preserved";
    return result;
}
inline void json(std::ostream& out,const Stats& s){
    out<<"{\"originalSummary\":"<<s.summary<<",\"retainedUnclipped\":"<<s.plain<<",\"retainedScissor\":"<<s.scissor<<",\"retainedStencil\":"<<s.stencil<<",\"unreadable\":"<<s.unreadable<<",\"invalid\":"<<s.invalid<<",\"firstUnsupported\":[";
    for(size_t i=0;i<s.unsupported.size();++i){if(i)out<<',';const auto& w=s.unsupported[i];out<<"{\"row\":"<<w.row<<",\"clip\":"<<w.clip<<",\"mode\":"<<w.mode<<'}';}out<<"]}";
}
inline void initialize(){
    using namespace selective_glow_clip_guards;
    layout_ready=selective_glow_ownership::ready&&selective_glow_ownership::guard(summary_rva,summary)&&selective_glow_ownership::guard(dispatch_rva,dispatch)&&selective_glow_ownership::guard(scissor_rva,scissor)&&selective_glow_ownership::guard(attachment_rva,attachment);
}
}
