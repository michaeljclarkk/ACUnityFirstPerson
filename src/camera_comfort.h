#pragma once
#include "camera_math.h"
#include <algorithm>
namespace fp {
constexpr float Pi=3.14159265358979323846f;
inline Vec3 Add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 Sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 Scale(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float Length(Vec3 a){return std::sqrt(Dot(a,a));}
inline float Wrap(float angle){return std::remainder(angle,2*Pi);}
inline Quat FromYawPitch(float yaw,float pitch) {
 const float sy=std::sin(yaw/2),cy=std::cos(yaw/2),sp=std::sin(pitch/2),cp=std::cos(pitch/2);
 return {cy*sp,sy*sp,sy*cp,cy*cp};
}
// Preserve native yaw/pitch: movement is resolved against Unity's camera.
struct LookController {
 float previousYaw=0,previousBody=0,relative=0,pitch=0;
 bool atLimit=false;
 bool Step(Quat input,Vec3 body,float,float,float,Quat& output) {
  Vec3 forward{};
  if(!EyePosition({0,0,0},input,0,1,forward)||!Finite(body))return false;
  previousYaw=std::atan2(-forward.x,forward.y);
  previousBody=std::atan2(-body.x,body.y);
  relative=Wrap(previousYaw-previousBody);
  pitch=std::asin(std::clamp(forward.z,-1.0f,1.0f));
  output=FromYawPitch(previousYaw,pitch);
  return true;
 }
};
struct CameraFallback {
 bool active=false;
 float clearTime=0;
 bool Step(float pullback,float relativeYaw,bool hasHead,float dt,bool automatic=true) {
  const bool enter=!hasHead||(automatic&&(pullback>0.25f||std::abs(relativeYaw)>85*Pi/180));
  const bool clear=hasHead&&(!automatic||(pullback<0.10f&&std::abs(relativeYaw)<70*Pi/180));
  if(enter){active=true;clearTime=0;}
  else if(active){
   clearTime=clear?clearTime+std::clamp(dt,0.0f,0.1f):0;
   if(clearTime>=0.75f){active=false;clearTime=0;}
  }
  return active;
 }
};
// Start local clearance near the upper torso, not on the native camera boom.
inline Vec3 ClearanceOrigin(Vec3 root,Vec3 anchor) {
 return {root.x,root.y,root.z+std::clamp(anchor.z-root.z-0.30f,0.20f,1.50f)};
}
struct HeadFilter {
 // Roughly one neck length: beyond this the chest can rotate through an eye
 // the animation has already left behind.
 static constexpr float LagLimit=0.08f;
 static constexpr float LagRecoveryRate=150.0f;
 static constexpr float LagMinimumSpeed=2.0f;
 // A head pose that moves this far in one frame is an animation state change,
 // not motion. Chasing it drags the eye through the body, so adopt it instead.
 static constexpr float PoseJumpLimit=0.25f;
 bool initialized=false,hasTracked=false,hasWanted=false;
 Vec3 offset{},lastRoot{},lastWanted{},lastDelta{};
 float missingTime=0;
 // Diagnostics: how far the eye trails the pose, and how many pose jumps were
 // adopted. Both are read by the plugin's one-second report.
 float lastLag=0;
 unsigned jumps=0;
 // A transient skeleton-buffer replacement must not switch camera ownership.
 bool Available() const {return hasTracked&&missingTime<=0.15f;}
 Vec3 Step(Vec3 root,Vec3 head,bool valid,float dt,float halfLife,float fallbackHeight,bool closePose=false) {
  if(!std::isfinite(dt)||dt<0)dt=0;
  dt=std::min(dt,0.1f);
  valid=valid&&Finite(head);
  if(!initialized || DistanceSquared(root,lastRoot)>25.0f) {
   offset=valid?Sub(head,root):Vec3{0,0,fallbackHeight};
   initialized=true;missingTime=0;hasTracked=false;hasWanted=false;
  }
  lastRoot=root;
  hasTracked=hasTracked||valid;
  missingTime=valid?0:missingTime+dt;
  Vec3 wanted=valid?Sub(head,root):offset;
  if(!valid&&missingTime>0.35f)wanted={0,0,fallbackHeight};
  // A pose that reverses between frames is animation noise, not travel.
  // Pulling the eye toward it is what made the 8 cm override vibrate, so only
  // motion that keeps going forwards is corrected. Direction is unknown until
  // two consecutive poses have been seen.
  const bool directionKnown=hasWanted&&Length(lastDelta)>0.001f;
  float poseSpeed=0;
  bool reverse=false,poseJump=false;
  if(valid&&dt>0) {
   if(hasWanted) {
    const Vec3 poseDelta=Sub(wanted,lastWanted);
    poseSpeed=Length(poseDelta)/dt;
    if(directionKnown) {
     reverse=Dot(poseDelta,lastDelta)<0;
     poseJump=!reverse&&Length(poseDelta)>PoseJumpLimit;
    }
    lastDelta=poseDelta;
   }
   lastWanted=wanted;
   hasWanted=true;
  }
  // An animation cut is matched by a cut, not by a swoop through the body.
  if(poseJump) {
   offset=wanted;
   ++jumps;
  }
  const float alpha=halfLife>0 ? 1-std::exp(-0.69314718056f*dt/halfLife):1;
  // Smooth only animation displacement; world translation follows immediately.
  Vec3 change=Scale(Sub(wanted,offset),alpha);
  const float distance=Length(change),limit=3.0f*dt;
  if(distance>limit&&distance>0)change=Scale(change,limit/distance);
  offset=Add(offset,change);
  // Bound how far the eye may trail the animated head. The correction may never
  // outrun the pose that is being followed, so a large lag cannot become a jump.
  if(valid&&directionKnown&&!reverse&&!poseJump) {
   const Vec3 lag=Sub(wanted,offset);
   const float lagLength=Length(lag);
   if(lagLength>LagLimit) {
    const float excess=lagLength-LagLimit;
    const float speed=std::min(excess*LagRecoveryRate,std::max(LagMinimumSpeed,poseSpeed));
    const float step=std::min(excess,speed*dt);
    if(step>0)offset=Add(offset,Scale(lag,step/lagLength));
   }
  }
  // Only deliberate roll/dive tracking may bypass the animation speed limit.
  // Applying this clamp to parkour defeats smoothing on every noisy pose.
  if(valid&&closePose) {
   const Vec3 lag=Sub(wanted,offset);
   const float lagLength=Length(lag);
   if(lagLength>0.08f)offset=Sub(wanted,Scale(lag,0.08f/lagLength));
  }
  lastLag=valid?Length(Sub(wanted,offset)):0;
  return Add(root,offset);
 }
};
// Retraction is immediate; only release toward a clear eye position is smoothed.
inline float CollisionDistance(float previous,float allowed,float dt) {
 if(allowed<previous)return allowed;
 return previous+(allowed-previous)*(1-std::exp(-12.0f*std::max(dt,0.0f)));
}
inline float SafeHitDistance(Vec3 start,Vec3 direction,float length,Vec3 hit,float padding) {
 const float distance=Dot(Sub(hit,start),direction);
 if(!Finite(hit)||!std::isfinite(distance)||distance<0||distance>length+0.01f)return length;
 return std::clamp(distance-padding,0.0f,length);
}
}
