#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cstring>
#include "../vendor/uevr/plugin-api.h"

#include "module-selection.h"
#include "plane-view.h"
#include "hud/glow-discovery.h"
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
 const auto s=ac8_modules::from_directory(root);
 if(!s.core)return true;
 // Read once at native startup. No file polling, deferred hook registration or sibling requirements.
 const auto attempt=[&](const char* name,auto fn){
  try{if(!fn()&&p->functions->log_error)p->functions->log_error("[AC8] %s native initialization declined",name);}
  catch(...){if(p->functions->log_error)p->functions->log_error("[AC8 modular] %s native initialization failed; other features retained",name);}
 };
 if(s.cinematics||s.loading)attempt("presentation",[&]{return ac8_cinematic_selected_initialize(s.cinematics||s.loading?p:nullptr,s.cinematics?p:nullptr,s.loading?p:nullptr);});
 if(s.hud){attempt("HUD support",[&]{return ac8_hud_support_initialize(p);});attempt("HUD tick guard",[&]{return ac8_warning_initialize(p);});}
 if(s.targets){attempt("target layout",[&]{return ac8_target_support_initialize(p);});attempt("billboards",[&]{return ac8_escort_initialize(p);});attempt("arrows",[&]{return ac8_arrow_initialize(p);});attempt("lock reticle",[&]{return ac8_lock_initialize(p);});}
 if(s.glow)attempt("glow",[&]{glow_discovery::initialize(p);return ac8_glow_initialize(p);});
 if(s.plane)attempt("plane view bootstrap",[&]{return plane_view::initialize(p);});
 if(s.pause)attempt("pause support",[&]{return ac8_pause_support_initialize(p);});

 return true;
}
