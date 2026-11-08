#pragma once
#include <cmath>
#include <algorithm>
// Common-HUD reference geometry, in metres. Independent of texture resolution
// and headset lens. Map the calibrated reference ray onto the current UI surface.
namespace weapon_surface_math {
constexpr double pi=3.14159265358979323846;
struct Point {double x=0,y=0;};
struct Ray {double x=0,y=0,z=0;};
struct Surface {double height=1,distance=1,angle=113.625*pi/180,aspect=16.0/9,x=0,y=0;bool flat=false;};
inline bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1e8&&std::abs(p.y)<1e8;}
inline bool valid(const Surface& s){return std::isfinite(s.height+s.distance+s.angle+s.aspect+s.x+s.y)&&s.height>0&&s.distance>0&&s.aspect>0&&s.angle>0&&s.angle<2*pi;}
inline bool same(const Surface& a,const Surface& b){return a.flat==b.flat&&std::abs(a.height-b.height)<1e-6&&std::abs(a.distance-b.distance)<1e-6&&std::abs(a.angle-b.angle)<1e-6&&std::abs(a.aspect-b.aspect)<1e-6&&std::abs(a.x-b.x)<1e-6&&std::abs(a.y-b.y)<1e-6;}
inline Ray ray(const Surface& s,Point uv){
 const double width=s.height*s.aspect;
 if(s.flat)return{s.x+(uv.x-.5)*width,s.y+(.5-uv.y)*s.height,-s.distance};
 const double r=width/s.angle,t=(uv.x-.5)*s.angle;
 return{s.x+r*std::sin(t),s.y+(.5-uv.y)*s.height,r-s.distance-r*std::cos(t)};
}
inline bool intersect(const Surface& s,Ray d,Point& uv){
 if(!valid(s)||!std::isfinite(d.x+d.y+d.z)||d.z>=-1e-12)return false;
 const double width=s.height*s.aspect;
 if(s.flat){const double t=-s.distance/d.z;uv={.5+(d.x*t-s.x)/width,.5-(d.y*t-s.y)/s.height};return finite(uv);}
 const double r=width/s.angle,ox=-s.x,oz=s.distance-r;
 const double a=d.x*d.x+d.z*d.z,b=2*(ox*d.x+oz*d.z),c=ox*ox+oz*oz-r*r,disc=b*b-4*a*c;
 if(a<1e-20||disc<0||!std::isfinite(disc))return false;
 const double roots[]{(-b-std::sqrt(disc))/(2*a),(-b+std::sqrt(disc))/(2*a)};
 // Choose the front panel, rather than the back of a large curved surface.
 for(double t:roots){if(t<=1e-12)continue;const double x=ox+t*d.x,z=oz+t*d.z;if(z>=0)continue;
  uv={.5+std::atan2(x,-z)/s.angle,.5-(t*d.y-s.y)/s.height};if(finite(uv))return true;
 }
 return false;
}
inline bool map(const Surface& from,const Surface& to,Point in,Point& out){
 if(!valid(from)||!valid(to)||!finite(in))return false;
 if(same(from,to)){out=in;return true;}
 return intersect(to,ray(from,in),out);
}
// Pixel -> logical viewport affine basis measured through original UE helpers.
struct Basis {Point origin{},x{},y{};double width=0,height=0;};
inline bool logicalToUV(const Basis& b,Point p,Point& uv){
 const double det=b.x.x*b.y.y-b.x.y*b.y.x;
 if(!finite(p)||b.width<=0||b.height<=0||!std::isfinite(det)||std::abs(det)<1e-12)return false;
 const double x=p.x-b.origin.x,y=p.y-b.origin.y;
 uv={(x*b.y.y-y*b.y.x)/(det*b.width),(y*b.x.x-x*b.x.y)/(det*b.height)};return finite(uv);
}
inline Point uvToLogical(const Basis& b,Point p){return{b.origin.x+b.x.x*p.x*b.width+b.y.x*p.y*b.height,b.origin.y+b.x.y*p.x*b.width+b.y.y*p.y*b.height};}
// Draw-local constants. Keep map/ray/intersect above as the simple reference
// formulation; runtime prepares the invariant terms once for all vertices.
struct PreparedSurface {
 Surface surface{};double width=0,radius=0,inverseWidth=0,inverseHeight=0;
 double originX=0,originZ=0,quadraticConstant=0;
};
inline PreparedSurface prepareSurface(const Surface& s){
 PreparedSurface p;p.surface=s;p.width=s.height*s.aspect;p.radius=p.width/s.angle;
 p.inverseWidth=1/p.width;p.inverseHeight=1/s.height;p.originX=-s.x;p.originZ=s.distance-p.radius;
 p.quadraticConstant=p.originX*p.originX+p.originZ*p.originZ-p.radius*p.radius;return p;
}
struct Prepared {
 bool active=false;PreparedSurface from{},to{};Basis basis{};
 double ux=0,uy=0,vx=0,vy=0,xu=0,xv=0,yu=0,yv=0,strokeScale=1;
};
inline bool prepare(const Surface& from,const Surface& to,const Basis& b,Prepared& p){
 p={};const double det=b.x.x*b.y.y-b.x.y*b.y.x;
 if(!valid(from)||!valid(to)||!finite(b.origin)||!finite(b.x)||!finite(b.y)||
    !std::isfinite(b.width+b.height+det)||b.width<=0||b.height<=0||std::abs(det)<1e-12)return false;
 p.from=prepareSurface(from);p.to=prepareSurface(to);p.basis=b;
 const double iu=1/(det*b.width),iv=1/(det*b.height);
 p.ux=b.y.y*iu;p.uy=-b.y.x*iu;p.vx=-b.x.y*iv;p.vy=b.x.x*iv;
 p.xu=b.x.x*b.width;p.xv=b.y.x*b.height;p.yu=b.x.y*b.width;p.yv=b.y.y*b.height;
 p.strokeScale=to.distance*from.height/(from.distance*to.height);
 if(!std::isfinite(p.ux+p.uy+p.vx+p.vy+p.xu+p.xv+p.yu+p.yv+p.strokeScale)||p.strokeScale<=0)return false;
 p.active=true;return true;
}
inline bool preparedMap(const Prepared& p,Point in,Point& out){
 if(!p.active||!finite(in))return false;
 const double x=in.x-p.basis.origin.x,y=in.y-p.basis.origin.y;
 const Point uv{x*p.ux+y*p.uy,x*p.vx+y*p.vy};if(!finite(uv))return false;
 const auto& f=p.from;const auto& t=p.to;const auto& fs=f.surface;const auto& ts=t.surface;
 Ray d{};
 if(fs.flat)d={fs.x+(uv.x-.5)*f.width,fs.y+(.5-uv.y)*fs.height,-fs.distance};
 else{const double angle=(uv.x-.5)*fs.angle;d={fs.x+f.radius*std::sin(angle),fs.y+(.5-uv.y)*fs.height,f.radius-fs.distance-f.radius*std::cos(angle)};}
 if(!std::isfinite(d.x+d.y+d.z)||d.z>=-1e-12)return false;
 Point mapped{};
 if(ts.flat){const double k=-ts.distance/d.z;mapped={.5+(d.x*k-ts.x)*t.inverseWidth,.5-(d.y*k-ts.y)*t.inverseHeight};}
 else{
  const double a=d.x*d.x+d.z*d.z,b=2*(t.originX*d.x+t.originZ*d.z),disc=b*b-4*a*t.quadraticConstant;
  if(a<1e-20||disc<0||!std::isfinite(disc))return false;
  const double root=std::sqrt(disc),denom=2*a;const double roots[]{(-b-root)/denom,(-b+root)/denom};bool found=false;
  for(double k:roots){if(k<=1e-12)continue;const double px=t.originX+k*d.x,pz=t.originZ+k*d.z;if(pz>=0)continue;
   mapped={.5+std::atan2(px,-pz)/ts.angle,.5-(k*d.y-ts.y)*t.inverseHeight};if(finite(mapped)){found=true;break;}
  }
  if(!found)return false;
 }
 if(!finite(mapped))return false;
 out={p.basis.origin.x+p.xu*mapped.x+p.xv*mapped.y,p.basis.origin.y+p.yu*mapped.x+p.yv*mapped.y};return finite(out);
}
}
