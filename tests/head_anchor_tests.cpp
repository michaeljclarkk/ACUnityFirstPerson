#include "head_anchor.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
void Check(bool b,const char* msg){if(!b){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
struct Memory {
 std::array<unsigned char,0x10000> bytes{};
 template<class T> void put(std::uintptr_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof(v));}
 template<class T> bool operator()(std::uintptr_t p,T& v)const{
  if(p>bytes.size() || sizeof(v)>bytes.size()-p)return false;
  std::memcpy(&v,bytes.data()+p,sizeof(v));return true;
 }
};
int main(){
 Memory m;
 constexpr std::uintptr_t base=0x140000000, entity=0x100, components=0x400, skeleton=0x600, info=0x1000, mapping=0x2000, buffer=0x4000, poses=0x5000;
 fp::Transform t{{1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1}};
 m.put(entity+0x20,t);m.put(entity+0x78,components);m.put(entity+0x82,std::uint16_t(1));
 m.put(components,skeleton);m.put(skeleton,base+0x2E76630);m.put(skeleton+0x10,entity);
 m.put(skeleton+0x270,info);m.put(info+0x38,mapping);m.put(info+0x42,std::uint16_t(451));
 m.put(skeleton+0x288,buffer);m.put(buffer+0x178,poses);
 m.put(mapping+8*450,std::uint32_t(0x07C159A2));m.put(mapping+8*450+4,std::uint16_t(156));
 m.put(poses+0x20*156,fp::Vec3{0,0.5f,1.4f});
 fp::Vec3 head{};
 Check(fp::ReadHead(m,base,entity,head),"head found in a 451-entry mapping");
 Check(std::abs(head.y-20.5f)<0.001f && std::abs(head.z-31.4f)<0.001f,"animation translation follows body");
 m.put(poses+0x20*156,fp::Vec3{0,0.9f,1.1f});
 Check(fp::ReadHead(m,base,entity,head)&&std::abs(head.y-20.9f)<0.001f&&std::abs(head.z-31.1f)<0.001f,"sprint lean and crouch height followed");
 t.m[0]=0;t.m[1]=1;t.m[4]=-1;t.m[5]=0;m.put(entity+0x20,t);
 Check(fp::ReadHead(m,base,entity,head)&&std::abs(head.x-9.1f)<0.001f,"player rotation maps local pose into world");
 m.put(mapping+8*450+4,std::uint16_t(65535));
 Check(!fp::ReadHead(m,base,entity,head),"invalid pose index rejected");
 m.put(mapping+8*450+4,std::uint16_t(156));m.put(poses+0x20*156,fp::Vec3{0,0,std::numeric_limits<float>::quiet_NaN()});
 Check(!fp::ReadHead(m,base,entity,head),"invalid pose rejected");
 m.put(poses+0x20*156,fp::Vec3{0,0,1.5f});m.put(skeleton+0x10,std::uintptr_t(0x999));
 Check(!fp::ReadHead(m,base,entity,head),"foreign skeleton rejected");
 m.put(skeleton+0x10,entity);m.put(buffer+0x178,std::uintptr_t(0xFFFFF));
 Check(!fp::ReadHead(m,base,entity,head),"unreadable pose rejected");
 std::puts("Animated head reader: transforms, sprint/crouch offsets, and invalid memory checks passed.");
}
