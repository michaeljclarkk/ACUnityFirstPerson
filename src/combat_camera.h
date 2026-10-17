#pragma once
#include "camera_comfort.h"
#include "game_build.h"
#include <cstdint>
namespace fp {
// ACU 1.5.0 Enter callbacks identified in the reference checkout's
// ParkourDebugging/LoggingTheHumanStates.cpp; no game state is written.
inline bool CombatEnter(std::uintptr_t rva,const GameBuild& build=Build150) {
 for(const auto address:build.combat)if(rva==address)return true;
 return false;
}
// Combat-roll Enter callbacks: gameplay verified on 1.5.0, code matched on 1.5.1.
// Keep this separate from general combat so parries/hit reactions cannot enable it.
inline bool RollEnter(std::uintptr_t rva,const GameBuild& build=Build150) {
 return rva==build.roll||(build.combatRoll&&rva==build.combatRoll);
}
// Captured during the user's Leap of Faith: safe-airborne node's action
// type getter is RVA 194D00 (mov eax,70h; ret). No game function is called.
template<class Reader>
bool LeapAction(Reader read,std::uintptr_t base,std::uintptr_t node,const GameBuild& build=Build150) {
 std::uintptr_t parents=0,parent=0,action=0,table=0,getter=0;
 std::uint16_t count=0;
 return read(node+0x28,parents)&&parents&&read(node+0x32,count)&&count>0&&count<=64&&
  read(parents,parent)&&parent&&read(parent+0x1F0,action)&&action&&
  read(action+8,table)&&table&&read(table+0x8B0,getter)&&getter==base+build.leapGetter;
}
enum class CombatState { Unknown, Clear, Active };
template<class Reader>
CombatState ReadCombat(Reader read,std::uintptr_t base,std::uintptr_t entity,bool* groundRoll=nullptr,bool* leap=nullptr,const GameBuild& build=Build150) {
 if(groundRoll)*groundRoll=false;
 if(leap)*leap=false;
 std::uintptr_t components=0,anim=0,owner=0,human=0,holder=0,receivers=0;
 std::uint8_t index=0;
 std::uint16_t count=0,receiverCount=0;
 if(!read(entity+0x78,components)||!components||!read(entity+0x82,count)||count>128||
    !read(entity+0x158,index)||index>=count||!read(components+8*index,anim)||!anim||
    !read(anim+0x10,owner)||owner!=entity||
    !read(anim+0xC58,human)||!human||!read(human+0x710,holder)||!holder||
    !read(holder+0x1B0,receivers)||!read(holder+0x1BA,receiverCount)||
    receiverCount>128||(receiverCount&&!receivers))return CombatState::Unknown;
 bool foundCombat=false,foundRoll=false,foundLeap=false;
 for(std::uint16_t i=0;i<receiverCount;++i) {
  std::uintptr_t node=0,enter=0;
  if(!read(receivers+16*i+8,node)||!node||!read(node+0xE0,enter))return CombatState::Unknown;
  if(enter>=base&&CombatEnter(enter-base,build))foundCombat=true;
  if(enter>=base&&RollEnter(enter-base,build))foundRoll=true;
  if(enter==base+build.leap&&LeapAction(read,base,node,build))foundLeap=true;
 }
 // Reject a replaced list instead of using a partial transition snapshot.
 std::uintptr_t current=0;std::uint16_t currentCount=0;
 if(!read(holder+0x1B0,current)||current!=receivers||
    !read(holder+0x1BA,currentCount)||currentCount!=receiverCount)return CombatState::Unknown;
 if(groundRoll)*groundRoll=foundRoll;
 if(leap)*leap=foundLeap;
 return foundCombat?CombatState::Active:CombatState::Clear;
}
struct CombatBlend {
 float amount=0,hold=0;
 float Step(CombatState state,bool enabled,float dt) {
  dt=std::isfinite(dt)?std::clamp(dt,0.0f,0.1f):0;
  // Brief receiver transitions do not pump the FOV during consecutive attacks.
  if(enabled&&state==CombatState::Active)hold=0.45f;
  else hold=enabled?std::max(0.0f,hold-dt):0;
  const float target=enabled&&hold>0?1.0f:0.0f;
  const float halfLife=target>amount?0.10f:0.18f;
  amount+=(target-amount)*(1-std::exp(-0.69314718056f*dt/halfLife));
  return amount;
 }
};
inline float CombatLerp(float normal,float combat,float amount) {
 return normal+(combat-normal)*std::clamp(amount,0.0f,1.0f);
}
// Widen only: respect a player's already wider FOV. Both values in radians.
inline float CombatFov(float normal,float combat,float amount) {
 return CombatLerp(normal,std::max(normal,combat),amount);
}
}

