#pragma once
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace hangar_near_clip {
inline std::atomic<double> camera_near{0};
inline void control(const char* name,const char* data){
    if(!name||std::strcmp(name,"AC8_CinematicNearClip")!=0)return;
    double value=0;
    if(data){char* end=nullptr;const double parsed=std::strtod(data,&end);
        if(end!=data&&end&&*end=='\0'&&std::isfinite(parsed)&&parsed>0&&parsed<=1.e7)value=parsed;}
    camera_near.store(value);
}
template<typename T> bool apply(T (&m)[4][4],bool hangar){
    const double near_plane=camera_near.load();
    if(!hangar||near_plane<=0)return false;
    // Verified UE infinite reversed-Z perspective layout. Preserve eye offsets,
    // focal terms and all depth conventions; only replace the near coefficient.
    for(const auto& row:m)for(const auto value:row)if(!std::isfinite(value))return false;
    if(std::abs(m[2][2])>1.e-9||std::abs(m[2][3]-1)>1.e-9||std::abs(m[3][3])>1.e-9||m[3][2]<=0||m[0][0]<=0||m[1][1]<=0)return false;
    m[3][2]=static_cast<T>(near_plane);return true;
}
}
