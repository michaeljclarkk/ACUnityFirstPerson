#include "camera_comfort.h"
#include <cstdio>
#include <cstdlib>
void Check(bool b,const char* msg){if(!b){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
 fp::Quat out{};fp::Vec3 f{};
 const float deg=fp::Pi/180;
 fp::LookController look;
 for(int i=-1080;i<=1080;++i) {
  Check(look.Step(fp::FromYawPitch(i*deg,-25*deg),{0,1,0},80*deg,60*deg,70*deg,out),"native look");
  fp::EyePosition({0,0,0},out,0,1,f);
  Check(std::abs(fp::Wrap(std::atan2(-f.x,f.y)-i*deg))<0.001f,"no yaw drift after multiple revolutions");
  Check(Near(std::asin(f.z),-25*deg),"no pitch drift");
 }
 fp::CameraFallback fallback;
 Check(fallback.Step(5.556f,0,true,0.016f),"large pullback suspends first person");
 for(int i=0;i<10;++i)Check(fallback.Step(0,0,true,0.016f),"clearance holds fallback briefly");
  for(int i=0;i<20;++i)Check(fallback.Step(0,0,true,0.016f),"short clearance cannot cause repeated switch");
 for(int i=0;i<20;++i)fallback.Step(0,0,true,0.016f);
 Check(!fallback.active,"clearance restores first person");
 Check(fallback.Step(0,90*deg,true,0.016f),"looking behind uses native camera");
 Check(fallback.Step(0,75*deg,true,0.1f),"angle hysteresis prevents flapping");
 Check(fallback.Step(0,0,false,0.1f),"missing head suspends first person");
 const auto origin=fp::ClearanceOrigin({10,20,30},{10.1f,20.2f,31.5f});
 Check(Near(origin.x,10)&&Near(origin.y,20)&&Near(origin.z,31.2f),"probe follows upper torso");
 const auto crouch=fp::ClearanceOrigin({10,20,30},{10,20,30.8f});
 Check(Near(crouch.z,30.5f),"probe lowers while crouched");
 fp::CameraFallback persistent;
 Check(!persistent.Step(0,179*deg,true,0.016f,false),"rear look stays first person");
 Check(!persistent.Step(0.608f,0,true,0.016f,false),"successful local wall adjustment stays first person");
 Check(!persistent.Step(0.608f,-179*deg,true,0.016f,false),"combined turn and wall adjustment stays first person");
 Check(persistent.Step(0,0,false,0.016f,false),"missing tracking still uses native camera");
 for(int i=0;i<60;++i)persistent.Step(0.608f,179*deg,true,0.016f,false);
 Check(!persistent.active,"tracking recovery does not require forward-facing view");
 fp::HeadFilter filter;
 auto p=filter.Step({0,0,0},{0,0,1.5f},true,0,0.07f,1.5f);
 p=filter.Step({10,0,0},{10,0,1.5f},true,1/60.0f,0.07f,1.5f);
 Check(Near(p.x,10),"teleport resets");
 p=filter.Step({10.1f,0,0},{10.1f,0,1.5f},true,1/60.0f,0.07f,1.5f);
 Check(Near(p.x,10.1f),"movement has no smoothing lag");
 p=filter.Step({10.1f,0,0},{10.1f,0,0.5f},true,1/60.0f,0.07f,1.5f,true);
 Check(p.z<=0.581f&&p.z>=0.5f,"large pose changes keep eye within eight centimetres");
 // Alternating parkour poses used to override the 3 m/s limiter by snapping
 // back to the eight-centimetre envelope each frame. Test several frame rates.
 for(const float rate:{30.0f,60.0f,144.0f}) {
  fp::HeadFilter parkour;
  auto previous=parkour.Step({0,0,0},{0,0,1.5f},true,0,0.07f,1.5f);
  float low=2,high=0;
  for(int i=0;i<120;++i) {
   const fp::Vec3 root{0.1f*i,0,0.05f*i};
   const auto current=parkour.Step(root,fp::Add(root,{0,0,i%2?1.8f:1.2f}),true,1/rate,0.07f,1.5f);
   const auto local=fp::Sub(current,root);
   Check(fp::Length(fp::Sub(local,previous))<=3/rate+0.0001f,"parkour respects animation speed limit");
   low=std::min(low,local.z);high=std::max(high,local.z);
   Check(Near(current.x,root.x),"parkour root translation remains immediate");
   previous=local;
  }
  Check(high-low<0.15f,"rapid alternating poses are attenuated");
 }
 // A continuous fast pose change (a dive or a big dive-adjacent animation) must
 // not leave the eye trailing behind the animated head by more than the neck
 // allows, or the chest rotates through the camera.
 for(const float rate:{30.0f,60.0f,144.0f}) {
  fp::HeadFilter dive;
  const fp::Vec3 root{};
  dive.Step(root,{0,0,1.5f},true,0,0.07f,1.5f);
  float peak=0;
  const float dt=1/rate;
  for(int i=0;i<static_cast<int>(rate*2);++i) {
   // 6 m/s: faster than the 3 m/s smoothing limit, so lag would grow unbounded.
   const float height=1.5f+6.0f*i*dt;
   const auto current=dive.Step(root,{0,0,height},true,dt,0.07f,1.5f);
   const float lag=height-(current.z-root.z);
   if(i>2)peak=std::max(peak,lag);
  }
  Check(peak<=fp::HeadFilter::LagLimit+0.02f,"fast forward motion stays near the neck clearance");
 }
 // A reversal must not be chased: that is the vibration 0.8.1 exhibited.
 {
  fp::HeadFilter noisy;
  const fp::Vec3 root{};
  noisy.Step(root,{0,0,1.5f},true,0,0.07f,1.5f);
  float previous=0,low=2,high=0;
  for(int i=0;i<240;++i) {
   const auto current=noisy.Step(root,{0,0,i%2?1.8f:1.2f},true,1/60.0f,0.07f,1.5f);
   const float z=current.z-root.z;
   if(i>0)Check(std::abs(z-previous)<=3/60.0f+0.0001f,"reversing poses stay on the smoothing path");
   previous=z;low=std::min(low,z);high=std::max(high,z);
  }
  Check(high-low<0.15f,"reversing poses are attenuated, not chased");
 }
 // An animation state change moves the head far in one frame. The eye must adopt
 // the new pose instead of swooping through the body to catch up.
 {
  fp::HeadFilter cut;
  const fp::Vec3 root{};
  cut.Step(root,{0,0,1.5f},true,1/60.0f,0.07f,1.5f);
  cut.Step(root,{0,0,1.5f},true,1/60.0f,0.07f,1.5f);
  cut.Step(root,{0,0,1.4f},true,1/60.0f,0.07f,1.5f);
  Check(cut.jumps==0,"ordinary motion is not a pose jump");
  const auto afterCut=cut.Step(root,{0,0,0.3f},true,1/60.0f,0.07f,1.5f);
  Check(cut.jumps==1,"a discontinuous pose is adopted");
  Check(Near(afterCut.z-root.z,0.3f),"the cut pose is taken in one frame");
  Check(cut.lastLag<0.001f,"a cut leaves no lag to sweep through");
 }
 // A large lag must never be repaid faster than the pose itself is moving, or
 // the catch-up reads as a flash.
 {
  fp::HeadFilter slow;
  const fp::Vec3 root{};
  slow.Step(root,{0,0,1.5f},true,1/60.0f,0.07f,1.5f);
  slow.Step(root,{0,0,1.5f},true,1/60.0f,0.07f,1.5f);
  // One smooth, slow frame after forcing a large trailing offset.
  const auto first=slow.Step(root,{0,0,1.5f},true,1/60.0f,0.07f,1.5f);
  float peakStep=0,previous=first.z-root.z;
  for(int i=0;i<120;++i) {
   const auto current=slow.Step(root,{0,0,1.5f+i*0.001f},true,1/60.0f,0.07f,1.5f);
   const float z=current.z-root.z;
   peakStep=std::max(peakStep,std::abs(z-previous));
   previous=z;
  }
  Check(peakStep<0.04f,"recovery never exceeds the pose's own speed");
 }
 fp::HeadFilter dropout;
 dropout.Step({0,0,0},{},false,0,0.07f,1.5f);
 Check(!dropout.Available(),"never invent tracking before first valid pose");
 dropout.Step({0,0,0},{0,0,1.5f},true,0.016f,0.07f,1.5f);
 fp::CameraFallback dropoutFallback;
 for(int i=0;i<5;++i) {
  dropout.Step({0,0,0},{},false,0.016f,0.07f,1.5f);
  Check(!dropoutFallback.Step(0,0,dropout.Available(),0.016f,false),"short pose dropout retains first-person ownership");
 }
 for(int i=0;i<10;++i)dropout.Step({0,0,0},{},false,0.016f,0.07f,1.5f);
 Check(dropoutFallback.Step(0,0,dropout.Available(),0.016f,false),"sustained tracking loss still falls back");
 dropout.Step({10,0,0},{},false,0,0.07f,1.5f);
 Check(!dropout.Available(),"teleport cannot reuse stale tracking");
 const float held=p.z;
 p=filter.Step({10.2f,0,0},{},false,1/60.0f,0.07f,1.5f);
 Check(Near(p.z,held)&&Near(p.x,10.2f),"missing frame holds pose without stopping movement");
 Check(Near(fp::SafeHitDistance({0,0,0},{0,1,0},4,{0,2,0},0.12f),1.88f),"wall margin");
 Check(Near(fp::SafeHitDistance({0,0,0},{0,1,0},4,{0,-1,0},0.12f),4),"behind-origin hit");
 Check(Near(fp::SafeHitDistance({0,0,0},{0,1,0},4,{0,0.05f,0},0.12f),0),"near wall clamp");
 Check(Near(fp::CollisionDistance(4,1,1/60.0f),1),"immediate retraction");
 const float release=fp::CollisionDistance(1,4,1/60.0f);
 Check(release>1&&release<4,"smooth release");
 std::puts("Look limits, angle wrapping, motion, dropout, collision response passed.");
}
