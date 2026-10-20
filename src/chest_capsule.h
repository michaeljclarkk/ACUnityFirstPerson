#pragma once
#include "body_motion.h"
namespace fp {
// Union of spheres along the hip-to-shoulder segment, with a linear taper.
// Negative clearance means inside the torso, except for the head carve-out.
inline bool ChestClearance(const TorsoPoints& torso,Vec3 head,Vec3 eye,float& clearance,Vec3* normal=nullptr) {
 const Vec3 top=Scale(Add(torso.left,torso.right),0.5f);
 const Vec3 axis=Sub(top,torso.hips);
 const float length=Length(axis),width=Length(Sub(torso.right,torso.left));
 if(!Finite(torso.hips)||!Finite(torso.left)||!Finite(torso.right)||!Finite(head)||!Finite(eye)||
    length<0.15f||length>1.0f||width<0.10f||width>0.8f)return false;
 constexpr float hipRadius=0.13f,shoulderRadius=0.16f,headRadius=0.12f;
 const Vec3 delta=Sub(eye,torso.hips),unit=Scale(axis,1/length);
 const float axial=Dot(delta,unit);
 const float radial=Length(Sub(delta,Scale(unit,axial)));
 const float slope=(shoulderRadius-hipRadius)/length;
 // Analytic minimum of distance(eye, segment(t)) - radius(t).
 const float along=std::clamp(axial+slope*radial/std::sqrt(1-slope*slope),0.0f,length);
 const float torsoDistance=Length(Sub(delta,Scale(unit,along)))-(hipRadius+slope*along);
 const float headDistance=headRadius-Length(Sub(eye,head));
 clearance=std::max(torsoDistance,headDistance);
 if(normal) {
  const Vec3 direction=headDistance>torsoDistance?Sub(head,eye):Sub(delta,Scale(unit,along));
  if(!Unit(direction,*normal)) {
   // The axis has no unique radial normal; choose anatomical forward.
   if(!Unit(Cross(axis,Sub(torso.right,torso.left)),*normal))return false;
  }
 }
 return std::isfinite(clearance);
}
// Small penetrations are corrected locally, before considering third person.
// The caller must wall-check this move and test the resulting eye again.
inline Vec3 CorrectChest(const TorsoPoints& torso,Vec3 head,Vec3 eye,bool valid) {
 float clearance=0;Vec3 normal{};
 if(!valid||!ChestClearance(torso,head,eye,clearance,&normal)||clearance>=0)return eye;
 return Add(eye,Scale(normal,std::min(0.08f,0.005f-clearance)));
}
struct ChestGuard {
 bool blocked=false;
 float recovery=0,missing=0,penetrating=0;
 bool Scope(bool manoeuvre,float dt) {
  dt=std::isfinite(dt)?std::clamp(dt,0.0f,0.1f):0;
  recovery=manoeuvre?0.25f:std::max(0.0f,recovery-dt);
  if(!manoeuvre&&recovery<=0){blocked=false;missing=0;penetrating=0;return false;}
  return true;
 }
 bool Step(const TorsoPoints& torso,Vec3 head,Vec3 eye,bool valid,float dt) {
  float clearance=0;
  valid=valid&&ChestClearance(torso,head,eye,clearance);
  dt=std::isfinite(dt)?std::clamp(dt,0.0f,0.1f):0;
  if(!valid) {
   // Missing torso data is not evidence of penetration. Do not manufacture
   // a fallback on jumps with transient skeleton replacement.
   missing+=dt;penetrating=0;
   if(missing>0.15f)blocked=false;
  } else {
   missing=0;
   penetrating=clearance < -0.03f?penetrating+dt:0;
   if(penetrating>=0.12f)blocked=true;
   else if(clearance>0.01f)blocked=false;
  }
  return blocked;
 }
};
}
