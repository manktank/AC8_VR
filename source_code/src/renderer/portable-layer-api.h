#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>

// ABI subset of Khronos openxr_loader_negotiation.h (MIT/Apache-2.0).
// No UEVR addresses, signatures, runtime discovery or private handle reads.
namespace portable_layer {
using Instance=void*;
using Result=int32_t;
using Function=void(__stdcall*)();
using GetProc=Result(__stdcall*)(Instance,const char*,Function*);
struct CreateInfo;
using Create=Result(__stdcall*)(const void*,const CreateInfo*,Instance*);
constexpr char name[]="XR_APILAYER_COLIN_ac8_glow";
constexpr uint64_t api_version=uint64_t(1)<<48;
struct NextInfo {
 int32_t type;uint32_t version;size_t size;char layer_name[256];
 GetProc get_proc;Create create;NextInfo* next;
};
struct CreateInfo {
 int32_t type;uint32_t version;size_t size;void* loader_instance;
 char settings[512];NextInfo* next;
};
struct LoaderInfo {
 int32_t type;uint32_t version;size_t size;uint32_t min_interface,max_interface;
 uint64_t min_api,max_api;
};
struct Request {
 int32_t type;uint32_t version;size_t size;uint32_t interface_version;
 uint64_t api;GetProc get_proc;Create create;
};
using Resolve=Result(__stdcall*)(Instance,const char*,Function*);
static_assert(sizeof(NextInfo)==296&&sizeof(CreateInfo)==544&&sizeof(LoaderInfo)==40&&sizeof(Request)==48);
}
