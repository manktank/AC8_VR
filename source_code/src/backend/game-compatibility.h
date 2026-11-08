#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// Game updates can change resources, signatures or timestamps without changing
// a native interface we use. A whole-EXE hash therefore is NOT permission to run
// a feature, and a different hash is NOT evidence that that feature is broken.
// Check its own dependencies instead. This does not remove the caller's reflected
// schema, native ABI, object/Slate lifetime, render-state or queue/fence checks.
//
// The runtime adapter obtains coherent code/data/return-site bindings from the
// startup resolver. A partial pattern hit is never enough: full instruction,
// reference, source-proof and live owner/type checks remain necessary. The
// executable identity is a cache key, not a reason to reject a new game build.
namespace ac8_game_compatibility {
struct Section {uint32_t rva=0,size=0,flags=0;};
struct Image {
    uintptr_t base=0;uint32_t size=0;std::vector<Section> sections;
    bool contains(uintptr_t rva,size_t bytes,bool code=false)const{
        if(!base||!bytes||rva>=size||bytes>size-rva)return false;
        for(const auto& s:sections)if(rva>=s.rva&&rva-s.rva<s.size&&bytes<=s.size-(rva-s.rva)&&
            (s.flags&IMAGE_SCN_MEM_READ)&&(!code||(s.flags&IMAGE_SCN_MEM_EXECUTE)))return true;
        return false;
    }
};
template<class T>inline bool read(uintptr_t at,T& value){
    SIZE_T count=0;return at&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&value,sizeof(value),&count)&&count==sizeof(value);
}
// Structural PE parsing, not an exact build allowlist. The actual mapped size
// bounds all later reads; a changed image size/timestamp is not a rejection.
inline Image inspect(uintptr_t base){
    Image image;IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!read(base,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<static_cast<LONG>(sizeof(dos))||dos.e_lfanew>0x100000||
       base>UINTPTR_MAX-static_cast<uintptr_t>(dos.e_lfanew)||!read(base+dos.e_lfanew,nt)||nt.Signature!=IMAGE_NT_SIGNATURE||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
       nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||!nt.FileHeader.NumberOfSections||nt.FileHeader.NumberOfSections>96||
       nt.OptionalHeader.SizeOfImage<nt.OptionalHeader.SizeOfHeaders||!nt.OptionalHeader.SizeOfHeaders)return image;
    const auto table=static_cast<uintptr_t>(dos.e_lfanew)+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
    const auto bytes=static_cast<size_t>(nt.FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER);
    if(table>=nt.OptionalHeader.SizeOfHeaders||bytes>nt.OptionalHeader.SizeOfHeaders-table||base>UINTPTR_MAX-nt.OptionalHeader.SizeOfImage)return image;
    image.base=base;image.size=nt.OptionalHeader.SizeOfImage;
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
        IMAGE_SECTION_HEADER s{};
        if(!read(base+table+i*sizeof(s),s))return {};
        if(!s.Misc.VirtualSize)s.Misc.VirtualSize=s.SizeOfRawData;
        if(!s.Misc.VirtualSize)continue; // Empty unrelated sections are legal.
        if(s.VirtualAddress>=image.size||s.Misc.VirtualSize>image.size-s.VirtualAddress)return {};
        for(const auto& prior:image.sections)if(s.VirtualAddress<prior.rva+prior.size&&prior.rva<s.VirtualAddress+s.Misc.VirtualSize)return {};
        image.sections.push_back({s.VirtualAddress,s.Misc.VirtualSize,s.Characteristics});
    }
    return image;
}
inline bool stereo_vtable(const Image& image, uintptr_t table, bool double_precision,
                          uintptr_t projection_rva, uintptr_t adjust_rva, bool* private_copy = nullptr) {
    // UEVR supplies the actual stereo device and precision on the engine's
    // stereo callback. Validate its companion method references, not an
    // arbitrary function at a familiar address. This is a late interface proof,
    // separate from PE preflight; neither one alone authorizes a pointer write.
    if (!double_precision || !table || table % alignof(uintptr_t) != 0 ||
        !image.contains(projection_rva, 1, true) || !image.contains(adjust_rva, 1, true)) {
        return false;
    }
    constexpr auto table_bytes = 13 * sizeof(uintptr_t);
    const bool in_game_image = table >= image.base && image.contains(table - image.base, table_bytes);
    if (!in_game_image) {
        // UEVR can copy its active device's vtable into a process-lifetime
        // vector. The callback supplies the live device, so admit a private
        // writable copy only when both independently resolved methods match.
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<const void*>(table), &region, sizeof(region)) ||
            region.State != MEM_COMMIT || region.Type != MEM_PRIVATE || region.Protect != PAGE_READWRITE) {
            return false;
        }
        const auto start = reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (region.RegionSize < table_bytes || table < start || table - start > region.RegionSize - table_bytes) {
            return false;
        }
    }
    uintptr_t projection = 0, adjust = 0;
    if (!read(table + 12 * sizeof(uintptr_t), projection) ||
        !read(table + 8 * sizeof(uintptr_t), adjust) ||
        projection != image.base + projection_rva || adjust != image.base + adjust_rva) {
        return false;
    }
    if (private_copy) {
        *private_copy = !in_game_image;
    }
    return true;
}
struct Probe {const char* name;uint32_t rva;const unsigned char* bytes;size_t count;};
struct Contract {const char* name;const Probe* probes;size_t count;bool late_virtual_proof=false;};
struct Result {bool allowed=false;const char* reason="not_checked";const char* dependency="";uint32_t rva=0;size_t compared=0;};
inline Result verify(const Image& image,const Contract& contract){
    if(!image.base)return {false,"invalid_mapped_image","",0,0};
    if(!contract.count&&!contract.late_virtual_proof)return {false,"missing_dependency_contract","",0,0};
    size_t compared=0;
    for(size_t i=0;i<contract.count;++i){const auto& p=contract.probes[i];
        if(!p.bytes||!p.count||!image.contains(p.rva,p.count,true))return {false,"dependency_outside_executable_section",p.name,p.rva,compared};
        std::vector<unsigned char> actual(p.count);SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+p.rva),actual.data(),p.count,&copied)||copied!=p.count)
            return {false,"dependency_unreadable",p.name,p.rva,compared};
        compared+=p.count;
        if(std::memcmp(actual.data(),p.bytes,p.count))return {false,"dependency_instructions_changed",p.name,p.rva,compared};
    }
    return {true,contract.late_virtual_proof?"waiting_live_virtual_contract":"dependency_instructions_verified","",0,compared};
}
// Pure, injectable cache used by both the runtime and tests. Preflight must run
// BEFORE our first hook: two features may depend on the same original entry.
// Reading that entry after another feature detours it would create a false
// incompatibility. Cached results also keep scans/OS reads out of frame callbacks.
class Cache {
    std::vector<Result> results_;Image image_;bool initialized_=false;
public:
    size_t evaluations=0,compared=0;
    void initialize(Image image,const Contract* contracts,size_t count,const bool* enabled=nullptr){
        if(initialized_)return;initialized_=true;image_=std::move(image);results_.reserve(count);
        for(size_t i=0;i<count;++i){
            if(enabled&&!enabled[i]){results_.push_back({false,"feature_not_requested","",0,0});continue;}
            auto r=verify(image_,contracts[i]);compared+=r.compared;results_.push_back(r);++evaluations;
        }
    }
    const Image& image()const{return image_;}
    Result get(size_t index)const{return index<results_.size()?results_[index]:Result{false,"preflight_not_run","",0,0};}
};
}
