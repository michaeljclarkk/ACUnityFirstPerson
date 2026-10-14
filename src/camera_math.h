#pragma once
#include <cmath>
namespace fp {
struct Vec3 { float x, y, z; };
struct Quat { float x, y, z, w; };
inline bool Finite(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
// AnvilNext uses Z up and camera-local Y forward.
inline bool EyePosition(Vec3 root, Quat q, float height, float forward, Vec3& out) {
    const float n = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (!Finite(root) || !std::isfinite(n) || n < 0.5f || n > 1.5f ||
        !std::isfinite(height) || !std::isfinite(forward)) return false;
    const float s = 2.0f/n;
    out = {root.x + forward*s*(q.x*q.y-q.w*q.z),
           root.y + forward*(1.0f-s*(q.x*q.x+q.z*q.z)),
           root.z + height + forward*s*(q.y*q.z+q.w*q.x)};
    return Finite(out);
}
inline float DistanceSquared(Vec3 a, Vec3 b) {
    const float x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
    return x*x+y*y+z*z;
}
}

namespace fp {
// Keep eye clearance horizontal: pitching down must not move the camera
// down into the open neck. Viewing rotation remains entirely game-controlled.
inline bool LevelEyePosition(Vec3 anchor,Quat q,float height,float forward,Vec3& out) {
    Vec3 direction{};
    if(!EyePosition({0,0,0},q,0,1,direction) || !Finite(anchor) ||
       !std::isfinite(height) || !std::isfinite(forward)) return false;
    const float length=std::sqrt(direction.x*direction.x+direction.y*direction.y);
    if(length>0.0001f) { direction.x/=length; direction.y/=length; }
    else { direction.x=0; direction.y=1; }
    out={anchor.x+forward*direction.x,anchor.y+forward*direction.y,anchor.z+height};
    return Finite(out);
}
}
