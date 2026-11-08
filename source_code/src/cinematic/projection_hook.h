#pragma once
#include "lens_projection.h"
#include "hangar_near_clip.h"
#include "bomb_projection.h"
#include "bomb_circle.h"
namespace cinematic_projection {
using Function=void*(*)(void*,void*,int);
inline std::atomic<Function> original{nullptr};
inline std::atomic<bool> installed{false},allowed{false};
inline std::atomic<bool> attempted{false};

inline std::atomic<const char*> state{"not_observed"};
inline std::mutex install_mutex;
inline thread_local bool double_precision=false;
inline void* wrapper(void* device,void* output,int index){
    const auto fn=original.load();if(!fn)return output;
    auto* result=fn(device,output,index);cinematic_lens::Fit fit{};
    if(result==output&&result&&double_precision){
        auto& matrix=*reinterpret_cast<UEVR_Matrix4x4d*>(result);
        if(cinematic_camera::enabled&&cinematic_lens::expanded_fit(fit)&&cinematic_lens::apply_expanded(matrix.m,fit)){}
        else if(cinematic_lens::apply_full_view(matrix.m)){}
        else if(cinematic_camera::enabled&&cinema_mask::requested&&cinematic_lens::current(fit)&&cinematic_lens::apply(matrix.m,fit)){}
        hangar_near_clip::apply(matrix.m,cinematic_camera::enabled&&cinema_mask::hangar_crop);}
    return result;
}
inline void verify(){
    // File identity says nothing about this virtual interface. Preflight checks
    // image structure; attach_guarded must ALSO prove the live vtable references
    // and UEVR's double-precision callback before changing any pointer.
    allowed=ac8_game_compatibility::allowed("stereo_projection");
    state=allowed?"waiting_stereo_vtable":"invalid_game_image";
}
// Recorded live UEVR log: projection slot12, game RVA04BBF060, UE5 double ABI.
// Change one pointer, forward through the existing UEVR hook, adjust AFTER it returns.
inline bool attach_guarded(void* device){
    __try {
        auto** table=*reinterpret_cast<void***>(device);auto** slot=table+12;
        auto* expected=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+ac8_game_compatibility::rva<0x04bbf060>();
        // A matching address alone is insufficient: require the companion
        // AdjustViewRect reference from the observed stereo interface as well.
        // A moved/reorganized interface is refused; no vtable slot guessing.
        bool private_copy = false;
        if (!ac8_game_compatibility::stereo_vtable(ac8_game_compatibility::runtime.image(),
                reinterpret_cast<uintptr_t>(table), double_precision, ac8_game_compatibility::rva<0x4bbf060>(),
                ac8_game_compatibility::rva<0x4bbe970>(), &private_copy) || *slot != expected) {
            return false;
        }
        // The verified UEVR copy is already writable. Avoid changing the
        // protection of a heap page that may hold other allocations.
        DWORD protection = 0;
        if (!private_copy && !VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
            return false;
        }
        original=reinterpret_cast<Function>(expected);
        const auto previous=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&wrapper),expected);
        if (!private_copy) {
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(void*), protection, &ignored);
        }
        if(previous!=expected){original=nullptr;return false;}
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void early(UEVR_StereoRenderingDeviceHandle device,int,float,UEVR_Vector3f*,UEVR_Rotatorf*,bool precision){
    double_precision=precision;
    if(!allowed||installed||attempted||!precision||!device)return;
    std::lock_guard<std::mutex> guard(install_mutex);if(installed||attempted)return;
    // The live device is first available here. Check it once, not once per eye
    // forever when an update has changed the interface. Refusal lasts until
    // restart; compatibility discovery is never a recurring frame workload.
    attempted=true;
    HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&wrapper),&pinned);
    if(!pinned)return;
    installed=attach_guarded(device);state=installed?"projection_slot_installed":"projection_slot_mismatch";
}
inline void control(const char* name,const char* data){
    hangar_near_clip::control(name,data);
    if(!name||!data)return;
    std::lock_guard<std::mutex> guard(cinematic_lens::mutex);
    if(std::strcmp(name,"AC8_CinematicLensMode")==0)cinematic_lens::requested=std::strcmp(data,"on")==0;
    if(std::strcmp(name,"AC8_CinematicLensFullView")==0)cinematic_lens::full_view=std::strcmp(data,"on")==0;
    if(std::strcmp(name,"AC8_CinematicExpanded")==0){cinematic_lens::expanded=std::strcmp(data,"on")==0;cinematic_lens::view={};}
    if(std::strcmp(name,"AC8_CinematicLens")==0){char* end=nullptr;const double fov=std::strtod(data,&end);
        if(end&&*end=='|'){const double aspect=std::strtod(end+1,nullptr);if(std::isfinite(fov+aspect)&&fov>=.25&&fov<=170&&aspect>=.5&&aspect<=5)cinematic_lens::lens={fov,aspect};}}
}
}
