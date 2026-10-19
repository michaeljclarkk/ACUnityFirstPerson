#include "camera_gait.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool Near(float a,float b,float tolerance){return std::abs(a-b)<=tolerance;}
// Planar movement advances X; Z is up in AnvilNext.
void Advance(fp::GaitRig& rig,fp::Vec3& root,float speed,float dt,bool enabled=true,float intensity=1.0f){
 root.x+=speed*dt;
 rig.Step(root,0.0f,dt,enabled,intensity);
}
int main(){
 const float step=1/60.0f;
 // A disabled rig must contribute nothing, even while travelling.
 {
  fp::GaitRig rig;fp::Vec3 root{};float peak=0;
  for(int i=0;i<240;++i){root.x+=3.0f*step;const auto o=rig.Step(root,0.0f,step,false);peak=std::max(peak,std::max(std::abs(o.right),std::abs(o.up)));}
  Check(peak<0.0001f,"disabled camera adds no offset");
 }
 // A still player gets no bob.
 {
  fp::GaitRig rig;const fp::Vec3 root{};
  for(int i=0;i<240;++i){const auto o=rig.Step(root,0.0f,step,true);Check(o.right==0.0f&&o.up==0.0f,"standing still does not bob");}
 }
 // Walking produces a bounded step cycle.
 {
  fp::GaitRig rig;fp::Vec3 root{};float peakUp=0,peakRight=0;
  for(int i=0;i<180;++i){root.x+=2.5f*step;const auto o=rig.Step(root,0.0f,step,true);
   peakUp=std::max(peakUp,std::abs(o.up));peakRight=std::max(peakRight,std::abs(o.right));}
  Check(peakUp>0.004f,"walking raises the eye");
  Check(peakUp<=fp::GaitRig::VerticalMetres+0.0001f,"step rise stays bounded");
  Check(peakRight>0.002f,"walking sways the hips");
  Check(peakRight<=fp::GaitRig::LateralMetres+0.0001f,"step sway stays bounded");
 }
 // Sprinting saturates the gait instead of exceeding it.
 {
  fp::GaitRig rig;fp::Vec3 root{};float peakUp=0,peakRight=0;
  for(int i=0;i<240;++i){root.x+=8.0f*step;const auto o=rig.Step(root,0.0f,step,true);
   peakUp=std::max(peakUp,std::abs(o.up));peakRight=std::max(peakRight,std::abs(o.right));}
  Check(peakUp<=fp::GaitRig::VerticalMetres+0.0001f,"sprint rise stays bounded");
  Check(peakRight<=fp::GaitRig::LateralMetres+0.0001f,"sprint sway stays bounded");
 }
 // The cycle follows elapsed time, not frame count.
 {
  float peaks[3]{};
  const float rates[3]={30.0f,60.0f,144.0f};
  for(int r=0;r<3;++r){
   fp::GaitRig rig;fp::Vec3 root{};float peak=0;
   const float dt=1/rates[r];
   for(int i=0;i<static_cast<int>(rates[r]*2);++i){root.x+=2.5f*dt;const auto o=rig.Step(root,0.0f,dt,true);
    if(i>static_cast<int>(rates[r]*0.75))peak=std::max(peak,std::abs(o.up));}
   peaks[r]=peak;
  }
  Check(Near(peaks[0],peaks[1],0.003f)&&Near(peaks[1],peaks[2],0.003f),"frame rate does not change the gait");
 }
 // A teleport restarts the cycle instead of throwing the camera.
 {
  fp::GaitRig rig;fp::Vec3 root{};
  for(int i=0;i<120;++i){root.x+=3.0f*step;rig.Step(root,0.0f,step,true);}
  root.x+=10.0f;
  const auto o=rig.Step(root,0.0f,step,true);
  Check(o.right==0.0f&&o.up==0.0f,"teleport clears the offset");
 }
 // Stopping lets the cycle and the sway fall away.
 {
  fp::GaitRig rig;fp::Vec3 root{};
  for(int i=0;i<120;++i){root.x+=3.0f*step;rig.Step(root,0.0f,step,true);}
  fp::GaitRig::Offset o{};
  for(int i=0;i<180;++i)o=rig.Step(root,0.0f,step,true);
  Check(std::abs(o.right)<0.0005f&&std::abs(o.up)<0.0005f,"stopping settles the camera");
 }
 // Turning adds a bounded lateral sway on top of the gait.
 {
  fp::GaitRig rig;fp::Vec3 root{};float yaw=0,peakRight=0;
  for(int i=0;i<180;++i){
   root.x+=2.5f*step;yaw+=3.0f*step;
   const auto o=rig.Step(root,yaw,step,true);
   peakRight=std::max(peakRight,std::abs(o.right));
  }
  Check(peakRight>fp::GaitRig::LateralMetres+0.001f,"turning adds look sway");
  Check(peakRight<=fp::GaitRig::LateralMetres+fp::GaitRig::SwayLimit+0.0001f,"look sway stays bounded");
 }
 // Zero intensity is the same as disabled.
 {
  fp::GaitRig rig;fp::Vec3 root{};
  for(int i=0;i<120;++i){root.x+=3.0f*step;rig.Step(root,0.0f,step,true);}
  fp::GaitRig::Offset o{};
  for(int i=0;i<180;++i){root.x+=3.0f*step;o=rig.Step(root,0.0f,step,true,0.0f);}
  Check(std::abs(o.right)<0.0005f&&std::abs(o.up)<0.0005f,"zero intensity settles the camera");
 }
 // Degenerate frames never produce a non-finite offset.
 {
  fp::GaitRig rig;fp::Vec3 root{};
  const float deltas[5]={0.0f,0.004f,0.02f,0.05f,0.1f};
  for(int i=0;i<500;++i){
   const float dt=deltas[i%5];
   if(i%37==0)root.x+=7.5f;
   if(i%53==0)root.z+=2.0f;
   const auto o=rig.Step(root,i*0.31f+i*0.013f,dt,(i%7)!=0,(i%11)*0.1f);
   Check(std::isfinite(o.right)&&std::isfinite(o.up),"offset stays finite");
  }
 }
 std::puts("Gait cycle, sway, settling and degenerate frames passed.");
}
