#pragma once
#include "camera_comfort.h"
#include <cstdint>
#include <cstring>
namespace fp {
// Verify the call's ownership/context and the helper's tail dispatch to +150.
inline bool NativeYawSiteMatches(std::uintptr_t ret,std::uintptr_t helper,
    const unsigned char (&site)[19],const unsigned char (&stub)[14]) {
 const unsigned char before[]={0x49,0x8B,0x8D,0xC0,0x09,0,0,0xE8};
 const unsigned char after[]={0x49,0x8B,0x95,0xC0,0x09,0,0};
 const unsigned char expected[]={0x48,0x8B,0x41,0x08,0x48,0x8B,0x90,0x50,0x01,0,0,0x48,0xFF,0xE2};
 std::int32_t displacement=0;std::memcpy(&displacement,site+8,sizeof(displacement));
 return std::memcmp(site,before,sizeof(before))==0&&
     std::memcmp(site+12,after,sizeof(after))==0&&ret+displacement==helper&&
     std::memcmp(stub,expected,sizeof(expected))==0;
}
// Native orbit uses viewYaw = Pi - orbit. Feed corrections back before Unity
// builds its view/movement quaternions; never clamp the rendered quaternion.
struct NativeYawLimit {
 bool initialized=false,atLimit=false;
 float lastOrbit=0,lastBody=0,relative=0;
 void Reset(){*this=NativeYawLimit{};}
 bool Step(float& current,float& target,float bodyYaw,float limit) {
  if(!std::isfinite(current)||!std::isfinite(target)||!std::isfinite(bodyYaw)||
     !std::isfinite(limit)||limit<0||limit>Pi)return false;
  if(limit>=Pi){Reset();return true;}
  const float wanted=initialized ? relative-Wrap(current-lastOrbit)-Wrap(bodyYaw-lastBody) :
      Wrap(Pi-current-bodyYaw);
  // Follow the same unwrapped branch for the target, including a large mouse
  // step across the rear seam. Excess input must not survive in the target.
  const float wantedTarget=wanted-Wrap(target-current);
  relative=std::clamp(wanted,-limit,limit);
  const float limitedTarget=std::clamp(wantedTarget,-limit,limit);
  atLimit=std::abs(wanted)>=limit||std::abs(wantedTarget)>=limit;
  current=Wrap(Pi-bodyYaw-relative);
  target=Wrap(Pi-bodyYaw-limitedTarget);
  lastOrbit=current;lastBody=bodyYaw;initialized=true;
  return true;
 }
};
}
