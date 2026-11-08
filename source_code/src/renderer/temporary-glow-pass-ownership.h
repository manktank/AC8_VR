#pragma once
// TEMP private ABI adapter. Exact caller37f349e..353f COPY-constructs arg1,
// arg11 and arg12; callee37f260e..26a1 consumes them. Preserve the original
// call's units; create independent units and transfer each exactly once.
#include "temporary-radio-paint-marker.h"
#include "temporary-glow-ownership-guards.h"
namespace selective_glow_ownership {
using Args=std::array<uintptr_t,12>;
using DeleteRhi=void(*)(uintptr_t);
inline DeleteRhi delete_rhi=nullptr;
inline bool ready=false;
inline bool in_image(uintptr_t p){
#ifdef AC8_GLOW_OWNERSHIP_FIXTURE
    return p>=0x10000;
#else
    // Keep this hot-path check as cheap as the old constant-size range test.
    // PE/section validation runs at startup; use its cached mapped extent here.
    const auto& image=ac8_game_compatibility::runtime.image();
    return image.base&&p>=image.base&&p-image.base<image.size;
#endif
}
template<class T>inline bool read(uintptr_t at,T& value){return selective_glow_inventory::read(reinterpret_cast<void*>(at),value);}
inline bool positive_count(uintptr_t at){LONG value=0;return read(at,value)&&value>0&&value<1000000;}
inline bool table(uintptr_t object,bool shared){
    uintptr_t vtable=0,first=0,second=0;
    return read(object,vtable)&&in_image(vtable)&&read(vtable,first)&&in_image(first)&&
        (!shared||(read(vtable+8,second)&&in_image(second)));
}
inline void release_rhi(uintptr_t value)noexcept{
    if(!value)return;const auto old=static_cast<uint32_t>(InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(value+8),-1));
    if((old&0x3fffffffu)==1)delete_rhi(value); // normal deferred native deletion
}
struct Rhi {
    uintptr_t value=0;bool owned=false;
    Rhi()=default;Rhi(const Rhi&)=delete;Rhi& operator=(const Rhi&)=delete;
    ~Rhi(){if(owned)release_rhi(value);}
    bool copy(uintptr_t borrowed){
        if(owned||!ready||!borrowed||!table(borrowed,false)||!positive_count(borrowed+8))return false;
        // The source has an existing owner throughout this synchronous copy.
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(borrowed+8));value=borrowed;owned=true;return true;
    }
    uintptr_t transfer()noexcept{if(!owned)return 0;owned=false;return reinterpret_cast<uintptr_t>(&value);}
};
struct Shared {
    std::array<uintptr_t,2> words{};bool owned=false;
    Shared()=default;Shared(const Shared&)=delete;Shared& operator=(const Shared&)=delete;
    ~Shared(){reset();}
    bool copy(uintptr_t borrowed){
        if(owned||!ready)return false;std::array<uintptr_t,2> pair{};
        if(!read(borrowed,pair)||!pair[0]||!pair[1]||!table(pair[1],true)||!positive_count(pair[1]+8)||!positive_count(pair[1]+12))return false;
        // Native TSharedPtr copy: object/control words and one strong unit.
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(pair[1]+8));words=pair;owned=true;return true;
    }
    void reset()noexcept{
        if(!owned)return;owned=false;const auto control=words[1];
        if(InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(control+8),-1)==1){
            const auto methods=*reinterpret_cast<const uintptr_t* const*>(control);
            reinterpret_cast<void(*)(uintptr_t)>(methods[0])(control);
            if(InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(control+12),-1)==1)
                reinterpret_cast<void(*)(uintptr_t,uint32_t)>(methods[1])(control,1);
        }
    }
    uintptr_t transfer()noexcept{if(!owned)return 0;owned=false;return reinterpret_cast<uintptr_t>(words.data());}
};
struct Prepared {
    uintptr_t primary=0,secondary=0;bool snapshot=false;Shared policy,sharp_policy;
};
static_assert(offsetof(Rhi,value)==0&&offsetof(Shared,words)==0,"native argument storage starts at the first word");
template<size_t N>inline bool guard(uintptr_t rva,const std::array<unsigned char,N>& bytes){
    std::array<unsigned char,N> actual{};return ac8_game_compatibility::copy_verified_source(radio_paint_marker::image_base+ac8_game_compatibility::translate(rva),actual,bytes)&&actual==bytes;
}
inline void initialize(){
    using namespace selective_glow_ownership_guards;
    ready=radio_paint_marker::ready&&guard(caller_copy_rva,caller_copy)&&guard(callee_consume_rva,callee_consume)&&guard(rhi_deferred_delete_rva,rhi_deferred_delete);
    if(ready)delete_rhi=reinterpret_cast<DeleteRhi>(radio_paint_marker::image_base+ac8_game_compatibility::rva<rhi_deferred_delete_rva>());
}
}
