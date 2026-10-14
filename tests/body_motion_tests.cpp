#include "body_motion.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
void Check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
bool Near(fp::Vec3 a,fp::Vec3 b){return fp::DistanceSquared(a,b)<0.001f;}
using fp::Vec3;
struct Memory {
 std::array<unsigned char,0x10000> bytes{};
 template<class T> void put(std::uintptr_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof(v));}
 template<class T> bool operator()(std::uintptr_t p,T& v)const {
  if(p>bytes.size()||sizeof(v)>bytes.size()-p)return false;
  std::memcpy(&v,bytes.data()+p,sizeof(v));return true;
 }
};
int main(){
 const Vec3 hips{0,0,0},left{-0.2f,0,0.6f},right{0.2f,0,0.6f};
 fp::Quat neutral{};
 Check(fp::TorsoRotation(hips,left,right,neutral),"standing torso frame");
 Check(Near(fp::Rotate(neutral,{0,1,0}),{0,1,0}),"standing faces forward");
 for(int axis=0;axis<3;++axis){
  fp::BodyMotion motion;
  Vec3 previous{};
  for(int degree=0;degree<=720;++degree){
   const float a=degree*fp::Pi/180;
   fp::Quat pose{axis==0?std::sin(a/2):0,axis==1?std::sin(a/2):0,axis==2?std::sin(a/2):0,std::cos(a/2)};
   fp::Quat torso{};
   Check(fp::TorsoRotation(hips,fp::Rotate(pose,left),fp::Rotate(pose,right),torso),"full roll/spin torso reconstructed");
   const auto delta=motion.Step(torso,true,0,1,1/120.0f);
   const auto up=fp::Rotate(delta,{0,0,1});
   Check(Near(up,fp::Rotate(pose,{0,0,1})),"inverted torso is retained");
   Check(Near(fp::Rotate(delta,{0,1,0}),fp::Rotate(pose,{0,1,0})),"animated forward direction retained");
   const Vec3 offset{0,0.16f,0.12f};
   Check(Near(fp::Rotate(delta,offset),fp::Rotate(pose,offset)),"eye offset rotates with animation");
   if(degree)Check(fp::DistanceSquared(up,previous)<0.001f,"no 180-degree branch jump");
   previous=up;
  }
 }
 fp::BodyMotion entering;
 Vec3 previousForward{0,1,0};
 for(int degree=0;degree<=360;++degree){
  const auto pose=fp::FromYawPitch(0,degree*fp::Pi/180);
  const auto delta=entering.Step(pose,true,0,0.7f,1/120.0f);
  const auto forward=fp::Rotate(delta,{0,1,0});
  Check(fp::DistanceSquared(forward,previousForward)<0.002f,"partial activation cannot flip at 180 degrees");
  previousForward=forward;
 }
 Check(!fp::TorsoRotation(hips,left,left,neutral),"degenerate shoulders rejected");
 Check(!fp::FollowRoll(true,false,true),"ordinary combat never enables torso tracking");
 Check(!fp::FollowRoll(false,true,true),"disabled roll tracking stays upright");
 Check(!fp::FollowRoll(true,true,false),"missing head cannot enable roll tracking");
 Check(fp::FollowRoll(true,true,true),"recognized roll enables tracking");
 fp::BodyMotion attack;
 for(int i=0;i<120;++i){
  const auto violentPose=fp::FromYawPitch(i*0.1f,2.5f);
  const auto output=attack.Step(violentPose,true,0,0,1/60.0f);
  Check(Near(fp::Rotate(output,{0,1,0}),{0,1,0}),"attack bends and spins cannot rotate camera");
 }
 fp::BodyMotion loss;
 loss.Step(fp::FromYawPitch(0,1),true,0,1,0.016f);
 for(int i=0;i<180;++i)loss.Step({},false,0,0,1/60.0f);
 Check(Near(fp::Rotate(loss.applied,{0,0,1}),{0,0,1}),"lost pose eases to upright");
 fp::BodyMotion dive;
 const auto nativeView=fp::FromYawPitch(1.8f,0.4f);
 const auto divePose=fp::FromYawPitch(0.2f,-1.2f);
 fp::Quat correction{};
 for(int i=0;i<90;++i)correction=dive.Step(divePose,true,0,1,1/60.0f,&nativeView);
 Check(Near(fp::Rotate(fp::Multiply(correction,nativeView),{0,1,0}),fp::Rotate(divePose,{0,1,0})),"leap follows dive instead of native cinematic orbit");
 for(int i=0;i<120;++i)correction=dive.Step({},false,0,0,1/60.0f);
 Check(Near(fp::Rotate(correction,{0,1,0}),{0,1,0}),"landing restores native view");
 Memory m;
 constexpr std::uintptr_t base=0x140000000,entity=0x100,components=0x400,cp=0x600,info=0x1000,map=0x2000,buffer=0x3000,poses=0x4000;
 fp::Transform transform{{1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1}};
 m.put(entity+0x20,transform);m.put(entity+0x78,components);m.put(entity+0x82,std::uint16_t(1));
 m.put(components,cp);m.put(cp,base+0x2E76630);m.put(cp+0x10,entity);m.put(cp+0x270,info);
 m.put(info+0x38,map);m.put(info+0x42,std::uint16_t(3));m.put(cp+0x288,buffer);m.put(buffer+0x178,poses);
 const std::uint32_t ids[]={0xDED10611,0xB898B609,0x63D89144};const Vec3 positions[]={hips,left,right};
 for(int i=0;i<3;++i){m.put(map+8*i,ids[i]);m.put(map+8*i+4,std::uint16_t(i));m.put(poses+32*i,positions[i]);}
 Check(fp::ReadTorso(m,base,entity,neutral),"mapped torso read");
 Check(Near(fp::Rotate(neutral,{0,0,1}),{0,0,1}),"translation does not affect orientation");
 m.put(map+4,std::uint16_t(348));Check(!fp::ReadTorso(m,base,entity,neutral),"out of bounds bone rejected");
 m.put(map+4,std::uint16_t(0));m.put(info+0x42,std::uint16_t(2));Check(!fp::ReadTorso(m,base,entity,neutral),"missing shoulder rejected");
 std::puts("Torso reader, full rolls/spins, eye offsets, normal turns and recovery passed.");
}
