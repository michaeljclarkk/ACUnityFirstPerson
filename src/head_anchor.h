#pragma once
#include "camera_math.h"
#include "game_build.h"
#include <cstdint>
namespace fp {
struct Transform { float m[16]; };
inline bool TransformPoint(const Transform& t,Vec3 p,Vec3& out) {
    for (float v:t.m) if (!std::isfinite(v)) return false;
    if(!Finite(p)) return false;
    out={t.m[0]*p.x+t.m[4]*p.y+t.m[8]*p.z+t.m[12],
         t.m[1]*p.x+t.m[5]*p.y+t.m[9]*p.z+t.m[13],
         t.m[2]*p.x+t.m[6]*p.y+t.m[10]*p.z+t.m[14]};
    return Finite(out);
}
// Read only the published ACU 1.5.0 skeleton layout. No game function calls.
template<class Reader>
bool ReadHead(Reader read,std::uintptr_t base,std::uintptr_t entity,Vec3& world,const GameBuild& build=Build150) {
    std::uintptr_t components=0;
    std::uint16_t componentCount=0;
    Transform transform{};
    if(!read(entity+0x20,transform) || !read(entity+0x78,components) || !components ||
       !read(entity+0x82,componentCount) || componentCount>128) return false;
    for(std::uint16_t i=0;i<componentCount;++i) {
        std::uintptr_t component=0,vt=0,owner=0;
        if(!read(components+8*i,component) || !component ||
           !read(component,vt) || vt!=base+build.skeleton ||
           !read(component+0x10,owner) || owner!=entity) continue;
        std::uintptr_t info=0,mapping=0,buffer=0,poses=0;
        std::uint16_t count=0;
        if(!read(component+0x270,info) || !info ||
           !read(info+0x38,mapping) || !mapping ||
           !read(info+0x42,count) || count>1024 ||
           !read(component+0x288,buffer) || !buffer ||
           !read(buffer+0x178,poses) || !poses) return false;
        for(std::uint16_t j=0;j<count;++j) {
            std::uint32_t id=0;
            std::uint16_t index=0;
            if(!read(mapping+8*j,id)) return false;
            if(id!=0x07C159A2) continue;
            Vec3 local{};
            if(!read(mapping+8*j+4,index) || index>=348 ||
               !read(poses+0x20*index,local) || !Finite(local) ||
               DistanceSquared(local,{0,0,0})>16.0f) return false;
            std::uintptr_t currentBuffer=0;
            if(!read(component+0x288,currentBuffer) || currentBuffer!=buffer) return false;
            if(!TransformPoint(transform,local,world)) return false;
            return DistanceSquared(world,{transform.m[12],transform.m[13],transform.m[14]})<16.0f;
        }
    }
    return false;
}
}
