#pragma once
#include "camera_comfort.h"
#include <algorithm>
namespace fp {
// A spring-damped additive eye offset over the settled eye: a step cycle driven
// by real planar travel, and a lateral sway driven by turning rate. It owns no
// camera state and returns a local offset only; the caller adds it to the eye.
struct GaitRig {
 struct Offset { float right=0.0f, up=0.0f; };
 static constexpr float VerticalMetres=0.010f;   // peak rise per stride
 static constexpr float LateralMetres=0.007f;    // hip sway per stride
 static constexpr float SwayMetres=0.006f;       // metres per radian/second
 static constexpr float SwayLimit=0.020f;
 static constexpr float WalkStart=0.35f;         // m/s before a step counts
 static constexpr float WalkFull=3.85f;          // m/s where the gait is full
 static constexpr float TeleportDistance=5.0f;   // metres between two frames
 static constexpr float MaxTravelSpeed=12.0f;    // above this a frame is a cut
 Vec3 lastRoot{};
 float lastYaw=0.0f;
 float phase=0.0f;
 float envelope=0.0f;
 float yawSpeed=0.0f;
 float sway=0.0f;
 bool initialized=false;
 void Reset() { *this=GaitRig{}; }
 // A cut or unreadable frame restarts the cycle instead of throwing the camera.
 void Restart(Vec3 root,float yaw) {
  Reset();
  lastRoot=root;
  lastYaw=yaw;
  initialized=true;
 }
 Offset Step(Vec3 root,float yaw,float dt,bool enabled,float intensity=1.0f) {
  if(!Finite(root)||!std::isfinite(yaw)||!std::isfinite(dt)||!std::isfinite(intensity))
   return {};
  dt=std::clamp(dt,0.0f,0.1f);
  if(!initialized||DistanceSquared(root,lastRoot)>TeleportDistance*TeleportDistance) {
   Restart(root,yaw);
   return {};
  }
  const Vec3 travel=Sub(root,lastRoot);
  lastRoot=root;
  const float speed=dt>0.0f?std::sqrt(travel.x*travel.x+travel.y*travel.y)/dt:0.0f;
  if(!std::isfinite(speed)||speed>MaxTravelSpeed) {
   Restart(root,yaw);
   return {};
  }
  const float turn=Wrap(yaw-lastYaw);
  lastYaw=yaw;
  const float turnSpeed=dt>0.0f?turn/dt:0.0f;
  // Gait, sway and yaw speed share the same exponential half-life model.
  yawSpeed+=(turnSpeed-yawSpeed)*(1.0f-std::exp(-0.69314718056f*dt/0.12f));
  const float gait=std::clamp((speed-WalkStart)/(WalkFull-WalkStart),0.0f,1.0f)*
   std::clamp(intensity,0.0f,1.0f);
  const float target=enabled?gait:0.0f;
  const float halfLife=target>envelope?0.12f:0.35f;
  envelope+=(target-envelope)*(1.0f-std::exp(-0.69314718056f*dt/halfLife));
  if(envelope>0.0005f) {
   const float frequency=std::clamp(1.60f+0.12f*speed,1.60f,2.80f);
   phase=std::fmod(phase+2.0f*Pi*frequency*dt,2.0f*Pi);
  } else phase=0.0f;
  // A turn lags the eye sideways the way a body-mounted rig does. The gait
  // envelope keeps the sway from returning while standing still.
  const float wantedSway=std::clamp(-yawSpeed*SwayMetres,-SwayLimit,SwayLimit)*envelope;
  sway+=(wantedSway-sway)*(1.0f-std::exp(-0.69314718056f*dt/0.08f));
  Offset offset{};
  offset.up=VerticalMetres*envelope*std::sin(2.0f*phase);
  offset.right=LateralMetres*envelope*std::cos(phase)+sway;
  return offset;
 }
};
}
