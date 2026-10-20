#pragma once
#include "parkour_clips.h"
#include "combat_clips.h"
#include <cmath>
namespace fp {
struct PoseClipSample {
 std::uintptr_t entity=0;
 std::uint64_t handle=0;
 std::uint32_t frame=0;
};
// Pose sampling callback: 1.5.0 RVA BCC20, 1.5.1 RVA BC640.
// Both versions retain this layout. The context's graph at +18 owns
// animCpnt (+6A0); its evaluation at +20 holds providers (+260, count +26A).
// Node +64 selects a provider; provider +28 is the actual shared animation
// block (handle +10), including dynamically selected/replaced animations.
// Reference: BCC3A..BCC57, BCCD3..BCCDE, 3E0EF..3E114 in saved 1.5.0 code.
template<class Reader>
bool ReadPoseClip(Reader read,std::uintptr_t node,std::uintptr_t context,
                  std::uintptr_t expectedPlayer,PoseClipSample& out) {
 out={};
 if(!node||!context||!expectedPlayer)return false;
 std::uintptr_t graph=0,anim=0,entity=0,evaluation=0,providers=0,provider=0,shared=0,object=0;
 std::uint16_t index=0,count=0;std::int32_t flags=0;PoseClipSample sample{};
 if(!read(context+0x18,graph)||!graph||!read(graph+0x6A0,anim)||!anim||
    !read(anim+0x10,entity)||entity!=expectedPlayer||
    !read(context+0x20,evaluation)||!evaluation||
    !read(node+0x64,index)||!read(evaluation+0x260,providers)||!providers||
    !read(evaluation+0x26A,count)||!count||count>1024||index>=count||
    !read(providers+8*index,provider)||!provider||
    !read(provider+0x28,shared)||!shared||!read(shared,object)||!object||
    !read(shared+0xC,flags)||flags>=0||
    !read(shared+0x10,sample.handle)||!sample.handle||
    !read(context+0x44,sample.frame))return false;
 std::uintptr_t current=0;
 if(!read(provider+0x28,current)||current!=shared||
    !read(anim+0xD0,current)||current!=graph)return false;
 sample.entity=entity;out=sample;return true;
}
// Bounded session capture of distinct player animation handles. Drained/logged
// by the GUI thread; callbacks only write fixed storage under playbackLock.
struct ParkourCapture {
 struct Entry {std::uint64_t handle=0;std::uint32_t token=0;};
 Entry entries[1024]{};
 unsigned count=0,reported=0;
 void Observe(const PoseClipSample& sample) {
  if(!sample.handle)return;
  for(unsigned i=0;i<count;++i)if(entries[i].handle==sample.handle)return;
  if(count<1024)entries[count++]={sample.handle,sample.frame};
 }
 bool Pop(Entry& entry) {
  if(reported==count)return false;
  entry=entries[reported++];return true;
 }
};
// Protected by the caller's short lock. No allocation/logging in the callback.
template<bool Combat> struct ClipPlayback {
 static bool Matches(std::uint64_t id) {
  if constexpr(Combat){const auto* clip=FindCombatClip(id);return clip&&clip->follow;}
  else return FindParkourClip(id)!=nullptr;
 }
 std::uintptr_t entity=0;unsigned epoch=0;
 std::uint32_t latestFrame=0,matchedFrame=0;
 std::uint64_t latestSampleAt=0,matchedAt=0,handle=0,samples=0,matches=0,lateSamples=0,lastHandle=0;
 std::uint32_t lastToken=0;
 bool initialized=false;
 void Observe(const PoseClipSample& sample,unsigned generation,std::uint64_t now) {
  if(!initialized||entity!=sample.entity||epoch!=generation){*this={};entity=sample.entity;epoch=generation;latestFrame=sample.frame;initialized=true;}
  ++samples;lastHandle=sample.handle;lastToken=sample.frame;
  if(Matches(sample.handle))++matches;
  // Combat contexts mix cache sentinels and unrelated tokens (observed:
  // FFFFFFFF, 743623600, 1393476043). They cannot order playback frames.
  if constexpr(Combat) {
   if(now<latestSampleAt){++lateSamples;return;}
   latestSampleAt=now;
  } else {
   const auto advance=static_cast<std::int32_t>(sample.frame-latestFrame);
   if(advance<0){++lateSamples;return;}
  }
  latestFrame=sample.frame;
  if(Matches(sample.handle)){handle=sample.handle;matchedFrame=sample.frame;matchedAt=now;}
 }
 std::uint64_t Active(std::uintptr_t player,unsigned generation,std::uint64_t now)const {
  return initialized&&entity==player&&epoch==generation&&handle&&now>=matchedAt&&
   now-matchedAt<=100&&(Combat||static_cast<std::uint32_t>(latestFrame-matchedFrame)<=1)?handle:0;
 }
};
using ParkourPlayback=ClipPlayback<false>;
using CombatPlayback=ClipPlayback<true>;
}
