#pragma once
// Native ownership contracts are admitted once by live-paint-routing.h. Cold
// objects resolve before paint; cached batches only revalidate captured edges.
#include <array>
#include <cstdint>
namespace live_owner {
namespace p=radio_paint_marker;
struct Bindings {
    bool enabled=false;
    uint64_t owner_generation=0;
    uintptr_t object_widget_table=0;
    size_t widget_object=0x298,parent_pair=0x1e0,self_pair=8;
};
struct Pair {uintptr_t widget=0,control=0;bool operator==(const Pair& b)const{return widget==b.widget&&control==b.control;}};
struct Edge {uintptr_t child=0,table=0;Pair self,parent;};
struct Decision {
    enum class Kind {glow,sharp,radar};
    Kind kind=Kind::glow;
    unsigned roles=0,count=0;
    uint64_t owner_generation=0;
    bool proven=false;
    p::Leaf owner;
    std::array<Edge,32> path{};
};
inline bool read_pair(uintptr_t at,Pair& pair){
    return p::local_read(reinterpret_cast<void*>(at),pair);
}
inline bool live_pair(const Pair& pair,const Bindings& bindings){
    if(!pair.widget||!pair.control||!p::readable_candidate(pair.widget,bindings.parent_pair+sizeof(Pair))||
       !p::readable_candidate(pair.control+8,4))return false;
    int32_t strong=0;Pair self;
    return p::local_read(reinterpret_cast<void*>(pair.control+8),strong)&&strong>0&&
           read_pair(pair.widget+bindings.self_pair,self)&&self==pair;
}
inline bool edge(uintptr_t slate,const Bindings& bindings,Edge& result){
    if(!slate||!p::readable_candidate(slate,bindings.parent_pair+sizeof(Pair))||
       !p::pointer_field(slate,result.table)||!read_pair(slate+bindings.self_pair,result.self)||
       result.self.widget!=slate||!live_pair(result.self,bindings)||!read_pair(slate+bindings.parent_pair,result.parent))return false;
    result.child=slate;
    if(!result.parent.widget&&!result.parent.control)return true;
    return live_pair(result.parent,bindings);
}
// policy(object) executes only on the engine's paint/construction thread.
// It supplies current-owner/class semantics, not a deferred discovery roster.
// Cached draw consumption calls validate() and performs no SDK/registry search.
template<class Policy>Decision classify(uintptr_t slate,const Bindings& bindings,Policy policy){
    Decision result;if(!bindings.enabled||!bindings.object_widget_table)return result;
    for(unsigned depth=0;depth<result.path.size();++depth){
        for(unsigned i=0;i<result.count;++i)if(result.path[i].child==slate)return {};
        Edge step;if(!edge(slate,bindings,step))return {};
        result.path[result.count++]=step;
        if(step.table==bindings.object_widget_table){
            p::Object object=nullptr;
            if(!p::local_read(reinterpret_cast<void*>(slate+bindings.widget_object),object))return {};
            p::Leaf owner;if(!p::make_identity(object,owner))return {};
            const unsigned roles=policy(object);
            {
                // Pair the actual engine back-pointer with the UObject's own
                // current weak Slate binding; address/class names alone cannot
                // prove lifetime or ownership, even on an apparent cache hit.
                bool bound=false;
                for(const uintptr_t field:{uintptr_t{0x108},uintptr_t{0xf8}}){
                    Pair pair;if(read_pair(reinterpret_cast<uintptr_t>(object)+field,pair)&&pair==step.self){
                        owner.slate=slate;owner.control=pair.control;owner.table=step.table;owner.weak_field=field;
                        if(p::valid(owner,reinterpret_cast<void*>(slate))){bound=true;break;}
                    }
                }
                if(!bound)return {};
                result.owner=std::move(owner);
                if(roles){result.roles=roles;result.owner_generation=bindings.owner_generation;
                    result.kind=(roles&64)?Decision::Kind::radar:Decision::Kind::sharp;result.proven=true;return result;}
            }
        }
        if(!step.parent.widget){result.proven=true;result.owner_generation=bindings.owner_generation;return result;}
        slate=step.parent.widget;
    }return {};
}
inline bool validate(const Decision& decision,const Bindings& bindings){
    if(!bindings.enabled||decision.owner_generation!=bindings.owner_generation||!decision.proven||!decision.count||decision.count>decision.path.size()||
       (decision.owner.object&&!p::valid(decision.owner,reinterpret_cast<void*>(decision.owner.slate))))return false;
    for(unsigned i=0;i<decision.count;++i){
        const auto& old=decision.path[i];Edge observed;observed.child=old.child;
        int32_t strong=0;
        if(!p::pointer_field(old.child,observed.table)||observed.table!=old.table||
           !read_pair(old.child+bindings.self_pair,observed.self)||!(observed.self==old.self)||
           !read_pair(old.child+bindings.parent_pair,observed.parent)||!(observed.parent==old.parent)||
           !p::local_read(reinterpret_cast<void*>(old.self.control+8),strong)||strong<=0)return false;
        if(i+1<decision.count&&observed.parent.widget!=decision.path[i+1].child)return false;
    }
    if(!decision.owner.object)return true;
    p::Object object=nullptr;
    return p::local_read(reinterpret_cast<void*>(decision.owner.slate+bindings.widget_object),object)&&object==decision.owner.object;
}
} // namespace live_owner

