#pragma once
#include "mask_geometry.h"
namespace cinema_mask {
// Reuse the movie screen's fixed pose for the existing flat HUD/subtitle layer.
// This changes compositor metadata only: the texture, flags and eye selection
// stay intact. Hangar and unknown/non-UI layers are excluded by the caller.
inline bool anchor_ui_quad(const xr_read::Layer* layer,const Window& screen,unsigned width,unsigned height,xr_read::Quad& output){
    if(!layer||layer->type!=xr_read::quad||!screen.space||layer->space!=screen.space||!width||!height)return false;
    const auto& ui=*reinterpret_cast<const xr_read::Quad*>(layer);
    if(ui.subImage.imageRect.extent.width!=static_cast<int>(width)||ui.subImage.imageRect.extent.height!=static_cast<int>(height)||
       !ui.subImage.swapchain||!std::isfinite(screen.width+screen.height)||screen.width<=0||screen.height<=0)return false;
    const auto& p=screen.pose;
    if(!std::isfinite(p.orientation.x+p.orientation.y+p.orientation.z+p.orientation.w+p.position.x+p.position.y+p.position.z))return false;
    const float norm=p.orientation.x*p.orientation.x+p.orientation.y*p.orientation.y+p.orientation.z*p.orientation.z+p.orientation.w*p.orientation.w;
    if(std::abs(norm-1)>0.01f)return false;
    output=ui;output.pose=screen.pose;output.width=screen.width;output.height=screen.height;
    return true;
}
struct MovieCrop {float left,top,right,bottom;};
// Locked cameras share authored axes. Tracked XR poses must not steer their crop.
inline bool movie_crops(const Window& window,float distance,const xr_read::ProjectionView* images,std::array<MovieCrop,2>& crops){
    if(!images||!std::isfinite(distance)||distance<=0||window.width<=0||window.height<=0)return false;
    const float aspect=window.width/window.height;
    float horizontal=window.width/(2*distance);
    for(unsigned eye=0;eye<2;++eye){const auto& f=images[eye].fov;
        const float l=std::tan(f.angleLeft),r=std::tan(f.angleRight),u=std::tan(f.angleUp),d=std::tan(f.angleDown);
        if(!std::isfinite(l+r+u+d)||l>=0||r<=0||u<=0||d>=0)return false;
        horizontal=std::min(horizontal,std::min(std::min(-l,r),std::min(u,-d)*aspect));}
    if(horizontal<=0)return false;
    const float vertical=horizontal/aspect;
    for(unsigned eye=0;eye<2;++eye){const auto& f=images[eye].fov;
        const float l=std::tan(f.angleLeft),r=std::tan(f.angleRight),u=std::tan(f.angleUp),d=std::tan(f.angleDown);
        crops[eye]={(-horizontal-l)/(r-l),(u-vertical)/(u-d),(horizontal-l)/(r-l),(u+vertical)/(u-d)};}
    return true;
}
inline bool movie_crop(const Window& window,const xr_read::ProjectionView& reference,MovieCrop& crop){
    const auto v=window_vertices(window,reference);
    crop={1,1,0,0};
    for(const auto& p:v){if(p.w<=0||!std::isfinite(p.x+p.y+p.w))return false;
        const float x=(p.x/p.w+1)/2,y=(1-p.y/p.w)/2;
        crop.left=std::min(crop.left,x);crop.right=std::max(crop.right,x);
        crop.top=std::min(crop.top,y);crop.bottom=std::max(crop.bottom,y);}
    crop.left=std::clamp(crop.left,0.f,1.f);crop.right=std::clamp(crop.right,0.f,1.f);
    crop.top=std::clamp(crop.top,0.f,1.f);crop.bottom=std::clamp(crop.bottom,0.f,1.f);
    return crop.right-crop.left>.01f&&crop.bottom-crop.top>.01f;
}
inline bool movie_quad(const Window& window,const xr_read::ProjectionView& image,const MovieCrop& crop,unsigned eye,xr_read::Quad& quad){
    const auto& r=image.subImage.imageRect;
    if(eye>1||!image.subImage.swapchain||r.extent.width<=0||r.extent.height<=0)return false;
    const int left=static_cast<int>(std::ceil(crop.left*r.extent.width)),top=static_cast<int>(std::ceil(crop.top*r.extent.height));
    const int right=static_cast<int>(std::floor(crop.right*r.extent.width)),bottom=static_cast<int>(std::floor(crop.bottom*r.extent.height));
    if(left<0||top<0||right>r.extent.width||bottom>r.extent.height||left>=right||top>=bottom)return false;
    quad={};quad.type=xr_read::quad;quad.space=window.space;quad.eyeVisibility=eye==0?1:2;
    quad.subImage=image.subImage;quad.subImage.imageRect={{r.offset.x+left,r.offset.y+top},{right-left,bottom-top}};
    quad.pose=window.pose;quad.width=window.width;quad.height=window.height;
    return true;
}
}
