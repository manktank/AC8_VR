#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include "../vendor/uevr/plugin-api.h"
#include "lua-event-queue.h"

namespace ac8_runtime_log {
inline std::mutex mutex;
inline HANDLE file=INVALID_HANDLE_VALUE;
inline std::filesystem::path current,previous;
inline constexpr DWORD limit=8*1024*1024;
inline DWORD bytes=0;
inline bool trace_callbacks=false;
inline UEVR_PluginFunctions host{};
inline UEVR_PluginFunctions functions{};
inline UEVR_PluginInitializeParam params{};

inline void failure(const char* operation,DWORD code){
    if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    if(host.log_error)host.log_error("[AC8 logging] %s failed; error=%lu; file logging unavailable, retain UEVR logs",operation,code);
    OutputDebugStringA("[AC8 logging] File logging failed; retain UEVR logs\n");
}
inline bool open(){
    file=CreateFileW(current.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){failure("open",GetLastError());return false;}
    bytes=0;return true;
}
inline bool rotate(){
    if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    if(!MoveFileExW(current.c_str(),previous.c_str(),MOVEFILE_REPLACE_EXISTING)){
        const auto code=GetLastError();
        if(code!=ERROR_FILE_NOT_FOUND){failure("rotate",code);return false;}
    }
    return open();
}
inline void write(const char* level,const char* text){
    const auto saved_error=GetLastError();
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(file!=INVALID_HANDLE_VALUE){
            SYSTEMTIME time{};GetSystemTime(&time);
            char line[8192]{};
            const int count=_snprintf_s(line,sizeof(line),_TRUNCATE,
                "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ uptime=%llu pid=%lu tid=%lu [%s] %.7000s\r\n",
                time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
                GetTickCount64(),GetCurrentProcessId(),GetCurrentThreadId(),level,text?text:"");
            const auto length=static_cast<DWORD>(count<0?std::strlen(line):static_cast<size_t>(count));
            if((bytes+length<=limit||rotate())&&file!=INVALID_HANDLE_VALUE){
                DWORD written=0;
                if(!WriteFile(file,line,length,&written,nullptr)||written!=length)failure("write",GetLastError());
                else bytes+=written;
            }
        }
    }
    SetLastError(saved_error);
}
inline void formatted(const char* level,const char* format,va_list args){
    char text[4096]{};vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);write(level,text);
    const auto sink=!std::strcmp(level,"error")?host.log_error:!std::strcmp(level,"warn")?host.log_warn:host.log_info;
    if(sink)sink("%s",text);
}
inline void error(const char* format,...){va_list args;va_start(args,format);formatted("error",format,args);va_end(args);}
inline void warn(const char* format,...){va_list args;va_start(args,format);formatted("warn",format,args);va_end(args);}
inline void info(const char* format,...){va_list args;va_start(args,format);formatted("info",format,args);va_end(args);}
inline void event(const char* name,const char* data){
    if(!name)return;
    if(!std::strcmp(name,"AC8_VR_LogMode")){
        // Queried from Lua: reply through the deferred queue (lua-event-queue.h).
        ac8_lua_events::post("AC8_VR_LogMode",trace_callbacks?"trace":"normal");
    }else if(!std::strcmp(name,"AC8_VR_LuaLog"))write("lua",data);
}
inline const UEVR_PluginInitializeParam* initialize(const UEVR_PluginInitializeParam* p,const wchar_t* root){
    host=*p->functions;
    current=std::filesystem::path(root)/L"AC8_VR-Runtime.log";
    previous=std::filesystem::path(root)/L"AC8_VR-Runtime.previous.log";
    const auto flag=std::filesystem::path(root)/L"data"/L"AC8_VR-TraceCallbacks.flag";
    const auto attributes=GetFileAttributesW(flag.c_str());
    trace_callbacks=attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY);
    rotate();
    functions=host;functions.log_error=error;functions.log_warn=warn;functions.log_info=info;
    params=*p;params.functions=&functions;
    info("[AC8 logging] Session started; build=%s %s; callbacks=%s; max_file_bytes=%lu",
         __DATE__,__TIME__,trace_callbacks?"trace":"normal",limit);
    if(host.get_commit_hash)info("[AC8 logging] UEVR commit=%s",host.get_commit_hash());
    if(!p->callbacks->on_custom_event(event))error("[AC8 logging] Lua log callback registration failed");
    return &params;
}
struct Scope {
    const char* name;
    explicit Scope(const char* value):name(value){if(trace_callbacks)write("trace begin",name);}
    ~Scope(){if(trace_callbacks)write("trace end",name);}
};
}
