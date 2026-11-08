#pragma once
// Private CPU prototype only. No native call, GPU command, or image permissions.
// Only a positively verified, unmixed selected identity authorizes removal.
// Unknown/mixed identity retains that ORIGINAL batch, not the entire source.
// Structural/lifetime validation is independent and still refuses unsafe data.
namespace identity_batch_plan {
using Raw=selective_glow_batch_capture::Raw;
struct Origin {unsigned mask=4;bool verified=false;};
struct Plan {
    bool ready=false;const char* reason="not_planned";int32_t first=-1;
    unsigned sharp_draws=0,glow_draws=0,excluded_draws=0;
    unsigned uncertain_retained=0;
    int32_t first_uncertain=-1;
    std::vector<Raw> private_records;
    std::vector<int32_t> included,excluded;
};
template<class T> inline T get(const Raw& r,size_t at){T value{};std::memcpy(&value,r.bytes.data()+at,sizeof(value));return value;}
inline Plan make(const std::vector<Raw>& sharp_source,const std::vector<Origin>& origins,int32_t first,int32_t vertices,int32_t indices){
    Plan p;
    if(sharp_source.size()>4096||sharp_source.size()!=origins.size()||vertices<0||indices<0){p.reason="invalid_bounds";return p;}
    std::vector<unsigned char> visited(sharp_source.size(),0);
    int32_t next=first;
    while(next!=-1){
        if(next<0||static_cast<size_t>(next)>=sharp_source.size()||visited[static_cast<size_t>(next)]){p.reason="invalid_or_cyclic_chain";return p;}
        const auto index=static_cast<size_t>(next);visited[index]=1;const auto& row=sharp_source[index];const auto origin=origins[index];
        if(row.bytes[0x8f]&0x02){p.reason="merged_row_in_active_chain";return p;}
        if(get<uintptr_t>(row,0x60)){p.reason="unsupported_custom_draw";return p;}
        const auto vo=get<int32_t>(row,0x6c),io=get<int32_t>(row,0x70),vc=get<int32_t>(row,0x74),ic=get<int32_t>(row,0x78);
        if(vo<0||io<0||vc<=0||ic<=0||vo>vertices||io>indices||vc>vertices-vo||ic>indices-io){p.reason="geometry_outside_uploaded_buffers";return p;}
        ++p.sharp_draws;
        if(origin.verified&&origin.mask==1){p.excluded.push_back(next);++p.excluded_draws;}
        else{p.included.push_back(next);++p.glow_draws;
            if(!origin.verified||origin.mask!=2){++p.uncertain_retained;if(p.first_uncertain<0)p.first_uncertain=next;}}
        next=get<int32_t>(row,0x7c);
    }
    p.private_records=sharp_source;
    for(size_t i=0;i<p.included.size();++i){const int32_t link=i+1<p.included.size()?p.included[i+1]:-1;
        std::memcpy(p.private_records[static_cast<size_t>(p.included[i])].bytes.data()+0x7c,&link,sizeof(link));}
    p.first=p.included.empty()?-1:p.included.front();p.ready=true;p.reason=p.uncertain_retained?"private_glow_chain_planned_with_retained_unknowns":"private_glow_chain_planned";return p;
}
}
