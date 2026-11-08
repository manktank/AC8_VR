#pragma once
#include "game-compatibility.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>
#include <set>

namespace ac8_native_resolution {
using Image=ac8_game_compatibility::Image;
struct Reference {uint32_t at=0,end=0,target=0;unsigned char size=0;bool code=false,module_rva=false;uint32_t symbol=0;};
struct Pattern {
    const char* name=nullptr;uint32_t symbol=0,reference_rva=0;
    const unsigned char* bytes=nullptr;const unsigned char* mask=nullptr;uint32_t size=0,anchor=0,anchor_size=0;
    const Reference* references=nullptr;uint32_t reference_count=0;uint32_t groups=0;
};
enum class State:uint32_t {unrequested,missing,unique,ambiguous,invalid_reference};
struct Match {State state=State::unrequested;uint32_t rva=0;};
struct Metrics {uint64_t scans=0,scan_bytes=0,candidate_checks=0,proof_bytes=0,cache_hits=0;};
inline bool masked_equal(const unsigned char* actual,const Pattern& p){
    for(uint32_t i=0;i<p.size;++i)if((actual[i]&p.mask[i])!=(p.bytes[i]&p.mask[i]))return false;
    return true;
}
inline bool valid_pattern(const Pattern& p){
    if(!p.bytes||!p.mask||!p.size||p.size>65536||!p.anchor_size||p.anchor>=p.size||p.anchor_size>p.size-p.anchor||(!p.references&&p.reference_count))return false;
    for(uint32_t i=0;i<p.anchor_size;++i)if(p.mask[p.anchor+i]!=255)return false;
    for(uint32_t i=0;i<p.reference_count;++i){const auto& r=p.references[i];
        if((r.size!=1&&r.size!=4)||r.at>=p.size||r.size>p.size-r.at||r.end>p.size)return false;
        for(unsigned j=0;j<r.size;++j)if(p.mask[r.at+j])return false;
    }
    return true;
}
inline bool read_reference(const unsigned char* bytes,uint32_t location,const Reference& ref,uint32_t& target){
    if(ref.module_rva){if(ref.size!=4)return false;std::memcpy(&target,bytes+ref.at,4);return true;}
    int64_t displacement=0;
    if(ref.size==1){int8_t value=0;std::memcpy(&value,bytes+ref.at,1);displacement=value;}
    else if(ref.size==4){int32_t value=0;std::memcpy(&value,bytes+ref.at,4);displacement=value;}
    else return false;
    const auto absolute=static_cast<int64_t>(location)+ref.end+displacement;
    if(absolute<0||absolute>UINT32_MAX)return false;target=static_cast<uint32_t>(absolute);return true;
}
inline bool prove(const Image& image,const Pattern& pattern,uint32_t at,Metrics& metrics){
    if(!valid_pattern(pattern)||!image.contains(at,pattern.size,true))return false;
    std::vector<unsigned char> actual(pattern.size);SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+at),actual.data(),actual.size(),&copied)||copied!=actual.size())return false;
    metrics.proof_bytes+=actual.size();if(!masked_equal(actual.data(),pattern))return false;
    for(uint32_t i=0;i<pattern.reference_count;++i){uint32_t target=0;const auto& r=pattern.references[i];
        if(!read_reference(actual.data(),at,r,target))return false;
        if(!r.target&&!r.code&&!r.module_rva){if(target!=0)return false;continue;} // Proven image-base LEA.
        if(!image.contains(target,1,r.code))return false;
        if(r.target>=pattern.reference_rva&&r.target-pattern.reference_rva<pattern.size&&target!=at+(r.target-pattern.reference_rva))return false;
    }
    return true;
}
// Search only executable sections, reading bounded chunks once. All requested
// patterns share those reads. A recipe's fixed instruction bytes find candidates;
// its entire masked fragment and typed relative references validate them.
// Ambiguous results are never resolved by proximity or a uniform address delta.
inline bool scan(const Image& image,const Pattern* patterns,size_t count,uint32_t demand,std::vector<Match>& matches,Metrics& metrics){
    matches.assign(count,{});std::vector<size_t> active;uint32_t overlap=0;
    // Retain alternatives until independent call edges have been checked. A
    // short wrapper may have several byte-identical copies in unrelated types.
    // Candidate overflow stays ambiguous; a truncated set can never prove one.
    constexpr size_t candidate_limit=256;
    std::vector<std::vector<uint32_t>> candidates(count);std::vector<bool> overflow(count,false);
    for(size_t i=0;i<count;++i)if(patterns[i].groups&demand){
        matches[i].state=State::missing;
        if(!valid_pattern(patterns[i])||patterns[i].anchor_size<8)continue;
        active.push_back(i);overlap=std::max(overlap,patterns[i].size);
    }
    if(active.empty())return true;++metrics.scans;
    constexpr size_t chunk=1024*1024;
    std::vector<unsigned char> bytes(chunk+overlap);
    for(const auto& section:image.sections){
        if(!(section.flags&IMAGE_SCN_MEM_EXECUTE)||!(section.flags&IMAGE_SCN_MEM_READ))continue;
        for(uint32_t offset=0;offset<section.size;){
            const auto main=std::min<size_t>(chunk,section.size-offset);
            const auto length=std::min<size_t>(main+overlap,section.size-offset);SIZE_T read=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+section.rva+offset),bytes.data(),length,&read)||read!=length)return false;
            metrics.scan_bytes+=main;
            for(const auto index:active){const auto& p=patterns[index];
                if(overflow[index])continue;
                auto begin=bytes.begin(),end=begin+static_cast<ptrdiff_t>(length);
                const auto search=std::boyer_moore_horspool_searcher(p.bytes+p.anchor,p.bytes+p.anchor+p.anchor_size);
                while(begin<end){auto found=std::search(begin,end,search);if(found==end)break;
                    const auto anchor_at=static_cast<size_t>(found-bytes.begin());
                    if(anchor_at>=p.anchor){const auto start=anchor_at-p.anchor;
                        if(start<main&&p.size<=length-start){++metrics.candidate_checks;
                            if(masked_equal(bytes.data()+start,p)){
                                const auto at=section.rva+offset+static_cast<uint32_t>(start);
                                if(prove(image,p,at,metrics)){
                                    auto& list=candidates[index];
                                    if(std::find(list.begin(),list.end(),at)==list.end())list.push_back(at);
                                    if(list.size()>candidate_limit){overflow[index]=true;break;}
                                }
                            }
                        }
                    }
                    begin=found+1;
                }
            }
            offset+=static_cast<uint32_t>(main);
        }
    }
    bool changed=true;
    while(changed){changed=false;
        for(const auto i:active){if(overflow[i])continue;const auto& p=patterns[i];auto& list=candidates[i];
            const auto end=std::remove_if(list.begin(),list.end(),[&](uint32_t location){
                for(uint32_t k=0;k<p.reference_count;++k){const auto& ref=p.references[k];if(!ref.code)continue;
                    uint32_t target=0;std::array<unsigned char,4> operand{};SIZE_T copied=0;
                    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+location+ref.at),operand.data(),ref.size,&copied)||copied!=ref.size)return true;
                    auto relative=ref;relative.at=0;
                    if(!read_reference(operand.data(),location,relative,target))return true;
                    const auto symbol=ref.symbol?ref.symbol:ref.target;
                    bool constrained=false,accepted=false;
                    for(const auto j:active){const auto& other=patterns[j];
                        if(i==j||symbol<other.symbol||symbol-other.symbol>=other.size)continue;
                        constrained=true;if(overflow[j]){accepted=true;continue;}
                        for(auto hit:candidates[j])if(target==hit+(symbol-other.symbol)){accepted=true;break;}
                    }
                    if(constrained&&!accepted)return true;
                }
                return false;
            });
            if(end!=list.end()){list.erase(end,list.end());changed=true;}
        }
    }
    for(const auto i:active){const auto& list=candidates[i];matches[i]=overflow[i]||list.size()>1?Match{State::ambiguous,0}:list.empty()?Match{State::missing,0}:Match{State::unique,list[0]};}
    return true;
}
struct FileIdentity {
    uint64_t volume=0,size=0,write=0,change=0;std::array<unsigned char,16> file{};
    bool operator==(const FileIdentity& rhs)const{return volume==rhs.volume&&size==rhs.size&&write==rhs.write&&change==rhs.change&&file==rhs.file;}
};
inline bool identity(HANDLE file,FileIdentity& out){
    FILE_ID_INFO id{};FILE_BASIC_INFO basic{};FILE_STANDARD_INFO standard{};
    if(file==INVALID_HANDLE_VALUE||!GetFileInformationByHandleEx(file,FileIdInfo,&id,sizeof(id))||
       !GetFileInformationByHandleEx(file,FileBasicInfo,&basic,sizeof(basic))||!GetFileInformationByHandleEx(file,FileStandardInfo,&standard,sizeof(standard)))return false;
    if(standard.EndOfFile.QuadPart<0||standard.Directory)return false;
    out.volume=id.VolumeSerialNumber;out.size=static_cast<uint64_t>(standard.EndOfFile.QuadPart);
    out.write=static_cast<uint64_t>(basic.LastWriteTime.QuadPart);out.change=static_cast<uint64_t>(basic.ChangeTime.QuadPart);
    std::memcpy(out.file.data(),id.FileId.Identifier,out.file.size());return true;
}
// The EXE identity is a CACHE KEY, never a compatibility allowlist. Unchanged
// file ID/size/write/change metadata reuses previously resolved RVAs. A changed
// key triggers resolution again instead of disabling the whole mod. We also
// check the current mapped instructions on every cache hit: file metadata alone
// cannot authorize a native call (another plugin can patch loaded code).
struct DiskCache {
    FileIdentity file{};std::string recipe;uint32_t coverage=0;std::vector<Match> matches;
};
inline uint64_t checksum(const std::vector<unsigned char>& data){uint64_t h=14695981039346656037ull;for(const auto byte:data){h^=byte;h*=1099511628211ull;}return h;}
inline void put(std::vector<unsigned char>& out,uint64_t value,unsigned count){for(unsigned i=0;i<count;++i)out.push_back(static_cast<unsigned char>(value>>(8*i)));}
inline bool load(const std::filesystem::path& path,const FileIdentity& file,const std::string& recipe,size_t count,DiskCache& out){
    std::error_code error;const auto size=std::filesystem::file_size(path,error);
    if(error||size>1024*1024||size<136||count>4096||recipe.size()!=64)return false;
    std::ifstream stream(path,std::ios::binary);std::vector<unsigned char> data(static_cast<size_t>(size));
    if(!stream.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size())))return false;
    size_t cursor=0;bool valid=true;
    const auto get=[&](unsigned bytes){uint64_t value=0;if(bytes>8||cursor>data.size()||bytes>data.size()-cursor){valid=false;return value;}for(unsigned i=0;i<bytes;++i)value|=static_cast<uint64_t>(data[cursor++])<<(8*i);return value;};
    if(get(8)!=0x3248434143384341ull)return false;
    FileIdentity stored;stored.volume=get(8);stored.size=get(8);stored.write=get(8);stored.change=get(8);for(auto& byte:stored.file)byte=static_cast<unsigned char>(get(1));
    std::string stored_recipe;for(unsigned i=0;i<64;++i)stored_recipe.push_back(static_cast<char>(get(1)));
    const auto coverage=static_cast<uint32_t>(get(4));const auto rows=get(4);
    if(!valid||!(stored==file)||stored_recipe!=recipe||rows!=count||size!=8+48+64+8+count*8+8)return false;
    std::vector<Match> matches(count);
    for(auto& match:matches){const auto state=get(4);match.rva=static_cast<uint32_t>(get(4));if(state>static_cast<uint32_t>(State::invalid_reference))return false;match.state=static_cast<State>(state);}
    const auto at=cursor;const auto expected=get(8);data.resize(at);
    if(!valid||expected!=checksum(data))return false;out={stored,stored_recipe,coverage,std::move(matches)};return true;
}
inline bool save(const std::filesystem::path& path,const DiskCache& cache){
    if(cache.recipe.size()!=64||cache.matches.size()>4096)return false;
    std::vector<unsigned char> data;put(data,0x3248434143384341ull,8);
    put(data,cache.file.volume,8);put(data,cache.file.size,8);put(data,cache.file.write,8);put(data,cache.file.change,8);for(auto byte:cache.file.file)put(data,byte,1);
    for(unsigned char byte:cache.recipe)put(data,byte,1);put(data,cache.coverage,4);put(data,cache.matches.size(),4);
    for(const auto& match:cache.matches){put(data,static_cast<uint32_t>(match.state),4);put(data,match.rva,4);}put(data,checksum(data),8);
    auto temporary=path;temporary+=L".tmp";std::error_code error;
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    {std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);if(!stream.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size())))return false;}
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
inline bool usable_cache(size_t count,uint32_t demand,const DiskCache& cache){
    if(cache.matches.size()!=count||(cache.coverage&demand)!=demand)return false;
    return true;
}
struct Resolution {std::vector<Match> locations,matches;Metrics metrics;bool complete=false,from_cache=false,cache_saved=false;};
// The native consumers retain their reviewed source RVAs as SYMBOL KEYS. They
// are never added to the game base directly. Translate a key only through a
// proved instruction span or the actual typed reference it contains; do not
// extrapolate a common delta into unproved code/data between those spans.
class Bindings {
    struct Span {uint32_t symbol=0,rva=0,size=0;const unsigned char* source=nullptr;};
    std::vector<Span> spans_;
    std::map<uint32_t,uint32_t> references_;
    std::set<uint32_t> conflicts_;
public:
    void initialize(const Image& image,const Pattern* patterns,size_t count,uint32_t demand,const std::vector<Match>& matches){
        spans_.clear();references_.clear();conflicts_.clear();
        if(matches.size()!=count)return;
        for(size_t i=0;i<count;++i){const auto& p=patterns[i];if(!(p.groups&demand)||matches[i].state!=State::unique)continue;
            const auto at=matches[i].rva;spans_.push_back({p.symbol,at,p.size,p.bytes});
            std::vector<unsigned char> bytes(p.size);SIZE_T copied=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+at),bytes.data(),bytes.size(),&copied)||copied!=bytes.size()){spans_.pop_back();continue;}
            for(uint32_t k=0;k<p.reference_count;++k){const auto& ref=p.references[k];uint32_t target=0;
                const auto key=ref.symbol?ref.symbol:ref.target;if(!key||!read_reference(bytes.data(),at,ref,target))continue;
                const auto [it,inserted]=references_.try_emplace(key,target);if(!inserted&&it->second!=target)conflicts_.insert(key);
            }
        }
    }
    uint32_t rva(uint32_t key)const{
        if(conflicts_.count(key))return 0;uint32_t result=0;
        const auto accept=[&](uint32_t candidate){if(result&&result!=candidate)return false;result=candidate;return true;};
        for(const auto& span:spans_)if(key>=span.symbol&&key-span.symbol<span.size)
            if(!accept(span.rva+(key-span.symbol)))return 0;
        const auto it=references_.find(key);if(it!=references_.end()&&!accept(it->second))return 0;
        return result;
    }
    bool proof(uint32_t key,const void* source,size_t size)const{
        if(!source||!size||!rva(key))return false;
        for(const auto& span:spans_)if(key>=span.symbol&&key-span.symbol<span.size&&size<=span.size-(key-span.symbol)&&
            !std::memcmp(source,span.source+(key-span.symbol),size))return true;
        return false;
    }
    bool proof_at(uint32_t actual,const void* source,size_t size)const{
        for(const auto& span:spans_)if(actual>=span.rva&&actual-span.rva<span.size&&size<=span.size-(actual-span.rva)&&
            rva(span.symbol+(actual-span.rva))==actual&&!std::memcmp(source,span.source+(actual-span.rva),size))return true;
        return false;
    }
};
inline void validate_graph(const Image& image,const Pattern* patterns,size_t count,uint32_t demand,std::vector<Match>& matches){
    // A masked CALL displacement is movable, not unconstrained. Recheck shared
    // code/data bindings on cache hits too. Byte equality alone would miss an
    // in-memory patch which redirected a still well-formed relative instruction.
    bool changed=true;
    while(changed){changed=false;std::map<uint32_t,std::pair<uint32_t,std::vector<size_t>>> targets;std::set<size_t> invalid;
        const auto add=[&](uint32_t symbol,uint32_t target,size_t owner){
            if(!symbol)return;auto [it,inserted]=targets.try_emplace(symbol,std::make_pair(target,std::vector<size_t>{}));
            if(!inserted&&it->second.first!=target){invalid.insert(owner);invalid.insert(it->second.second.begin(),it->second.second.end());}
            it->second.second.push_back(owner);
        };
        for(size_t i=0;i<count;++i)if((patterns[i].groups&demand)&&matches[i].state==State::unique){
            const auto& p=patterns[i];add(p.symbol,matches[i].rva,i);
            std::vector<unsigned char> bytes(p.size);SIZE_T copied=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image.base+matches[i].rva),bytes.data(),bytes.size(),&copied)||copied!=bytes.size()){invalid.insert(i);continue;}
            for(uint32_t k=0;k<p.reference_count;++k){const auto& ref=p.references[k];uint32_t target=0;
                if(!read_reference(bytes.data(),matches[i].rva,ref,target)){invalid.insert(i);continue;}
                const auto symbol=ref.symbol?ref.symbol:ref.target;add(symbol,target,i);
                bool constrained=false,accepted=false;
                for(size_t j=0;j<count;++j){const auto& other=patterns[j];
                    if(!(other.groups&demand)||symbol<other.symbol||symbol-other.symbol>=other.size)continue;
                    constrained=true;if(matches[j].state==State::unique&&target==matches[j].rva+(symbol-other.symbol))accepted=true;
                }
                if(constrained&&!accepted)invalid.insert(i);
            }
        }
        for(auto i:invalid)if(matches[i].state==State::unique){matches[i]={State::invalid_reference,0};changed=true;}
    }
}
inline void validate_loaded(const Image& image,const Pattern* patterns,size_t count,uint32_t demand,Resolution& result){
    result.matches=result.locations;
    for(size_t i=0;i<count;++i)if((patterns[i].groups&demand)&&result.matches[i].state==State::unique)
        if(!prove(image,patterns[i],result.matches[i].rva,result.metrics))result.matches[i]={State::invalid_reference,0};
    validate_graph(image,patterns,count,demand,result.matches);
}
inline Resolution resolve(const Image& image,const Pattern* patterns,size_t count,uint32_t demand,
                          const std::filesystem::path& executable,const std::filesystem::path& cache_file,const std::string& recipe){
    Resolution result;
    const auto file=CreateFileW(executable.c_str(),GENERIC_READ|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    struct Close {HANDLE file;~Close(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}} close{file};
    FileIdentity before{},after{};const bool identifiable=identity(file,before);
    DiskCache cached;
    if(identifiable&&load(cache_file,before,recipe,count,cached)&&usable_cache(count,demand,cached)&&
       identity(file,after)&&before==after){
        result.locations=std::move(cached.matches);result.complete=result.from_cache=true;++result.metrics.cache_hits;
        validate_loaded(image,patterns,count,demand,result);return result;
    }
    // Search the physical EXE, not a copy another loaded plugin may already
    // have detoured. Cache locations for that file; validate loaded instructions
    // separately before use. An in-memory hook must not poison the disk cache
    // with a permanent "function missing" verdict for an unchanged executable.
    const auto mapping=file==INVALID_HANDLE_VALUE?nullptr:CreateFileMappingW(file,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr);
    const auto* view=mapping?MapViewOfFile(mapping,FILE_MAP_READ,0,0,0):nullptr;
    struct Unmap {const void* view;HANDLE mapping;~Unmap(){if(view)UnmapViewOfFile(view);if(mapping)CloseHandle(mapping);}} unmap{view,mapping};
    const auto source=ac8_game_compatibility::inspect(reinterpret_cast<uintptr_t>(view));
    result.complete=source.base&&scan(source,patterns,count,demand,result.locations,result.metrics);
    if(!result.complete){
        // An unreadable code chunk could hide a second match. Partial search
        // results cannot authorize native calls, even when one match was found.
        result.locations.resize(count);for(auto& match:result.locations)match={State::invalid_reference,0};result.matches=result.locations;
        return result;
    }
    if(identifiable&&identity(file,after)&&before==after)
        result.cache_saved=save(cache_file,{before,recipe,demand,result.locations});
    validate_loaded(image,patterns,count,demand,result);
    // A cache-write failure is not incompatibility: current-process resolution
    // remains usable after its proofs pass. Missing file identity merely means
    // we cannot persist mappings safely for a later process.
    return result;
}
}
