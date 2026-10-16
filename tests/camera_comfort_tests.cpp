#include "camera_comfort.h"
#include "native_yaw.h"
#include "game_build.h"
#include <cstdio>
#include <cstdlib>
void Check(bool b,const char* msg){if(!b){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
 fp::Quat out{};fp::Vec3 f{};
 const float deg=fp::Pi/180;
 fp::LookController look;
 for(int i=-1080;i<=1080;++i) {
  Check(look.Step(fp::FromYawPitch(i*deg,-25*deg),{0,1,0},out),"native look");
  fp::EyePosition({0,0,0},out,0,1,f);
  Check(std::abs(fp::Wrap(std::atan2(-f.x,f.y)-i*deg))<0.001f,"no yaw drift after multiple revolutions");
  Check(Near(std::asin(f.z),-25*deg),"no pitch drift");
 }
 // Exact helper/call context from the independently captured executable text.
 const unsigned char helper[]={0x48,0x8B,0x41,0x08,0x48,0x8B,0x90,0x50,0x01,0,0,0x48,0xFF,0xE2};
 // Captured bytes, rather than constructing an expected call from the profile.
 unsigned char sites[][19]={
  {0x49,0x8B,0x8D,0xC0,0x09,0,0,0xE8,0xF7,0xF3,0x27,0xFE,0x49,0x8B,0x95,0xC0,0x09,0,0},
  {0x49,0x8B,0x8D,0xC0,0x09,0,0,0xE8,0x67,0x32,0x02,0,0x49,0x8B,0x95,0xC0,0x09,0,0}};
 const fp::GameBuild* profiles[]={&fp::Build150,&fp::Build151};
 for(unsigned i=0;i<2;++i) {
  const auto* build=profiles[i];auto& site=sites[i];
  Check(fp::NativeYawSiteMatches(build->yawFinalizeReturn,build->yawModeHelper,site,helper),"captured executable context verified");
  Check(!fp::NativeYawSiteMatches(build->yawFinalizeReturn+1,build->yawModeHelper,site,helper),"wrong call target rejected");
  site[12]=0x90;
  Check(!fp::NativeYawSiteMatches(build->yawFinalizeReturn,build->yawModeHelper,site,helper),"modified native context rejected");
 }
 // Model Unity feeding the corrected current/target into the next update.
 // Repeated excess mouse input must never wind up behind the rendered stop.
 for(const float limitDegrees:{0.0f,30.0f,90.0f,95.0f,100.0f,150.0f,179.0f}) {
  for(const float direction:{-1.0f,1.0f}) {
   fp::NativeYawLimit limiter;
   float current=fp::Pi,target=fp::Pi;
   const float limit=limitDegrees*deg;
   for(int i=0;i<1080;++i) {
    target=fp::Wrap(target-direction*deg);
    current=target;
    Check(limiter.Step(current,target,0,limit),"native feedback accepted");
    const float expected=direction*std::min(static_cast<float>(i+1),limitDegrees)*deg;
    Check(Near(fp::Wrap(fp::Pi-current),expected),"native current stays at stop across excess turns");
    Check(Near(fp::Wrap(fp::Pi-target),expected),"native target discards excess input");
    // The unchanged render controller must see the same heading as movement.
    fp::LookController view;
    Check(view.Step(fp::FromYawPitch(fp::Pi-current,-25*deg),{0,1,0},out),"native view accepted");
    fp::EyePosition({0,0,0},out,0,1,f);
    Check(Near(fp::Wrap(std::atan2(-f.x,f.y)-(fp::Pi-current)),0),"view and movement remain aligned");
   }
   target=fp::Wrap(target+direction*deg);current=target;
   limiter.Step(current,target,0,limit);
   Check(Near(limiter.relative,direction*std::max(0.0f,limitDegrees-1)*deg),"native reversal has no dead zone");
  }
 }
 // A large step can cross the rear seam before reaching the limiter.
 fp::NativeYawLimit seam;
 float current=fp::Wrap(fp::Pi-94*deg),target=current;
 seam.Step(current,target,0,95*deg);
 current=fp::Wrap(current-100*deg);target=current;
 seam.Step(current,target,0,95*deg);
 Check(Near(seam.relative,95*deg),"rear seam cannot switch stops");
 // Body catches up to the corrected native camera; no offset may remain.
 const float body=95*deg;
 seam.Step(current,target,body,95*deg);
 Check(Near(seam.relative,0)&&Near(fp::Wrap(fp::Pi-current),body),"body catch-up aligns view and movement");
 // Body/world heading wraps must not disturb an ordinary 20-degree look.
 fp::NativeYawLimit bodyLimit;
 for(int i=170;i<=550;++i) {
  current=fp::Wrap(fp::Pi-(i+20)*deg);target=current;
  bodyLimit.Step(current,target,fp::Wrap(i*deg),90*deg);
  Check(Near(bodyLimit.relative,20*deg),"world/body wraps preserve relative yaw");
 }
 fp::NativeYawLimit unrestricted;
 current=12.345f;target=-9.876f;
 unrestricted.Step(current,target,0,fp::Pi);
 Check(current==12.345f&&target==-9.876f,"180 degrees leaves native orbit untouched");
 Check(!unrestricted.Step(current,target,0,NAN),"invalid limit rejected");
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
