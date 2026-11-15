#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cstring>
#include "../vendor/uevr/plugin-api.h"

#include "module-selection.h"
#include "backend/game-compatibility-runtime.h"
#include "plane-view.h"
#include "hud/glow-discovery.h"
#include "runtime-log.h"
extern "C" bool ac8_cinematic_selected_initialize(const UEVR_PluginInitializeParam*,const UEVR_PluginInitializeParam*,const UEVR_PluginInitializeParam*);
extern "C" bool ac8_hud_support_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_target_support_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_pause_support_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_escort_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_glow_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_warning_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_arrow_initialize(const UEVR_PluginInitializeParam*);
extern "C" bool ac8_lock_initialize(const UEVR_PluginInitializeParam*);
extern "C" __declspec(dllexport) void uevr_plugin_required_version(UEVR_PluginVersion* v){if(v)*v={UEVR_PLUGIN_VERSION_MAJOR,UEVR_PLUGIN_VERSION_MINOR,UEVR_PLUGIN_VERSION_PATCH};}
extern "C" __declspec(dllexport) bool uevr_plugin_initialize(const UEVR_PluginInitializeParam* p){
 if(!p||!p->functions||!p->functions->get_persistent_dir||!p->callbacks||!p->callbacks->on_custom_event)return false;
 wchar_t root[32768]{};if(!p->functions->get_persistent_dir(root,32768))return false;
 p=ac8_runtime_log::initialize(p,root);
 // Replies to Lua-originated requests; without it they are dropped, never sent synchronously.
 if(!ac8_lua_events::initialize(p))ac8_runtime_log::error("[AC8] Deferred Lua event delivery unavailable; native replies disabled");
 const auto s=ac8_modules::from_directory(root);
 ac8_runtime_log::info("[AC8 modules] core=%d hud=%d targets=%d glow=%d cinematics=%d loading=%d pause=%d plane=%d",
  s.core,s.hud,s.targets,s.glow,s.cinematics,s.loading,s.pause,s.plane);
 if(!s.core){ac8_runtime_log::warn("[AC8] Core absent; no native features started");return true;}
 // Cache feature contracts before any module changes shared native entries.
 ac8_runtime_log::info("[AC8] Compatibility preflight begin");
 ac8_game_compatibility::preflight(p,{s.glow,s.targets,s.hud,s.pause,s.cinematics,s.loading});
 ac8_runtime_log::info("[AC8] Compatibility preflight complete");
 // Read once at native startup. No file polling, deferred hook registration or sibling requirements.
 const auto attempt=[&](const char* name,auto fn){
  ac8_runtime_log::info("[AC8] %s native initialization begin",name);
  try{if(fn())ac8_runtime_log::info("[AC8] %s native initialization complete",name);
   else ac8_runtime_log::error("[AC8] %s native initialization declined",name);}
  catch(...){if(p->functions->log_error)p->functions->log_error("[AC8 modular] %s native initialization failed; other features retained",name);}
 };
 if(s.cinematics||s.loading)attempt("presentation",[&]{return ac8_cinematic_selected_initialize(s.cinematics||s.loading?p:nullptr,s.cinematics?p:nullptr,s.loading?p:nullptr);});
 if(s.hud){attempt("HUD support",[&]{return ac8_hud_support_initialize(p);});attempt("HUD tick guard",[&]{return ac8_warning_initialize(p);});}
 if(s.targets){attempt("target layout",[&]{return ac8_target_support_initialize(p);});attempt("billboards",[&]{return ac8_escort_initialize(p);});attempt("arrows",[&]{return ac8_arrow_initialize(p);});attempt("lock reticle",[&]{return ac8_lock_initialize(p);});}
 if(s.glow)attempt("glow",[&]{glow_discovery::initialize(p);return ac8_glow_initialize(p);});
 if(s.plane)attempt("plane view bootstrap",[&]{return plane_view::initialize(p);});
 if(s.pause)attempt("pause support",[&]{return ac8_pause_support_initialize(p);});

 ac8_runtime_log::info("[AC8] Native startup complete");
 return true;
}
