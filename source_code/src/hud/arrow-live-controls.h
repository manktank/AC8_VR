#pragma once
// TEMPORARY calibration: additive offsets only. No anchoring, normalization,
// shape scaling, scene work, UObject setters or discovery. Remove after tuning.
namespace live_controls {
using Draw=void(*)(UEVR_UObjectHandle,UEVR_UObjectHandle,unsigned char,const Point*,const void*,float,const void*,float,const Point*);
inline Draw original=nullptr;
// Text uses its own native draw call. Limit the correction to the exact target
// body/canvas, after that body emitted a shifted target arrow. No pooled widgets
// are retained or written; no scan, new tick, allocation or GPU work.
using TextDraw=void(*)(UEVR_UObjectHandle,const void*,const void*,const Point*,uint64_t,uint64_t,uint64_t,const Point*,unsigned char,unsigned char,int);
inline TextDraw originalText=nullptr;
inline uintptr_t gameBase=0;

struct TargetContext { UEVR_UObjectHandle widget,canvas;uint64_t bits;bool moved=false;Point shift{}; };
inline thread_local TargetContext* currentTarget=nullptr;
struct TargetScope {
 TargetContext value;TargetContext* prior;
 TargetScope(UEVR_UObjectHandle w,UEVR_UObjectHandle c):value{w,c,packedBits()},prior(currentTarget){currentTarget=&value;}
 static uint64_t packedBits();
 ~TargetScope(){currentTarget=prior;}
 TargetScope(const TargetScope&)=delete;TargetScope& operator=(const TargetScope&)=delete;
};
inline std::atomic<bool> installed{false};
inline std::atomic<uint64_t> packed{0};
inline uint64_t TargetScope::packedBits(){return packed.load(std::memory_order_relaxed);}

inline std::atomic<const char*> state{"waiting"};
constexpr uintptr_t rva=0x723f9e0;
constexpr unsigned char head[]={0x4c,0x8b,0xdc,0x55,0x53,0x49,0x8d,0xab,0x18,0xff,0xff,0xff,0x48,0x81,0xec,0xd8,0x01,0x00,0x00};
inline Point delta(uint64_t bits,unsigned char kind){
 const unsigned shift=kind==0?0:32;
 const uint16_t x=static_cast<uint16_t>(bits>>shift),y=static_cast<uint16_t>(bits>>(shift+16));
 int16_t sx=0,sy=0;std::memcpy(&sx,&x,2);std::memcpy(&sy,&y,2);
 return{sx/10.0,sy/10.0};
}
inline void notify(){
 if(host&&host->functions&&host->functions->dispatch_lua_event)
  host->functions->dispatch_lua_event("AC8_ArrowLiveStatus",installed?"ready":!std::strcmp(state.load(),"waiting")?"waiting":"unavailable");
}

inline void event(const char* name,const char* data){
 if(name&&!std::strcmp(name,"AC8_ArrowLiveSnapshot")){static_cast<void>(0);return;}
 if(!name||!data||std::strcmp(name,"AC8_ArrowLiveControls"))return;
 if(!std::strcmp(data,"query")){notify();return;}
 if(!std::strcmp(data,"off")){packed=0;return;}
 double values[4]{};char trailing=0;
 if(sscanf_s(data,"%lf|%lf|%lf|%lf%c",&values[0],&values[1],&values[2],&values[3],&trailing,1u)!=4)return;
 uint64_t bits=0;
 for(unsigned i=0;i<4;++i){
  if(!std::isfinite(values[i])||std::abs(values[i])>3000)return;
  const auto v=static_cast<int16_t>(std::llround(values[i]*10));
  bits|=static_cast<uint64_t>(static_cast<uint16_t>(v))<<(16*i);
 }
 if(packed.exchange(bits)!=bits)static_cast<void>(0);
 notify();
}
inline void draw(UEVR_UObjectHandle widget,UEVR_UObjectHandle canvas,unsigned char kind,const Point* size,
                 const void* projection,float thickness,const void* color,float shapeScale,const Point* offset){
 const auto fn=original;if(!fn)return;
 const auto ctx=currentTarget;
 const auto bits=(kind==1&&ctx&&ctx->widget==widget&&ctx->canvas==canvas)?ctx->bits:packed.load(std::memory_order_relaxed);
 // Zero offsets preserve the original pointers/arguments without any memory
 // probing, coordinate lookup, counters, array walk or property writes.
 if(!bits||kind>1){fn(widget,canvas,kind,size,projection,thickness,color,shapeScale,offset);return;}
 const auto shift=delta(bits,kind);Point prior{},moved{};const Point* selected=offset;
 if((shift.x!=0||shift.y!=0)&&read(offset,prior)&&shiftedPoint(prior,shift,moved)){
  selected=&moved;static_cast<void>(0);
  if(kind==1&&ctx&&ctx->widget==widget&&ctx->canvas==canvas){ctx->moved=true;ctx->shift=shift;}
 }
 fn(widget,canvas,kind,size,projection,thickness,color,shapeScale,selected);
}
// These four direct call sites in the verified target body emit name/type,
// range and target readouts. On-screen labels enter a different route, without
// a shifted arrow in this scope, and keep their original arguments.
inline bool textSite(uintptr_t site){
 if(!gameBase)return false;
 const auto at=site-gameBase;
 return at==bodyRva+0x86c||at==bodyRva+0x8c0||at==bodyRva+0x91c||at==bodyRva+0xbe0;
}
inline void textAt(uintptr_t site,UEVR_UObjectHandle canvas,const void* text,const void* color,const Point* position,
 uint64_t a5,uint64_t a6,uint64_t a7,const Point* alignment,unsigned char convert,unsigned char low,int font){
 const auto fn=originalText;if(!fn)return;
 const auto ctx=currentTarget;const Point* selected=position;Point prior{},moved{};
 // The target body has already converted the projected anchor to canvas-local
 // coordinates, then adds authored text offsets. The arrow offset is in those
 // same viewport-local units. Translate only the position; retain font/color,
 // alignment, priority and all opaque native stack inputs byte-for-byte.
 if(ctx&&ctx->moved&&ctx->canvas==canvas&&!convert&&textSite(site)&&
    read(position,prior)&&shiftedPoint(prior,ctx->shift,moved)){
  selected=&moved;static_cast<void>(0);
 }
 fn(canvas,text,color,selected,a5,a6,a7,alignment,convert,low,font);
}
__declspec(noinline) inline void drawText(UEVR_UObjectHandle canvas,const void* text,const void* color,const Point* position,
 uint64_t a5,uint64_t a6,uint64_t a7,const Point* alignment,unsigned char convert,unsigned char low,int font){
 textAt(reinterpret_cast<uintptr_t>(_ReturnAddress()),canvas,text,color,position,a5,a6,a7,alignment,convert,low,font);
}
inline void initialize(unsigned char* base){
 if(!codeMatches(base+rva,head)||!codeMatches(base+textRva,textHead)){state="code_guard_failed";return;}
 gameBase=reinterpret_cast<uintptr_t>(base);
 const int hook=host->functions->register_inline_hook(base+rva,reinterpret_cast<void*>(&draw),reinterpret_cast<void**>(&original));
 const int textHook=host->functions->register_inline_hook(base+textRva,reinterpret_cast<void*>(&drawText),reinterpret_cast<void**>(&originalText));
 installed=hook>=0&&original&&textHook>=0&&originalText;
 if(!installed){
  if(hook>=0)host->functions->unregister_inline_hook(hook);
  if(textHook>=0)host->functions->unregister_inline_hook(textHook);
  original=nullptr;originalText=nullptr;
 }
 state=installed?"additive_offsets_and_target_text":"hook_unavailable";
}
} // namespace live_controls
