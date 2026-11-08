#pragma once
#include "../../vendor/uevr/plugin-api.h"
#include "game-compatibility.h"
#include "game-compatibility-catalog.h"
#include "native-resolution.h"
#include "native-resolution-recipes.h"
#include "native-used-symbols.h"
namespace ac8_game_compatibility {
inline Cache runtime;
inline ac8_native_resolution::Bindings native_bindings;
inline ac8_native_resolution::Resolution native_resolution;
inline std::array<Result,std::size(ac8_generated_resolution::group_names)> native_results{};
inline std::once_flag preflight_once;
struct Demand {bool glow=false,targets=false,hud=false,pause=false,cinematics=false,loading=false;};
inline bool requested(const char* name,const Demand& demand){
    if(!std::strcmp(name,"glow"))return demand.glow;
    if(!std::strcmp(name,"target_layout")||!std::strcmp(name,"arrows")||!std::strcmp(name,"seeker"))return demand.targets;
    // Bomb/weapon helpers are initialized by ac8_hud_support_initialize, even
    // when Cinematics is absent. Follow the real native owner, not the folder
    // containing the implementation, or module removal would disable siblings.
    if(!std::strcmp(name,"menu_gauge")||!std::strcmp(name,"hud_ticks")||!std::strcmp(name,"bomb")||!std::strcmp(name,"weapon"))return demand.hud;
    if(!std::strcmp(name,"pause_ticks"))return demand.pause;
    if(!std::strcmp(name,"stereo_projection"))return demand.cinematics;
    if(!std::strcmp(name,"bomb_circle")||!std::strcmp(name,"bomb_render")||!std::strcmp(name,"weapon_surface"))return demand.hud;
    if(!std::strcmp(name,"viewport_helpers"))return demand.hud||demand.targets;
    if(!std::strcmp(name,"locator_helpers")||!std::strcmp(name,"locator_ally"))return demand.targets;
    if(!std::strcmp(name,"native_fade"))return demand.loading;
    if(!std::strcmp(name,"refueling"))return demand.targets;
    return false;
}
// Called at native initialization before registering any inline hook. A failed
// contract is recorded for that feature; it never aborts sibling initialization.
// There is no per-frame image scan, disk hash, file polling or fixed retry timer.
inline void preflight(const UEVR_PluginInitializeParam* compatibility_host,Demand demand){
    std::call_once(preflight_once,[compatibility_host,demand]{
        // Respect optional module removal. Absent consumers incur no dependency
        // reads, and their absence is not logged as an incompatibility.
        namespace g=ac8_generated_resolution;namespace n=ac8_native_resolution;
        uint32_t groups=0;for(size_t i=0;i<std::size(g::group_names);++i)if(requested(g::group_names[i],demand))groups|=1u<<i;
        const auto image=inspect(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
        runtime.initialize(image,nullptr,0);
        wchar_t executable[32768]{},directory[32768]{};
        const auto length=GetModuleFileNameW(nullptr,executable,32768);
        const auto persistent=compatibility_host&&compatibility_host->functions&&compatibility_host->functions->get_persistent_dir?
            compatibility_host->functions->get_persistent_dir(directory,32768):0;
        if(groups&&length&&length<32768&&persistent&&persistent<32768)
            native_resolution=n::resolve(image,g::patterns,std::size(g::patterns),groups,executable,
                std::filesystem::path(directory)/L"data"/L"AC8_VR-NativeAddresses.cache",g::fingerprint);
        native_bindings.initialize(image,g::patterns,std::size(g::patterns),groups,native_resolution.matches);
        for(size_t i=0;i<std::size(g::group_names);++i){
            auto& r=native_results[i];const auto bit=1u<<i;
            if(!(groups&bit)){r={false,"feature_not_requested","",0,0};continue;}
            r={native_resolution.complete,"native_dependencies_resolved","",0,0};
            // Reviewed versions of one routine are ALTERNATIVES for a stable
            // symbol. Requiring every historical implementation to match would
            // recreate a version allowlist in a different form.
            std::set<uint32_t> checked;
            for(const auto& pattern:g::patterns)if((pattern.groups&bit)&&checked.insert(pattern.symbol).second){
                bool matched=false;for(size_t j=0;j<std::size(g::patterns)&&j<native_resolution.matches.size();++j)
                    if(g::patterns[j].symbol==pattern.symbol&&native_resolution.matches[j].state==n::State::unique){matched=true;break;}
                if(!matched){r={false,"dependency_missing_ambiguous_or_changed",pattern.name,pattern.symbol,r.compared};break;}
                r.compared+=pattern.size;
            }
            if(r.allowed)for(const auto& key:ac8_used_native_symbols::keys)if((key.groups&bit)&&!native_bindings.rva(key.symbol)){
                r={false,"dependent_address_unresolved",key.consumers,key.symbol,r.compared};break;
            }
            if(!r.allowed&&compatibility_host&&compatibility_host->functions&&compatibility_host->functions->log_error)
                compatibility_host->functions->log_error("[AC8 compatibility] %s disabled: %s; dependency=%s symbol=0x%lx",g::group_names[i],r.reason,r.dependency,static_cast<unsigned long>(r.rva));
        }
    });
}
inline Result result(const char* name){
    for(size_t i=0;i<std::size(ac8_generated_resolution::group_names);++i)if(!std::strcmp(name,ac8_generated_resolution::group_names[i]))return native_results[i];
    return {false,"unknown_feature_contract","",0,0};
}
inline bool allowed(const char* name){return result(name).allowed;}
inline uint32_t translate(uintptr_t symbol){
    if(symbol>UINT32_MAX)return 0;
#ifdef AC8_NATIVE_RESOLUTION_IDENTITY_FIXTURE
    return static_cast<uint32_t>(symbol); // Synthetic consumer fixtures explicitly opt in at build time.
#else
    return native_bindings.rva(static_cast<uint32_t>(symbol));
#endif
}
template<uint32_t Symbol>inline uint32_t rva(){
#ifdef AC8_NATIVE_RESOLUTION_IDENTITY_FIXTURE
    return Symbol;
#else
    // Resolve a scalar binding once. Frames do not search maps, read files or
    // inspect instructions; they reuse this initialized value. Preflight precedes
    // every consumer's registration, so this cannot cache a pre-startup address.
    static const uint32_t value=translate(Symbol);return value;
#endif
}
inline bool code_proof(uintptr_t address,const void* expected,size_t size){
#ifdef AC8_NATIVE_RESOLUTION_IDENTITY_FIXTURE
    std::vector<unsigned char> actual(size);SIZE_T copied=0;
    return address&&expected&&size&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),actual.data(),size,&copied)&&copied==size&&!std::memcmp(actual.data(),expected,size);
#else
    const auto base=runtime.image().base;
    return expected&&size&&address>=base&&address-base<=UINT32_MAX&&native_bindings.proof_at(static_cast<uint32_t>(address-base),expected,size);
#endif
}
template<class T>inline bool code_proof(uintptr_t address,const T& expected){return code_proof(address,std::data(expected),std::size(expected));}
template<class T>inline bool code_proof(const void* address,const T& expected){return code_proof(reinterpret_cast<uintptr_t>(address),expected);}
template<class A,class E,class P>inline bool copy_verified_source(P address,A& original,const E& expected){
    // Legacy callers compare an instruction buffer with reviewed source bytes.
    // A relocated RIP operand no longer has those literal bytes. Reuse the
    // BEFORE-HOOK live proof and return its verified source representation;
    // never treat a previously unknown address/fragment as a passed guard.
    static_assert(sizeof(A)==sizeof(E),"source proof buffer must match guard extent");
    if(!code_proof(address,expected))return false;std::memcpy(std::data(original),std::data(expected),sizeof(A));return true;
}
inline bool tail_alias(uintptr_t base,uint32_t alias,uint32_t body){
    const auto at=translate(alias),target=translate(body);if(!at||!target)return false;
    std::array<unsigned char,5> bytes{};SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+at),bytes.data(),bytes.size(),&copied)||copied!=bytes.size()||bytes[0]!=0xe9)return false;
    int32_t displacement=0;std::memcpy(&displacement,bytes.data()+1,4);
    return static_cast<int64_t>(at)+5+displacement==target;
}
inline bool contains(uintptr_t address,size_t bytes=1,bool code=false){
    const auto& image=runtime.image();return address>=image.base&&image.contains(address-image.base,bytes,code);
}
}
