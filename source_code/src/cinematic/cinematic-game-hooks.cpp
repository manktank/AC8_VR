#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <sstream>
#include "../../vendor/uevr/plugin-api.h"
#include "mask.h"
#include "camera_lock.h"
#include "projection_hook.h"
#include "world_marker_tone.h"
#include "world_target_layout.h"
#include "pause-navigation.h"
#include "bomb_render.h"

static const UEVR_PluginInitializeParam* api=nullptr;
static bool cinematic_selected=false;
static bool camera_available=false;
static void tick_shared(UEVR_UGameEngineHandle,float){
    static ULONGLONG next_mask_check=0;
    if(cinema_mask::host && GetTickCount64()>=next_mask_check) {
        next_mask_check=GetTickCount64()+2000;
        cinema_mask::attach();
        
        if(cinematic_selected){
        if(cinematic_projection::installed)api->functions->dispatch_lua_event("AC8_CinematicProjection_Ready","ready");
        if(cinema_mask::attached && !cinema_mask::failed) {
            if(camera_available)api->functions->dispatch_lua_event("AC8_CinematicCamera_Ready","ready");
            api->functions->dispatch_lua_event("AC8_CinematicMask_Ready","ready");
        }
        }
        if(cinema_mask::failed) api->functions->dispatch_lua_event("AC8_CinematicMask_Failed","failed");
    }
}


extern "C" bool ac8_cinematic_selected_initialize(const UEVR_PluginInitializeParam* shared,const UEVR_PluginInitializeParam* cinematic,const UEVR_PluginInitializeParam* loading){
    api=shared;
    if(!api||!api->sdk||!api->sdk->callbacks||!api->sdk->callbacks->on_pre_engine_tick||!api->callbacks||!api->callbacks->on_custom_event||!api->functions||!api->functions->dispatch_lua_event)return false;

    cinematic_selected=cinematic!=nullptr;
    bool ok=true;
    if(shared){
        cinema_mask::host=shared;
        ok=shared->sdk->callbacks->on_pre_engine_tick(tick_shared)&&ok;
        ok=shared->callbacks->on_custom_event(cinema_mask::control)&&ok;
    }
    if(cinematic){
        cinematic_camera::host=cinematic;
        const bool control=cinematic->callbacks->on_custom_event(cinematic_camera::control);
        const bool early=cinematic->sdk->callbacks->on_early_calculate_stereo_view_offset&&cinematic->sdk->callbacks->on_early_calculate_stereo_view_offset(cinematic_camera::early);
        const bool post=cinematic->sdk->callbacks->on_post_calculate_stereo_view_offset&&cinematic->sdk->callbacks->on_post_calculate_stereo_view_offset(cinematic_camera::post);
        camera_available=control&&early&&post;
        cinematic_projection::verify();
        const bool projection_control=cinematic->callbacks->on_custom_event(cinematic_projection::control);
        const bool projection_callback=cinematic->sdk->callbacks->on_early_calculate_stereo_view_offset&&cinematic->sdk->callbacks->on_early_calculate_stereo_view_offset(cinematic_projection::early);
        if(!projection_control||!projection_callback)cinematic_projection::allowed=false;
    }
    if(loading){native_fade::initialize();ok=loading->callbacks->on_custom_event(native_fade::control)&&ok;}
    return ok;
}
extern "C" bool ac8_hud_support_initialize(const UEVR_PluginInitializeParam* p){
    bomb_projection::initialize(p);bomb_circle::initialize(p);
    // Schema/build guards deliberately forward unchanged when unavailable.
    if(!bomb_render::initialize(p)&&p->functions->log_error)p->functions->log_error("[AC8] Bomb support unavailable; forwarding game rendering");
    return true;
}
extern "C" bool ac8_target_support_initialize(const UEVR_PluginInitializeParam* p){
    if(!world_target_layout::initialize(p)&&p->functions->log_error)p->functions->log_error("[AC8] Target layout unavailable; forwarding game rendering");return true;
}
extern "C" bool ac8_pause_navigation_initialize(const UEVR_PluginInitializeParam* p){
    auto sdk=p->sdk;
    if(!sdk||!sdk->uobject||!sdk->uobject->get_bool_property||!sdk->ustruct||!sdk->fname||!sdk->ffield||!sdk->ffield_class||!sdk->fproperty||!sdk->farrayproperty||!sdk->fstructproperty||!sdk->uscriptstruct||!sdk->uobject_array||!sdk->uobject_hook||!sdk->functions||!sdk->callbacks||!sdk->callbacks->on_pre_engine_tick)return false;
    world_marker_tone::host=p;pause_trace::host=p;
    return p->callbacks->on_custom_event(pause_trace::control)&&sdk->callbacks->on_pre_engine_tick(pause_trace::tick);
}
