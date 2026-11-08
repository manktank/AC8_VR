#pragma once
#include <atomic>
// An optional native hook can publish its verified original function. Consumers
// remain independent when that hook/module is absent, and never accept an
// arbitrary patched entry as a substitute for the executable code guard.
namespace ac8_viewport_helper {
inline std::atomic<void*> entry{nullptr},original{nullptr};
inline std::atomic<void*> screenEntry{nullptr},screenOriginal{nullptr};
inline void publishScreen(void* address,void* trampoline){
 if(!address||!trampoline)return;
 screenOriginal.store(trampoline,std::memory_order_relaxed);
 screenEntry.store(address,std::memory_order_release);
}
inline void* verifiedScreenOriginal(void* address){
 return screenEntry.load(std::memory_order_acquire)==address?screenOriginal.load(std::memory_order_relaxed):nullptr;
}
inline void publish(void* address,void* trampoline){
 if(!address||!trampoline)return;
 original.store(trampoline,std::memory_order_relaxed);
 entry.store(address,std::memory_order_release);
}
inline void* verifiedOriginal(void* address){
 return entry.load(std::memory_order_acquire)==address?original.load(std::memory_order_relaxed):nullptr;
}
}
