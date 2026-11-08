#pragma once
// Two saved game-thread crashes returned a null GetGaugeImage result and then
// called the image-material getter. The caller already tests the returned
// material for null. Guard ONLY that null image and exact verified call site.
namespace menu_gauge_guard {
using Object=UEVR_UObjectHandle;
using Getter=Object(*)(Object);
inline const UEVR_PluginInitializeParam* host=nullptr;
inline Getter original=nullptr;
inline uintptr_t base=0;
inline bool accepted=false;

constexpr uintptr_t getterRva=0x3cc7560,callerReturnRva=0x7a0ebe4;
constexpr std::array<unsigned char,32> getterHead{{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x99,0x10,0x02,0x00,0x00,0x48,0x8b,0xf9,0x48,0x85,0xdb,0x0f,0x84,0x8a,0x00,0x00,0x00,0xe8,0xce,0x97}};
constexpr std::array<unsigned char,5> callerHead{{0xe8,0x7c,0x89,0x2b,0xfc}};
inline bool missingAtVerifiedCaller(Object image,uintptr_t caller){return accepted&&!image&&caller==base+ac8_game_compatibility::rva<callerReturnRva>();}
#ifdef AC8_MENU_GAUGE_FIXTURE
inline uintptr_t fixtureCaller=0;
#endif

inline Object wrapper(Object image){
#ifdef AC8_MENU_GAUGE_FIXTURE
    const auto caller=fixtureCaller;
#else
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
#endif
    if(missingAtVerifiedCaller(image,caller)){
        return nullptr;
    }
    return original?original(image):nullptr;
}
template<class T> inline bool read(const void* address,T& value){SIZE_T count=0;return address&&ReadProcessMemory(GetCurrentProcess(),address,&value,sizeof(value),&count)&&count==sizeof(value);}
#ifndef AC8_MENU_GAUGE_FIXTURE
inline bool initialize(const UEVR_PluginInitializeParam* p){
    host=p;auto module=GetModuleHandleW(nullptr);base=reinterpret_cast<uintptr_t>(module);
    // The getter and its exact caller are the dependency, not the EXE date/hash.
    std::array<unsigned char,32> getter{};std::array<unsigned char,5> caller{};
    if(!module||!ac8_game_compatibility::allowed("menu_gauge")||
       !ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(base+ac8_game_compatibility::rva<getterRva>()),getter,getterHead)||getter!=getterHead||
       !ac8_game_compatibility::copy_verified_source(reinterpret_cast<void*>(base+ac8_game_compatibility::rva<callerReturnRva-5>()),caller,callerHead)||caller!=callerHead){
        return false;
    }
    const int id=p->functions->register_inline_hook(reinterpret_cast<void*>(base+ac8_game_compatibility::rva<getterRva>()),reinterpret_cast<void*>(&wrapper),reinterpret_cast<void**>(&original));
    accepted=id>=0&&original;
    if(!accepted&&id>=0)p->functions->unregister_inline_hook(id);
    
    return accepted;
}
#endif
}
