#pragma once
// TEMP Plan C: explicit bounded CPU/cadence comparison; never GPU work/waits.
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
namespace selective_glow_performance {
#ifdef AC8_DISABLE_GLOW_REPORTING
inline constexpr bool reporting = false;
#else
inline constexpr bool reporting = true;
#endif
inline uint64_t clock(){LARGE_INTEGER v{};QueryPerformanceCounter(&v);return static_cast<uint64_t>(v.QuadPart);}
// Keep the real clock available for discovery budgets and rendering guards.
// Use this clock only when the result is consumed by development reports.
inline uint64_t sample_clock(){return reporting ? clock() : 0;}
inline double frequency(){LARGE_INTEGER v{};QueryPerformanceFrequency(&v);return static_cast<double>(v.QuadPart);}
struct Samples {
    std::array<uint64_t,8192> ticks{};
    size_t count=0;uint64_t dropped=0;
    void add(uint64_t value){if(count<ticks.size())ticks[count++]=value;else ++dropped;}
};
struct Phase {
    Samples engine_intervals,present_intervals,paint_cpu,wrapper_cpu;
    uint64_t begin=0,end=0,last_engine=0,last_present=0;
};
struct Session {
    std::mutex mutex;
    std::array<Phase,2> phases;
    unsigned phase=0;
    std::atomic<uint64_t> in_flight{0};
    std::string id;
};
inline std::shared_ptr<Session> current;
inline std::filesystem::path directory;
inline unsigned sequence=0;
inline void samples_json(std::ostream& out,const Samples& values,double scale){
    out<<"{\"samples\":"<<values.count<<",\"dropped\":"<<values.dropped;
    if(values.count){
        std::vector<uint64_t> sorted(values.ticks.begin(),values.ticks.begin()+values.count);std::sort(sorted.begin(),sorted.end());
        long double sum=0;for(const auto v:sorted)sum+=v;
        const auto percentile=[&](size_t percent){return sorted[(sorted.size()*percent+99)/100-1]*scale;};
        out<<",\"mean\":"<<static_cast<double>(sum/values.count)*scale<<",\"p50\":"<<percentile(50)<<",\"p95\":"<<percentile(95)<<",\"max\":"<<sorted.back()*scale;
    }
    out<<'}';
}
inline void save(const std::shared_ptr<Session>& session,const char* reason){
    if constexpr(!reporting)return;
    if(!session||directory.empty())return;
    std::lock_guard<std::mutex> guard(session->mutex);
    const double hz=frequency();if(hz<=0)return;
    std::ostringstream out;out<<"{\"version\":1,\"test\":\"native_radio_paint_marker\",\"id\":\""<<session->id<<"\",\"reason\":\""<<reason<<"\",\"inFlightPaintSamplesAtSave\":"<<session->in_flight.load()<<",\"phases\":[";
    for(unsigned i=0;i<2;++i){if(i)out<<',';const auto& p=session->phases[i];
        out<<"{\"name\":\""<<(i?"marker_on":"marker_off_baseline")<<"\",\"seconds\":"<<(p.begin?static_cast<double>((p.end?p.end:clock())-p.begin)/hz:0)
            <<",\"engineTickIntervalMs\":";samples_json(out,p.engine_intervals,1000/hz);
        out<<",\"uevrPresentIntervalMs\":";samples_json(out,p.present_intervals,1000/hz);
        out<<",\"allRichTextOriginalPaintCpuUs\":";samples_json(out,p.paint_cpu,1000000/hz);
        out<<",\"observedRichTextWrapperCpuUs\":";samples_json(out,p.wrapper_cpu,1000000/hz);out<<'}';
    }
    out<<"],\"scope\":\"CPU/cadence comparison of marker off/on with the observer hook installed in both. Paint samples include all SRichTextBlock calls, not just radio. Wrapper timing excludes its own final metrics aggregation and initial metrics lookup. Engine/present arrivals are pacing proxies, not headset FPS or GPU duration. First 8192 samples per series/phase; no automatic phase timing. Different scene/radio activity or frame pacing can dominate differences. This does not measure a future separated glow/sharp renderer or compare with an unhooked executable.\",\"gpuCommandsAdded\":false,\"gpuTimingMeasured\":false}\n";
    const auto body=out.str();std::ofstream(directory/L"AC8_VR-SelectiveGlowPerformance-state.json",std::ios::binary)<<body;
    std::ofstream(directory/("AC8_VR-SelectiveGlowPerformance-"+session->id+".json"),std::ios::binary)<<body;
}
inline void stop(const char* reason){
    if constexpr(!reporting)return;
    auto session=std::atomic_exchange(&current,std::shared_ptr<Session>{});if(!session)return;
    {std::lock_guard<std::mutex> guard(session->mutex);session->phases[session->phase].end=clock();}
    save(session,reason);
}
inline void baseline(){
    if constexpr(!reporting)return;
    stop("baseline_replaced");auto session=std::make_shared<Session>();session->id=std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);
    session->phases[0].begin=clock();std::atomic_store(&current,session);
}
inline void marker(){
    if constexpr(!reporting)return;
    auto session=std::atomic_load(&current);
    if(!session){session=std::make_shared<Session>();session->id=std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);std::atomic_store(&current,session);}
    std::lock_guard<std::mutex> guard(session->mutex);const auto now=clock();
    if(session->phase==0&&session->phases[0].begin)session->phases[0].end=now;
    session->phase=1;session->phases[1].begin=now;session->phases[1].last_engine=session->phases[1].last_present=0;
}
inline void frame(bool present){
    if constexpr(!reporting)return;
    auto session=std::atomic_load(&current);if(!session)return;const auto now=clock();
    std::lock_guard<std::mutex> guard(session->mutex);auto& p=session->phases[session->phase];
    auto& previous=present?p.last_present:p.last_engine;
    if(previous&&now>=previous)(present?p.present_intervals:p.engine_intervals).add(now-previous);previous=now;
}
struct PaintSample {
    std::shared_ptr<Session> session=reporting?std::atomic_load(&current):nullptr;
    uint64_t begin=0,original_ticks=0;unsigned phase=0;
    PaintSample(){if(session){session->in_flight.fetch_add(1);begin=clock();std::lock_guard<std::mutex> guard(session->mutex);phase=session->phase;}}
    ~PaintSample(){if(!session)return;const auto end=clock();
        {std::lock_guard<std::mutex> guard(session->mutex);auto& p=session->phases[phase];p.paint_cpu.add(original_ticks);p.wrapper_cpu.add(end>=begin+original_ticks?end-begin-original_ticks:0);}
        session->in_flight.fetch_sub(1);
    }
};
}
