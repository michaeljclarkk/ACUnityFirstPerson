#pragma once
#include "head_anchor.h"
#include "camera_comfort.h"
namespace fp {
inline Vec3 Cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Unit(Vec3 v,Vec3& out){const float n=Length(v);if(!Finite(v)||n<0.001f)return false;out=Scale(v,1/n);return true;}
inline Quat Multiply(Quat a,Quat b){return {
 a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
 a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
 a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
 a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline Quat InverseUnit(Quat q){return {-q.x,-q.y,-q.z,q.w};}
inline Vec3 Rotate(Quat q,Vec3 v){const auto p=Multiply(Multiply(q,{v.x,v.y,v.z,0}),InverseUnit(q));return {p.x,p.y,p.z};}
inline Quat BlendRotation(Quat a,Quat b,float amount){
 float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
 if(dot<0){b={-b.x,-b.y,-b.z,-b.w};dot=-dot;}
 amount=std::clamp(amount,0.0f,1.0f);
 float wa=1-amount,wb=amount;
 if(dot<0.9995f){const float angle=std::acos(std::clamp(dot,0.0f,1.0f));wa=std::sin((1-amount)*angle)/std::sin(angle);wb=std::sin(amount*angle)/std::sin(angle);}
 Quat q{a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb,a.w*wa+b.w*wb};
 const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
 return {q.x/n,q.y/n,q.z/n,q.w/n};
}
// Anatomical axes avoid assuming an undocumented bone quaternion rest orientation.
inline bool TorsoRotation(Vec3 hips,Vec3 left,Vec3 right,Quat& q){
 Vec3 x{},y{},z{};
 if(!Unit(Sub(right,left),x)||!Unit(Sub(Scale(Add(left,right),0.5f),hips),z)||
    !Unit(Cross(z,x),y))return false;
 z=Cross(x,y);
 const float trace=x.x+y.y+z.z;
 if(trace>0){const float s=std::sqrt(trace+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,0.25f*s};}
 else if(x.x>y.y&&x.x>z.z){const float s=std::sqrt(1+x.x-y.y-z.z)*2;q={0.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
 else if(y.y>z.z){const float s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,0.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
 else {const float s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,0.25f*s,(x.y-y.x)/s};}
 return true;
}
template<class Reader>
bool ReadTorso(Reader read,std::uintptr_t base,std::uintptr_t entity,Quat& rotation,const GameBuild& build=Build150){
 Transform transform{};std::uintptr_t components=0;std::uint16_t count=0;
 if(!read(entity+0x20,transform)||!read(entity+0x78,components)||!components||
    !read(entity+0x82,count)||count>128)return false;
 for(std::uint16_t i=0;i<count;++i){
  std::uintptr_t cp=0,vt=0,owner=0,info=0,map=0,buffer=0,poses=0;std::uint16_t bones=0;
  if(!read(components+8*i,cp)||!cp||!read(cp,vt)||vt!=base+build.skeleton||
     !read(cp+0x10,owner)||owner!=entity)continue;
  if(!read(cp+0x270,info)||!info||!read(info+0x38,map)||!map||!read(info+0x42,bones)||bones>1024||
     !read(cp+0x288,buffer)||!buffer||!read(buffer+0x178,poses)||!poses)return false;
  Vec3 points[3]{};unsigned found=0;
  for(std::uint16_t j=0;j<bones;++j){
   std::uint32_t id=0;std::uint16_t index=0;
   if(!read(map+8*j,id))return false;
   const int slot=id==0xDED10611?0:id==0xB898B609?1:id==0x63D89144?2:-1;
   if(slot<0)continue;
   Vec3 local{};
   if(!read(map+8*j+4,index)||index>=348||!read(poses+32*index,local)||
      !Finite(local)||DistanceSquared(local,{0,0,0})>16||!TransformPoint(transform,local,points[slot]))return false;
   found|=1u<<slot;
  }
  std::uintptr_t current=0;
  if(found!=7||!read(cp+0x288,current)||current!=buffer)return false;
  return TorsoRotation(points[0],points[1],points[2],rotation);
 }
 return false;
}
// Only explicit roll states authorize animation-driven orientation.
inline bool FollowRoll(bool enabled,bool groundRoll,bool hasHead) {return enabled&&groundRoll&&hasHead;}
// Capture the view offset once at entry. Following world-space torso rotation
// includes root turns without applying native camera yaw a second time.
struct ParkourView {
 bool tracking=false;Quat offset{0,0,0,1};
 Quat Step(Quat torso,Quat view,bool active) {
  if(!active){tracking=false;return torso;}
  if(!tracking){offset=Multiply(InverseUnit(torso),view);tracking=true;}
  return Multiply(torso,offset);
 }
};
struct BodyMotion {
 Quat applied{0,0,0,1};
 Quat Step(Quat torso,bool valid,float bodyYaw,float amount,float dt,const Quat* viewReference=nullptr){
  dt=std::isfinite(dt)?std::clamp(dt,0.0f,0.1f):0;
  if(valid&&amount>0.5f){
   const auto delta=Multiply(torso,InverseUnit(viewReference?*viewReference:FromYawPitch(bodyYaw,0)));
   // Blend from the previous orientation, never from upright each frame.
   // Identity-to-pose blending changes branches at 180 degrees mid-roll.
   applied=BlendRotation(applied,delta,1-std::exp(-60*dt));
  } else applied=BlendRotation(applied,{0,0,0,1},1-std::exp(-12*dt));
  return applied;
 }
};
}
