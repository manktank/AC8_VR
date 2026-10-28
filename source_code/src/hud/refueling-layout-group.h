// Included inside escort_draw: one shared native refueling layout state.
// This is not an Actor scale (screen-sized material billboards ignore that).
// Evidence: guarded game initializer at 0x7240970 and its adjoining update code; exact instruction guards below.
// No new hook/callback/scan. Only the existing, exact refueling actor is touched.
namespace refueling_group {
struct Values {float text,texture;double width,height;float span;};
struct Row {UEVR_UObjectHandle actor=nullptr;Values original{},last{};float factor=1,cache=0,forced=0;bool pending=false;};
static std::map<std::wstring,Row> rows;
static std::set<UEVR_UStructHandle> schemas;
static std::wstring schemaLevel;
static bool codeChecked=false,codeValid=false;

constexpr size_t offsets[]={0x310,0x3a8,0x3c0,0x3c8,0x3e0};
static double number(const Values& v,size_t i){switch(i){case 0:return v.text;case 1:return v.texture;case 2:return v.width;case 3:return v.height;default:return v.span;}}
static void assign(Values& v,size_t i,double n){switch(i){case 0:v.text=static_cast<float>(n);break;case 1:v.texture=static_cast<float>(n);break;case 2:v.width=n;break;case 3:v.height=n;break;default:v.span=static_cast<float>(n);}}
static bool valid(const Values& v){
    for(size_t i=0;i<5;++i)if(!std::isfinite(number(v,i))||std::abs(number(v,i))>1e7)return false;
    return v.text>0&&v.texture>0&&v.width>0&&v.height>0;
}
static bool sameValue(double a,double b){return std::abs(a-b)<=1e-6*std::max(1.0,std::abs(b));}
static Values scaled(Values value,float factor){for(size_t i=0;i<5;++i)assign(value,i,number(value,i)*factor);return value;}
static bool readValue(UEVR_UObjectHandle actor,size_t i,double& value){
    auto p=reinterpret_cast<const unsigned char*>(actor)+offsets[i];
    if(i==2||i==3)return copy(p,value);
    float f=0;if(!copy(p,f))return false;value=f;return true;
}
static bool readValues(UEVR_UObjectHandle actor,Values& value){
    for(size_t i=0;i<5;++i){double n=0;if(!readValue(actor,i,n))return false;assign(value,i,n);}return valid(value);
}
static bool put(UEVR_UObjectHandle actor,size_t offset,const void* input,size_t length){
    SIZE_T n=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<unsigned char*>(actor)+offset,input,length,&n)&&n==length;
}
static bool writeValue(UEVR_UObjectHandle actor,size_t i,double value){
    if(i==2||i==3)return put(actor,offsets[i],&value,sizeof(value));
    const float f=static_cast<float>(value);return put(actor,offsets[i],&f,sizeof(f));
}
static bool patch(UEVR_UObjectHandle actor,const Values& before,const Values& wanted,float cache,float forced){
    for(size_t i=0;i<5;++i){
        double after=0;
        if(!writeValue(actor,i,number(wanted,i))||!readValue(actor,i,after)||!sameValue(after,number(wanted,i))){
            for(size_t j=0;j<=i;++j)writeValue(actor,j,number(before,j));return false;
        }
    }
    float after=0;
    if(!put(actor,0x3e4,&forced,4)||!copy(reinterpret_cast<unsigned char*>(actor)+0x3e4,after)||after!=forced){
        for(size_t i=0;i<5;++i)writeValue(actor,i,number(before,i));put(actor,0x3e4,&cache,4);return false;
    }
    return true;
}
static bool releaseRefresh(Row& row){
    if(!row.pending)return true;float current=0;
    if(!copy(reinterpret_cast<unsigned char*>(row.actor)+0x3e4,current))return false;
    // One game update gets the changed dimensions, then its original cache is
    // restored. Never force expensive layout refreshes for the whole mission.
    if(current==row.forced&&!put(row.actor,0x3e4,&row.cache,4))return false;
    row.pending=false;return true;
}
static bool signature(uintptr_t rva,const std::vector<unsigned char>& expected){
    auto base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    std::vector<unsigned char> actual(expected.size());SIZE_T n=0;
    return base&&ReadProcessMemory(GetCurrentProcess(),base+rva,actual.data(),actual.size(),&n)&&n==actual.size()&&actual==expected;
}
static bool codeGuard(){
    if(codeChecked)return codeValid;codeChecked=true;
    codeValid=signature(0x7240970,{0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57})&&
        signature(0x7240970+0x9c8,{0xf3,0x0f,0x11,0x83,0x10,0x03,0,0})&&
        signature(0x7240970+0x14ae,{0xf2,0x0f,0x10,0x8f,0xc8,0x03,0,0})&&
        signature(0x7240970+0x15b7,{0xf3,0x0f,0x59,0x87,0x0c,0x03,0,0});
    return codeValid;
}
static bool schema(UEVR_UObjectHandle actor){
    auto cls=reinterpret_cast<UEVR_UStructHandle>(host->sdk->uobject->get_class(actor));
    if(schemas.count(cls))return true;
    struct Spec{const wchar_t* name;int offset;};
    for(const auto& spec:{Spec{L"TextScreenHeight",0x300},{L"VRTextScreenHeight",0x304},{L"WidthAlignmentYOffsetScale",0x308},{L"WidthAlignmentXOffset",0x30c},{L"AerialRefuelTextureScale",0x368},{L"AerialRefuelVRTextureScale",0x36c}}){
        auto p=property(cls,spec.name);
        if(!p||propertyType(p)!=L"FloatProperty"||host->sdk->fproperty->get_offset(p)!=spec.offset)return false;
    }
    if(schemas.size()>=16)schemas.clear();schemas.insert(cls);return true;
}

static bool apply(UEVR_UObjectHandle actor,const std::wstring& name,float factor){
    if(!std::isfinite(factor)||factor<.1f||factor>2)return false;
    auto it=rows.find(name);
    if(it!=rows.end()&&it->second.actor!=actor){rows.erase(it);it=rows.end();}
    if(it!=rows.end()&&!releaseRefresh(it->second))return false;
    // Two cached driver reads detect native reinitialization. No widget/array
    // discovery, text setters, or layout writes in the stable branch.
    double text=0,texture=0;if(!readValue(actor,0,text)||!readValue(actor,1,texture))return false;
    if(it!=rows.end()&&it->second.factor==factor&&sameValue(text,it->second.last.text)&&sameValue(texture,it->second.last.texture))return true;
    Values now{};if(!readValues(actor,now))return false;
    Values base=now;
    if(it!=rows.end()&&sameValue(text,it->second.last.text)&&sameValue(texture,it->second.last.texture))for(size_t i=0;i<5;++i)if(sameValue(number(now,i),number(it->second.last,i)))assign(base,i,number(it->second.original,i));
    Values wanted=scaled(base,factor);float cache=0;
    if(!valid(wanted)||!copy(reinterpret_cast<unsigned char*>(actor)+0x3e4,cache)||!std::isfinite(cache)||std::abs(cache)>100000)return false;
    const float forced=cache+1;
    if(!patch(actor,now,wanted,cache,forced))return false;
    Row row{actor,base,wanted,factor,cache,forced,true};rows[name]=row;static_cast<void>(0);static_cast<void>(0);return true;
}
static void update(UEVR_UObjectHandle actor,const std::wstring& name,float factor){
    if(schemaLevel!=layout_level){schemas.clear();schemaLevel=layout_level;}
    if(!actor||!codeGuard()||!schema(actor))return;
    if(!apply(actor,name,factor)){}
}
static void finish(const std::set<std::wstring>& seen){
    for(auto it=rows.begin();it!=rows.end();){
        if(seen.count(it->first)){++it;continue;}
        auto actorPath=it->first.substr(0,it->first.rfind(L'.'));
        auto actor=host->sdk->uobject_array->find_uobject((L"BP_LiveAerialRefuel3DUIActor_C "+actorPath).c_str());
        if(actor==it->second.actor&&schema(actor)){
            releaseRefresh(it->second);
            if(!apply(actor,it->first,1)){static_cast<void>(0);++it;continue;}
            // An actor no longer being drawn needs no forced refresh now.
            releaseRefresh(it->second);
        }
        it=rows.erase(it);
    }
}
} // namespace refueling_group
