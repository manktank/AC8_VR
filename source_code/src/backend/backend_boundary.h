#pragma once
#include <bcrypt.h>
#include <vector>
#include <string>
#include <mutex>
#include <array>

namespace boundary {
constexpr DWORD wrapper_rva=0x352a50, expected_timestamp=1790232505, expected_size=12853248;
constexpr char expected_hash[]="70CCBAAF4C357DEAACC3990FD01A911A029D31DCCF0E5ABC01C7A9073E4F50C1";
constexpr unsigned char signature[]={0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x81,0xec,0xb0,0,0,0,0x48,0x8b,0x05,0x1c,0x46,0x82,0,0x48,0x33,0xc4,0x48,0x89,0x84,0x24,0xa8,0,0,0,0x48};

// Hash a file without loading or executing it. This protects the private RVA.
inline bool hash_open_file(HANDLE file,std::string& result) {
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD bytes=0,object_size=0;bool ok=false;
    std::vector<unsigned char> object,chunk(65536);unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
       BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&bytes,0)>=0) {
        object.resize(object_size);
        if(BCryptCreateHash(alg,&hash,object.data(),object_size,nullptr,0,0)>=0) {
            ok=true;
            for(;;) {
                if(!ReadFile(file,chunk.data(),static_cast<DWORD>(chunk.size()),&bytes,nullptr)){ok=false;break;}
                if(!bytes)break;
                if(BCryptHashData(hash,chunk.data(),bytes,0)<0){ok=false;break;}
            }
            if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
        }
    }
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);
    if(ok){static const char hex[]="0123456789ABCDEF";result.clear();for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}}
    return ok;
}
inline bool file_hash(const wchar_t* path,std::string& result) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    const bool ok=hash_open_file(file,result);CloseHandle(file);return ok;
}
struct HashIdentity {
    HMODULE module=nullptr;std::wstring path;
    FILE_ID_INFO id{};FILE_BASIC_INFO basic{};FILE_STANDARD_INFO standard{};
    bool operator==(const HashIdentity& other)const {
        return module==other.module&&path==other.path&&id.VolumeSerialNumber==other.id.VolumeSerialNumber&&
            !std::memcmp(id.FileId.Identifier,other.id.FileId.Identifier,sizeof(id.FileId.Identifier))&&
            basic.LastWriteTime.QuadPart==other.basic.LastWriteTime.QuadPart&&basic.ChangeTime.QuadPart==other.basic.ChangeTime.QuadPart&&
            standard.EndOfFile.QuadPart==other.standard.EndOfFile.QuadPart;
    }
};
inline bool hash_identity(HANDLE file,HMODULE module,HashIdentity& key){
    wchar_t path[32768]{};const auto n=GetFinalPathNameByHandleW(file,path,32768,FILE_NAME_NORMALIZED);
    if(!n||n>=32768||!GetFileInformationByHandleEx(file,FileIdInfo,&key.id,sizeof(key.id))||
       !GetFileInformationByHandleEx(file,FileBasicInfo,&key.basic,sizeof(key.basic))||
       !GetFileInformationByHandleEx(file,FileStandardInfo,&key.standard,sizeof(key.standard)))return false;
    key.module=module;key.path.assign(path,n);return true;
}
inline bool cached_module_file_hash(HMODULE module,const wchar_t* path,std::string& result){
    if(!module)return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    struct Handle {HANDLE value;~Handle(){CloseHandle(value);}} handle{file};
    HashIdentity before{};
    // Filesystems lacking these identity APIs retain the original uncached path.
    if(!hash_identity(file,module,before))return hash_open_file(file,result);
    struct Entry {HashIdentity key;std::string hash;bool used=false;};
    static std::mutex guard;static std::array<Entry,8> cache{};
    std::lock_guard<std::mutex> lock(guard);
    if(!hash_identity(file,module,before))return hash_open_file(file,result);
    for(const auto& entry:cache)if(entry.used&&entry.key==before){result=entry.hash;return true;}
    if(!hash_open_file(file,result))return false;
    HashIdentity after{};if(!hash_identity(file,module,after)||!(before==after)){result.clear();return false;}
    auto* destination=&cache[0];
    for(auto& entry:cache)if(!entry.used||entry.key.module==module){destination=&entry;break;}
    *destination={std::move(after),result,true};return true;
}
inline bool image_matches(const unsigned char* base) {
    __try {
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>4096)return false;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        return nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
            nt->FileHeader.TimeDateStamp==expected_timestamp && nt->OptionalHeader.SizeOfImage==expected_size &&
            memcmp(base+wrapper_rva,signature,sizeof(signature))==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
