#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include "../vendor/uevr/plugin-api.h"

// Forward the unchanged host API to the feature payload. The payload stays
// loaded for process lifetime because it can own host callbacks and hooks.
namespace ac8_loader {
static HMODULE payload=nullptr;
static bool initialize(const UEVR_PluginInitializeParam* p){
    if(!p||!p->version||p->version->major!=UEVR_PLUGIN_VERSION_MAJOR||p->version->minor<UEVR_PLUGIN_VERSION_MINOR||
       !p->functions||!p->functions->get_persistent_dir||!p->callbacks||!p->callbacks->on_custom_event)return false;
    if(!payload){
        HMODULE self=nullptr;wchar_t filename[32768]{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
             reinterpret_cast<LPCWSTR>(&initialize),&self)||!GetModuleFileNameW(self,filename,32768))return false;
        const auto path=std::filesystem::path(filename).parent_path()/L"AC8_VR-Features.bin";
        payload=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!payload){if(p->functions->log_error)p->functions->log_error("[AC8] Feature payload could not load; error=%lu",GetLastError());return false;}
    }
    const auto init=reinterpret_cast<bool(*)(const UEVR_PluginInitializeParam*)>(GetProcAddress(payload,"uevr_plugin_initialize"));
    const auto version=reinterpret_cast<void(*)(UEVR_PluginVersion*)>(GetProcAddress(payload,"uevr_plugin_required_version"));
    if(!init||!version)return false;
    UEVR_PluginVersion required{};version(&required);
    if(required.major!=p->version->major||required.minor>p->version->minor)return false;
    return init(p);
}
}
extern "C" __declspec(dllexport) void uevr_plugin_required_version(UEVR_PluginVersion* v){if(v)*v={UEVR_PLUGIN_VERSION_MAJOR,UEVR_PLUGIN_VERSION_MINOR,UEVR_PLUGIN_VERSION_PATCH};}
extern "C" __declspec(dllexport) bool uevr_plugin_initialize(const UEVR_PluginInitializeParam* p){return ac8_loader::initialize(p);}
