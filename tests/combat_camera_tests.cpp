#include "combat_camera.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
void Check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<0.001f;}
struct Memory {
 std::array<unsigned char,0x10000> bytes{};
 template<class T> void put(std::uintptr_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof(v));}
 template<class T> bool operator()(std::uintptr_t p,T& v)const {
  if(p>bytes.size()||sizeof(v)>bytes.size()-p)return false;
  std::memcpy(&v,bytes.data()+p,sizeof(v));return true;
 }
};
int main() {
 Memory m;
 constexpr std::uintptr_t base=0x140000000,entity=0x100,components=0x400,anim=0x600,
 human=0x2000,holder=0x3000,receivers=0x4000,node=0x5000;
 m.put(entity+0x78,components);m.put(entity+0x82,std::uint16_t(1));
 m.put(entity+0x158,std::uint8_t(0));m.put(components,anim);m.put(anim+0x10,entity);
 m.put(anim+0xC58,human);m.put(human+0x710,holder);
 m.put(holder+0x1B0,receivers);m.put(holder+0x1BA,std::uint16_t(1));m.put(receivers+8,node);
 m.put(node+0xE0,base+0x19BAA40);
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Active,"combat parent detected");
 for(const auto address:fp::Build151.combat) {
  m.put(node+0xE0,base+address);
  Check(fp::ReadCombat(m,base,entity,nullptr,nullptr,fp::Build151)==fp::CombatState::Active,"1.5.1 combat callback recognized");
  Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Clear,"1.5.1 callback rejected by old profile");
 }
 m.put(node+0xE0,base+0x19AAD80);
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Active,"parry detected");
 bool leap=false;
 constexpr std::uintptr_t parents=0x6000,parent=0x7000,action=0x8000,table=0x9000;
 m.put(node+0x28,parents);m.put(node+0x32,std::uint16_t(1));m.put(parents,parent);
 m.put(parent+0x1F0,action);m.put(action+8,table);m.put(table+0x8B0,base+0x194D00);
 m.put(node+0xE0,base+0x1ADD2D0);
 Check(fp::ReadCombat(m,base,entity,nullptr,&leap)==fp::CombatState::Clear&&leap,"specific Leap of Faith action detected");
 m.put(node+0xE0,base+fp::Build151.leap);
 m.put(table+0x8B0,base+fp::Build151.leapGetter);
 Check(fp::ReadCombat(m,base,entity,nullptr,&leap,fp::Build151)==fp::CombatState::Clear&&leap,"1.5.1 leap state and action detected");
 m.put(table+0x8B0,base+fp::Build150.leapGetter);
 fp::ReadCombat(m,base,entity,nullptr,&leap,fp::Build151);
 Check(!leap,"mixed-version leap getter rejected");
 m.put(node+0xE0,base+fp::Build150.leap);
 m.put(table+0x8B0,base+0x123456);
 fp::ReadCombat(m,base,entity,nullptr,&leap);
 Check(!leap,"ordinary safe jumps excluded");
 m.put(table+0x8B0,base+0x194D00);m.put(node+0xE0,base+0x19DECC0);
 fp::ReadCombat(m,base,entity,nullptr,&leap);Check(!leap,"haystack ends leap even with stale action");
 m.put(node+0xE0,base+0x1ADD2D0);m.put(node+0x32,std::uint16_t(65));
 fp::ReadCombat(m,base,entity,nullptr,&leap);Check(!leap,"invalid parent stack rejected");
 bool groundRoll=false;
 m.put(node+0xE0,base+0x1A69680);
 Check(fp::ReadCombat(m,base,entity,&groundRoll)==fp::CombatState::Clear&&groundRoll,"ground roll recognized independently of combat");
 m.put(node+0xE0,base+0x1A93DF0);
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Clear,"hanging is not combat");
 m.put(node+0xE0,base+0x1A462F0);
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Clear,"assassination is not combat");
 m.put(holder+0x1BA,std::uint16_t(129));
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Unknown,"oversized list rejected");
 m.put(holder+0x1BA,std::uint16_t(1));m.put(receivers+8,std::uintptr_t(0xFFFFF));
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Unknown,"unreadable state rejected");
 m.put(receivers+8,node);m.put(anim+0x10,std::uintptr_t(0));
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Unknown,"foreign component rejected");
 m.put(anim+0x10,entity);m.put(entity+0x158,std::uint8_t(1));
 Check(fp::ReadCombat(m,base,entity)==fp::CombatState::Unknown,"invalid component index rejected");
 fp::CombatBlend blend;
 Check(blend.Step(fp::CombatState::Clear,true,0.016f)==0,"exploration unaffected");
 const float first=blend.Step(fp::CombatState::Active,true,0.016f);
 Check(first>0&&first<0.2f,"combat entry is gradual");
 for(int i=0;i<120;++i)blend.Step(fp::CombatState::Active,true,1/60.0f);
 Check(Near(blend.amount,1),"settles into combat framing");
 for(int i=0;i<12;++i)blend.Step(fp::CombatState::Unknown,true,1/60.0f);
 Check(blend.amount>0.99f,"brief state gaps hold view");
 for(int i=0;i<180;++i)blend.Step(fp::CombatState::Unknown,true,1/60.0f);
 Check(blend.amount<0.001f,"tracking failure cannot latch combat framing");
 fp::CombatBlend slow,fast;
 for(int i=0;i<30;++i)slow.Step(fp::CombatState::Active,true,1/30.0f);
 for(int i=0;i<144;++i)fast.Step(fp::CombatState::Active,true,1/144.0f);
 Check(Near(slow.amount,fast.amount),"frame rate independent transition");
 for(int i=0;i<180;++i)fast.Step(fp::CombatState::Active,false,1/60.0f);
 Check(fast.amount<0.001f,"disabled option returns to exploration even in combat");
 const float before=slow.amount;
 Check(slow.Step(fp::CombatState::Active,true,std::numeric_limits<float>::quiet_NaN())==before,"invalid time cannot corrupt blend");
 Check(Near(fp::CombatFov(90,80,1),90),"wide player FOV is never narrowed");
 Check(Near(fp::CombatFov(70,80,0.5f),75),"FOV blends with position");
 fp::Vec3 normal{},combat{};
 const auto q=fp::FromYawPitch(0,0);
 Check(fp::LevelEyePosition({0,0,1.5f},q,0.12f,0.16f,normal),"normal eye");
 Check(fp::LevelEyePosition({0,0,1.5f},q,-0.06f,0.02f,combat),"combat eye");
 Check(Near(normal.z-combat.z,0.18f)&&Near(normal.y-combat.y,0.14f),"default combat view is 18cm lower and 14cm back");
 // A hand below/in front of the head gains space above the bottom screen edge.
 const fp::Vec3 hand{0,0.6f,1.1f};
 const float oldScreen=(hand.z-normal.z)/(hand.y-normal.y)/std::tan(70*fp::Pi/360);
 const float newScreen=(hand.z-combat.z)/(hand.y-combat.y)/std::tan(80*fp::Pi/360);
 Check(oldScreen < -1 && newScreen > -1,"representative low hand moves inside combat view");
 std::puts("Combat state detection, transitions, dropout, FOV and hand framing passed.");
}
