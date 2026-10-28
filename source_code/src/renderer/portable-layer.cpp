#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "portable-layer-api.h"
#include "../../vendor/uevr/backend-api.h"
#include <array>
#include <cstring>
#include <cwchar>
#include <mutex>

namespace portable_layer {
struct Entry {Instance instance=nullptr;GetProc get_proc=nullptr;};
std::array<Entry,8> instances{};
std::recursive_mutex mutex;
constexpr Result invalid=-12,unsupported=-7,initialization_failed=-6;
GetProc find(Instance instance){for(const auto& entry:instances)if(entry.instance==instance)return entry.get_proc;return nullptr;}
Result __stdcall resolve(Instance instance,const char* command,Function* output){
 if(!output||!command)return -1;*output=nullptr;
 std::lock_guard<std::recursive_mutex> lock(mutex);
 const auto next=find(instance);return next?next(instance,command,output):invalid;
}
Result __stdcall destroy(Instance instance){
 std::lock_guard<std::recursive_mutex> lock(mutex);
 Function fn=nullptr;const auto next=find(instance);if(!next)return invalid;
 const Result lookup=next(instance,"xrDestroyInstance",&fn);if(lookup!=0||!fn)return lookup!=0?lookup:unsupported;
 const Result result=reinterpret_cast<Result(__stdcall*)(Instance)>(fn)(instance);
 if(result==0)for(auto& entry:instances)if(entry.instance==instance)entry={};
 return result;
}
Result __stdcall get_proc(Instance instance,const char* command,Function* output){
 if(!output||!command)return -1;*output=nullptr;
 if(!std::strcmp(command,"xrGetInstanceProcAddr")){*output=reinterpret_cast<Function>(&get_proc);return 0;}
 if(!std::strcmp(command,"xrDestroyInstance")){*output=reinterpret_cast<Function>(&destroy);return 0;}
 return resolve(instance,command,output);
}
Result __stdcall create(const void* info,const CreateInfo* layer,Instance* instance){
 if(!layer||!instance||layer->type!=4||layer->version!=1||layer->size!=sizeof(CreateInfo)||!layer->next)return initialization_failed;
 const auto* next=layer->next;
 if(next->type!=5||next->version!=1||next->size!=sizeof(NextInfo)||
    std::strcmp(next->layer_name,name)||!next->get_proc||!next->create)return initialization_failed;
 std::lock_guard<std::recursive_mutex> lock(mutex);
 Entry* vacant=nullptr;for(auto& entry:instances)if(!entry.instance){vacant=&entry;break;}
 if(!vacant)return initialization_failed;
 auto forwarded=*layer;forwarded.next=next->next;
 const Result result=next->create(info,&forwarded,instance);
 if(result==0&&*instance)*vacant={*instance,next->get_proc};
 return result;
}
// Environment changes are process-local, before UEVR creates its instance.
// Preserve other API layers and paths. No registry or global runtime changes.
bool append_environment(const wchar_t* key,const wchar_t* value){
 wchar_t old[16384]{};const DWORD length=GetEnvironmentVariableW(key,old,16384);
 if(length>=16384)return false;
 if(length&&std::wcsstr(old,value))return true;
 wchar_t combined[32768]{};
 if(length){if(wcscpy_s(combined,old)||wcscat_s(combined,L";"))return false;}
 if(wcscat_s(combined,value))return false;
 return SetEnvironmentVariableW(key,combined)!=FALSE;
}
bool bootstrap(HMODULE module){
 wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return false;
 const auto* leaf=std::wcsrchr(executable,L'\\');leaf=leaf?leaf+1:executable;
 if(_wcsicmp(leaf,L"AceCombat8.exe"))return true;
 wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return false;
 auto* slash=std::wcsrchr(path,L'\\');if(!slash)return false;*slash=0;
 if(wcscat_s(path,L"\\AC8_VR-OpenXRLayer"))return false;
 if(!append_environment(L"XR_API_LAYER_PATH",path))return false;
 return append_environment(L"XR_ENABLE_API_LAYERS",L"XR_APILAYER_COLIN_ac8_glow");
}
}
extern "C" __declspec(dllexport) int32_t __stdcall xrNegotiateLoaderApiLayerInterface(
 const portable_layer::LoaderInfo* loader,const char* layer,portable_layer::Request* request){
 using namespace portable_layer;
 if(!loader||!request||!layer||std::strcmp(layer,name)||loader->type!=1||loader->version!=1||loader->size!=sizeof(LoaderInfo)||
    request->type!=2||request->version!=1||request->size!=sizeof(Request)||loader->min_interface>1||loader->max_interface<1||
    loader->min_api>api_version||loader->max_api<api_version)return initialization_failed;
 request->interface_version=1;request->api=api_version;request->get_proc=get_proc;request->create=create;return 0;
}
extern "C" __declspec(dllexport) int32_t __stdcall ac8_openxr_layer_resolve(
 portable_layer::Instance instance,const char* command,portable_layer::Function* output){
 return portable_layer::resolve(instance,command,output);
}
extern "C" __declspec(dllexport) void uevr_plugin_required_version(UEVR_PluginVersion* v){if(v)*v={2,39,0};}
extern "C" __declspec(dllexport) bool uevr_plugin_initialize(const UEVR_PluginInitializeParam*){
 
 return true;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);portable_layer::bootstrap(module);}
 return TRUE;
}
