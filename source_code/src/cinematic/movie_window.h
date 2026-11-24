#pragma once
#include "mask_geometry.h"
namespace cinema_mask {
// Reuse the movie screen's fixed pose for the existing flat HUD/subtitle layer.
// This changes compositor metadata only: the texture, flags and eye selection
// stay intact. Callers admit only verified UI layers and a current screen mapping.
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
inline const xr_read::Layer* presentation_ui_layer(const xr_read::EndInfo* info,unsigned width,unsigned height){
    if(!info||info->type!=xr_read::frame_end_info||info->next||info->environmentBlendMode!=1||
       !info->layers||info->layerCount!=2||!width||!height)return nullptr;
    const xr_read::Projection* source=nullptr;const xr_read::Layer* ui=nullptr;
    for(unsigned i=0;i<2;++i){
        const auto* layer=info->layers[i];if(!layer)return nullptr;
        if(layer->type==xr_read::projection){
            if(source)return nullptr;
            source=reinterpret_cast<const xr_read::Projection*>(layer);
        }else{
            if(ui||(layer->type!=xr_read::quad&&layer->type!=xr_read::cylinder))return nullptr;
            ui=layer;
        }
    }
    if(!source||!source->space||source->viewCount!=2||!source->views||!ui||ui->space!=source->space)return nullptr;
    for(unsigned eye=0;eye<2;++eye){
        const auto& sub=source->views[eye].subImage;
        if(!sub.swapchain||sub.imageRect.extent.width<=0||sub.imageRect.extent.height<=0)return nullptr;
    }
    const auto& sub=ui->type==xr_read::quad?reinterpret_cast<const xr_read::Quad*>(ui)->subImage:
        reinterpret_cast<const xr_read::Cylinder*>(ui)->subImage;
    const auto visibility=ui->type==xr_read::quad?reinterpret_cast<const xr_read::Quad*>(ui)->eyeVisibility:
        reinterpret_cast<const xr_read::Cylinder*>(ui)->eyeVisibility;
    if(visibility!=0||!sub.swapchain||sub.imageRect.extent.width<=0||sub.imageRect.extent.height<=0||
       static_cast<unsigned>(sub.imageRect.extent.width)!=width||static_cast<unsigned>(sub.imageRect.extent.height)!=height)return nullptr;
    return ui;
}
inline bool separate_ui_quad(const xr_read::Layer* layer,const xr_read::ProjectionView* views,xr_read::Quad& adjusted){
    if(!layer||layer->type!=xr_read::quad||!views)return false;
    const auto& original=*reinterpret_cast<const xr_read::Quad*>(layer);
    if(original.subImage.imageRect.extent.width<=0||original.subImage.imageRect.extent.height<=0)return false;
    const auto& left=views[0].pose.position;const auto& right=views[1].pose.position;
    const double x=(static_cast<double>(left.x)+right.x)/2,y=(static_cast<double>(left.y)+right.y)/2,
        z=(static_cast<double>(left.z)+right.z)/2;
    const double dx=original.pose.position.x-x,dy=original.pose.position.y-y,dz=original.pose.position.z-z;
    const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(distance)||distance<0.1)return false;
    const double scale=(distance-0.02)/distance;
    Window window{original.pose,original.space,static_cast<float>(original.width*scale),static_cast<float>(original.height*scale)};
    // Scale the whole quad about the head midpoint so all corner rays are preserved there.
    window.pose.position={static_cast<float>(x+dx*scale),static_cast<float>(y+dy*scale),static_cast<float>(z+dz*scale)};
    return anchor_ui_quad(layer,window,static_cast<unsigned>(original.subImage.imageRect.extent.width),
        static_cast<unsigned>(original.subImage.imageRect.extent.height),adjusted);
}
inline bool clip_movie_to_ui(const xr_read::Quad& ui,const xr_read::ProjectionView& eye,xr_read::Quad& movie){
    const auto& rect=movie.subImage.imageRect;
    if(!std::isfinite(movie.width+movie.height+ui.width+ui.height)||movie.width<=0||movie.height<=0||ui.width<=0||ui.height<=0||
       rect.extent.width<=0||rect.extent.height<=0||rect.offset.x<0||rect.offset.y<0||
       static_cast<int64_t>(rect.offset.x)+rect.extent.width>INT32_MAX||
       static_cast<int64_t>(rect.offset.y)+rect.extent.height>INT32_MAX)return false;
    const auto& m=movie.pose.orientation;const auto& u=ui.pose.orientation;
    const double mn=std::sqrt(static_cast<double>(m.x)*m.x+static_cast<double>(m.y)*m.y+static_cast<double>(m.z)*m.z+static_cast<double>(m.w)*m.w),
        un=std::sqrt(static_cast<double>(u.x)*u.x+static_cast<double>(u.y)*u.y+static_cast<double>(u.z)*u.z+static_cast<double>(u.w)*u.w);
    if(!std::isfinite(mn+un)||mn<=0||un<=0)return false;
    const xr_read::Quaternion rotation{static_cast<float>(m.x/mn),static_cast<float>(m.y/mn),
        static_cast<float>(m.z/mn),static_cast<float>(m.w/mn)};
    const xr_read::Quaternion inverse{-rotation.x,-rotation.y,-rotation.z,rotation.w};
    const xr_read::Quaternion ui_rotation{static_cast<float>(u.x/un),static_cast<float>(u.y/un),
        static_cast<float>(u.z/un),static_cast<float>(u.w/un)};
    const auto local=[&](xr_read::Vector3 p){
        p.x-=movie.pose.position.x;p.y-=movie.pose.position.y;p.z-=movie.pose.position.z;
        return rotate(inverse,p);
    };
    const auto origin=local(eye.pose.position);
    struct Point {double x,y;};
    std::array<Point,4> polygon{};
    constexpr int corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    for(unsigned i=0;i<polygon.size();++i){
        auto p=rotate(ui_rotation,{corners[i][0]*ui.width/2,corners[i][1]*ui.height/2,0});
        p.x+=ui.pose.position.x;p.y+=ui.pose.position.y;p.z+=ui.pose.position.z;p=local(p);
        const double dz=static_cast<double>(p.z)-origin.z;
        if(!std::isfinite(dz)||std::abs(dz)<1e-8)return false;
        const double t=-origin.z/dz;
        if(!std::isfinite(t)||t<=0)return false;
        polygon[i]={origin.x+t*(static_cast<double>(p.x)-origin.x),origin.y+t*(static_cast<double>(p.y)-origin.y)};
        if(!std::isfinite(polygon[i].x+polygon[i].y))return false;
    }
    double area=0;
    for(unsigned i=0;i<polygon.size();++i){const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];area+=a.x*b.y-a.y*b.x;}
    if(!std::isfinite(area)||std::abs(area)<1e-12)return false;
    const double sign=area>0?1:-1;
    double left=-movie.width/2,right=movie.width/2,bottom=-movie.height/2,top=movie.height/2;
    // Intersect each eye's UI edge rays with the scene plane, then keep an interior scene rectangle.
    for(unsigned i=0;i<polygon.size();++i){
        const auto& p=polygon[i];const auto& q=polygon[(i+1)%polygon.size()];
        const double a=sign*(p.y-q.y),b=sign*(q.x-p.x),c=sign*(q.y*p.x-q.x*p.y);
        if(std::abs(a)>=std::abs(b)){
            if(std::abs(a)<1e-12)return false;
            const double bound=-(std::min(b*bottom,b*top)+c)/a;
            if(a>0)left=std::max(left,bound);else right=std::min(right,bound);
        }else{
            const double bound=-(std::min(a*left,a*right)+c)/b;
            if(b>0)bottom=std::max(bottom,bound);else top=std::min(top,bound);
        }
        if(!std::isfinite(left+right+bottom+top)||left>=right||bottom>=top)return false;
    }
    // Keep the source rectangle constant: per-frame imageRect changes can make
    // compositors drop the layer for a frame. Fit the whole crop uniformly instead.
    const double scale=std::min((right-left)/movie.width,(top-bottom)/movie.height);
    if(!std::isfinite(scale)||scale<=0.5||scale>1)return false;
    auto clipped=movie;
    const auto shift=rotate(rotation,{static_cast<float>((left+right)/2),static_cast<float>((bottom+top)/2),0});
    clipped.pose.position.x+=shift.x;clipped.pose.position.y+=shift.y;clipped.pose.position.z+=shift.z;
    clipped.width=static_cast<float>(movie.width*scale);
    clipped.height=static_cast<float>(movie.height*scale);
    if(!std::isfinite(clipped.pose.position.x+clipped.pose.position.y+clipped.pose.position.z+clipped.width+clipped.height)||
       clipped.width<=0||clipped.height<=0)return false;
    movie=clipped;return true;
}
enum class HangarUiSeparation {unchanged,adjusted,invalid_geometry,adjusted_edge_unavailable};
// Both eyes are staged first so a partial result never reaches the compositor.
inline bool clip_movies_to_ui(const xr_read::Quad& ui,const xr_read::ProjectionView* views,std::array<xr_read::Quad,2>& movies){
    if(!views)return false;
    auto clipped=movies;
    if(!clip_movie_to_ui(ui,views[0],clipped[0])||!clip_movie_to_ui(ui,views[1],clipped[1]))return false;
    movies=clipped;return true;
}
inline HangarUiSeparation separate_hangar_ui(bool in_scope,const xr_read::EndInfo* input,xr_read::EndInfo& output,
    std::array<xr_read::Quad,2>& movies,unsigned width,unsigned height,xr_read::Quad& adjusted,
    std::array<const xr_read::Layer*,16>& storage){
    if(!in_scope||!output.layers||output.layerCount!=4)return HangarUiSeparation::unchanged;
    const auto* layer=presentation_ui_layer(input,width,height);
    if(!layer||layer->type!=xr_read::quad)return HangarUiSeparation::unchanged;
    const auto& ui=*reinterpret_cast<const xr_read::Quad*>(layer);
    if(input->layers[0]->type!=xr_read::projection||input->layers[1]!=layer||
       output.layers[0]!=reinterpret_cast<const xr_read::Layer*>(&movies[0])||
       output.layers[1]!=reinterpret_cast<const xr_read::Layer*>(&movies[1])||
       !output.layers[2]||output.layers[2]->type!=xr_read::projection||output.layers[2]->space!=ui.space||
       output.layers[2]==input->layers[0]||output.layers[3]!=layer)return HangarUiSeparation::unchanged;
    xr_read::Quad validated{};
    if(!anchor_ui_quad(layer,{ui.pose,ui.space,ui.width,ui.height},width,height,validated))return HangarUiSeparation::unchanged;
    const auto& q=ui.pose.orientation;
    const double ui_norm=static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+
        static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w;
    for(unsigned eye=0;eye<2;++eye){
        const auto& movie=movies[eye];
        if(movie.type!=xr_read::quad||movie.space!=ui.space||movie.eyeVisibility!=static_cast<int>(eye+1)||
           movie.subImage.imageRect.extent.width<=0||movie.subImage.imageRect.extent.height<=0||
           !anchor_ui_quad(reinterpret_cast<const xr_read::Layer*>(&movie),
               {movie.pose,movie.space,movie.width,movie.height},
               static_cast<unsigned>(movie.subImage.imageRect.extent.width),
               static_cast<unsigned>(movie.subImage.imageRect.extent.height),validated))return HangarUiSeparation::unchanged;
        const double x=static_cast<double>(movie.pose.position.x)-ui.pose.position.x,
            y=static_cast<double>(movie.pose.position.y)-ui.pose.position.y,
            z=static_cast<double>(movie.pose.position.z)-ui.pose.position.z;
        const auto& r=movie.pose.orientation;
        const double movie_norm=static_cast<double>(r.x)*r.x+static_cast<double>(r.y)*r.y+
            static_cast<double>(r.z)*r.z+static_cast<double>(r.w)*r.w;
        const double dot=static_cast<double>(q.x)*r.x+static_cast<double>(q.y)*r.y+
            static_cast<double>(q.z)*r.z+static_cast<double>(q.w)*r.w;
        if(x*x+y*y+z*z>1e-6||std::abs(dot)/std::sqrt(ui_norm*movie_norm)<1-1e-6)return HangarUiSeparation::unchanged;
    }
    const auto* source=reinterpret_cast<const xr_read::Projection*>(input->layers[0]);
    if(!separate_ui_quad(layer,source->views,adjusted))return HangarUiSeparation::invalid_geometry;
    const bool clipped=clip_movies_to_ui(adjusted,source->views,movies);
    for(unsigned i=0;i<output.layerCount;++i)storage[i]=output.layers[i]==layer?
        reinterpret_cast<const xr_read::Layer*>(&adjusted):output.layers[i];
    output.layers=storage.data();
    return clipped?HangarUiSeparation::adjusted:HangarUiSeparation::adjusted_edge_unavailable;
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
