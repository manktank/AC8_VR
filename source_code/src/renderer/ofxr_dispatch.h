#pragma once
#include "portable-layer-api.h"
#include <array>
#include <cstring>

// OFXR is an implicit layer above our portable explicit layer. Observe its
// application-facing calls so its private presentation frames/images stay
// outside the game's glow transaction. Only use its public negotiated ABI;
// OFXR is optional and no version, address, graphics device or size is assumed.
namespace ofxr_dispatch {
using Table=std::array<portable_layer::Function,12>;
inline constexpr const char* commands[]={
 "xrAcquireSwapchainImage","xrWaitSwapchainImage","xrReleaseSwapchainImage",
 "xrDestroySwapchain","xrBeginFrame","xrEndSession","xrCreateSession",
 "xrEndFrame","xrCreateSwapchain","xrEnumerateSwapchainImages",
 "xrBeginSession","xrDestroySession"};
enum class Outcome { absent,ready,incomplete };
using Negotiate=portable_layer::Result(__stdcall*)(const portable_layer::LoaderInfo*,const char*,portable_layer::Request*);
inline Outcome resolve(portable_layer::GetProc get,portable_layer::Instance instance,Table& result){
 result={};if(!get||!instance)return Outcome::absent;
 portable_layer::Function properties=nullptr;
 // A loaded DLL may not participate in this instance. Do not select it then.
 if(get(instance,"xrGetInstanceProperties",&properties)!=0||!properties)return Outcome::absent;
 Table candidate{};
 for(size_t i=0;i<candidate.size();++i)
  if(get(instance,commands[i],&candidate[i])!=0||!candidate[i])return Outcome::incomplete;
 result=candidate;return Outcome::ready;
}
inline Outcome from_negotiation(Negotiate negotiate,const char* layer,portable_layer::Instance instance,Table& result){
 result={};if(!negotiate)return Outcome::absent;
 const portable_layer::LoaderInfo loader{1,1,sizeof(portable_layer::LoaderInfo),1,1,portable_layer::api_version,portable_layer::api_version};
 portable_layer::Request request{2,1,sizeof(portable_layer::Request),0,0,nullptr,nullptr};
 if(negotiate(&loader,layer,&request)!=0||request.interface_version!=1||!request.get_proc)return Outcome::absent;
 return resolve(request.get_proc,instance,result);
}
inline Outcome find(portable_layer::Instance instance,Table& result){
 result={};
 // Public OFXR package/module and layer identities, shared by release builds.
 // GetModuleHandle never loads or enables OFXR for someone who did not use it.
 const wchar_t* modules[]={L"XR_APILAYER_OFXRBridge.dll",L"XR_APILAYER_XRFrameBridge_diagnostic.dll",L"XR_APILAYER_XRFrameBridge.dll"};
 const char* layers[]={"XR_APILAYER_OFXRBridge","XR_APILAYER_XRFrameBridge_diagnostic","XR_APILAYER_XRFrameBridge"};
 for(const auto* name:modules){
  const auto module=GetModuleHandleW(name);if(!module)continue;
  const auto negotiate=reinterpret_cast<Negotiate>(GetProcAddress(module,"xrNegotiateLoaderApiLayerInterface"));
  for(const auto* layer:layers){const auto status=from_negotiation(negotiate,layer,instance,result);if(status!=Outcome::absent)return status;}
 }
 return Outcome::absent;
}
}
