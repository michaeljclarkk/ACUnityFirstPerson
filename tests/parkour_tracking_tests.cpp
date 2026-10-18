#include "parkour_tracking.h"
#include "body_motion.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
void Check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
struct Memory {
 std::array<unsigned char,0x10000> bytes{};
 template<class T> void put(std::uintptr_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof(v));}
 template<class T> bool operator()(std::uintptr_t p,T& v)const {
  if(p>bytes.size()||sizeof(v)>bytes.size()-p)return false;
  std::memcpy(&v,bytes.data()+p,sizeof(v));return true;
 }
};
bool Near(fp::Quat a,fp::Quat b){return fp::DistanceSquared(fp::Rotate(a,{0,1,0}),fp::Rotate(b,{0,1,0}))<0.001f&&fp::DistanceSquared(fp::Rotate(a,{0,0,1}),fp::Rotate(b,{0,0,1}))<0.001f;}
int main(){
 std::uint64_t previous=0;
 for(const auto& clip:fp::ParkourClips){Check(clip.handle>previous,"catalogue unique and sorted");Check(fp::FindParkourClip(clip.handle)==&clip,"every catalogue entry searchable");previous=clip.handle;}
 Check(!fp::FindParkourClip(0)&&!fp::FindParkourClip(106205824533ULL),"ordinary/ combat clips excluded");
 // Actual 1.5.0 playback captured during the user's descent test.
 for(const auto id:{73293114258ULL,73293114280ULL,73293114302ULL,
                   136228815296ULL,136228815311ULL,11122296262ULL,
                   36167538121ULL,87698354887ULL,36167533242ULL,
                   99964888056ULL,99964887982ULL,192626060514ULL,
                   192309869443ULL,69136278309ULL})
  Check(fp::FindParkourClip(id)!=nullptr,"captured descent clips have camera coverage");
 for(const auto id:{67097403506ULL,150699227138ULL,34453365708ULL,35208239518ULL})
  Check(!fp::FindParkourClip(id),"ordinary running, navigation turns and straight jumps remain excluded");
 constexpr std::uint64_t handle=31011512705ULL;
 Check(fp::FindParkourClip(handle)!=nullptr,"wall eject catalogue identity");
 Memory m;constexpr std::uintptr_t node=0x100,context=0x200,graph=0x1000,anim=0x2000,player=0x3000,eval=0x4000,providers=0x5000,provider=0x6000,shared=0x7000,object=0x8000;
 m.put(context+0x18,graph);m.put(graph+0x6A0,anim);m.put(anim+0x10,player);m.put(anim+0xD0,graph);
 m.put(context+0x20,eval);m.put(node+0x64,std::uint16_t(0));m.put(eval+0x260,providers);m.put(eval+0x26A,std::uint16_t(1));m.put(providers,provider);
 m.put(provider+0x28,shared);m.put(shared,object);m.put(shared+0xC,std::int32_t(0x80000001));m.put(shared+0x10,handle);m.put(context+0x44,std::uint32_t(10));
 fp::PoseClipSample sample{};
 Check(fp::ReadPoseClip(m,node,context,player,sample)&&sample.handle==handle&&sample.frame==10,"actual sampled provider reference read");
 Check(!fp::ReadPoseClip(m,node,context,player+1,sample)&&sample.handle==0,"NPC pose rejected and output cleared");
 m.put(node+0x64,std::uint16_t(1));Check(!fp::ReadPoseClip(m,node,context,player,sample),"out-of-bounds provider rejected");m.put(node+0x64,std::uint16_t(0));
 m.put(eval+0x26A,std::uint16_t(1025));Check(!fp::ReadPoseClip(m,node,context,player,sample),"excessive count rejected");m.put(eval+0x26A,std::uint16_t(1));
 m.put(shared+0xC,std::int32_t(1));Check(!fp::ReadPoseClip(m,node,context,player,sample),"unloaded shared block rejected");m.put(shared+0xC,std::int32_t(0x80000001));
 m.put(anim+0xD0,graph+1);Check(!fp::ReadPoseClip(m,node,context,player,sample),"replaced graph rejected");m.put(anim+0xD0,graph);
 auto replaced=[&](std::uintptr_t a,auto& v){if(a==provider+0x28){static int reads=0;if(++reads==2)return false;}return m(a,v);};
 Check(!fp::ReadPoseClip(replaced,node,context,player,sample),"provider replacement/read failure rejected");
 fp::ParkourCapture capture;fp::ParkourCapture::Entry captured{};
 capture.Observe({player,999,10});capture.Observe({player,999,11});
 Check(capture.count==1&&capture.Pop(captured)&&captured.handle==999&&captured.token==10&&!capture.Pop(captured),"unmapped clips captured once and drain completely");
 for(unsigned i=1;i<=1100;++i)capture.Observe({player,10000+i,i});
 Check(capture.count==1024,"session capture remains bounded");
 fp::ParkourPlayback playback;
 playback.Observe({player,handle,10},1,1000);
 Check(playback.Active(player,1,1000)==handle,"sample enables matching player");
 Check(!playback.Active(player+1,1,1000)&&!playback.Active(player,2,1000),"player and camera epoch isolate observations");
 playback.Observe({player,999,11},1,1016);Check(playback.Active(player,1,1016)==handle,"one-frame evaluation ordering tolerance");
 playback.Observe({player,999,12},1,1032);Check(!playback.Active(player,1,1032),"ordinary playback expires old spin after one frame");
 playback.Observe({player,handle,10},1,1033);Check(!playback.Active(player,1,1033),"late worker cannot revive old spin");
 playback.Observe({player,handle,13},1,1048);Check(!playback.Active(player,1,1149),"stopped sampling expires without new callbacks");Check(!playback.Active(player,1,1000),"backwards clock rejected");
 playback.Observe({player,999,14},2,1064);Check(!playback.Active(player,2,1064),"toggle epoch cannot inherit old clip");
 playback.Observe({player,handle,0xFFFFFFFF},3,2000);playback.Observe({player,999,0},3,2016);Check(playback.Active(player,3,2016)==handle,"frame wrap preserves one-frame grace");
 playback.Observe({player,999,1},3,2032);Check(!playback.Active(player,3,2032),"frame wrap expires old spin");
 // The observed 1.5.0 context token is UINT32_MAX, a cache sentinel,
 // not an advancing frame counter. Wall-clock expiry must still end tracking.
 playback.Observe({player,73293114258ULL,0xFFFFFFFF},4,3000);
 playback.Observe({player,999,0xFFFFFFFF},4,3050);
 Check(playback.Active(player,4,3050)==73293114258ULL,"descent recognized with observed cache sentinel");
 Check(!playback.Active(player,4,3101),"constant cache token cannot leave descent tracking stuck");
 // World-space root turns and native yaw must not add together. Start looking
 // to one side, simulate native camera also following the root through 360.
 fp::ParkourView follow;const auto entry=fp::FromYawPitch(0.7f,0.3f);
 fp::BodyMotion motion;
 fp::Quat last=entry;
 for(int degree=0;degree<=360;++degree){
  const float angle=degree*fp::Pi/180;const auto torso=fp::FromYawPitch(angle,0);
  const auto native=fp::FromYawPitch(angle+0.7f,0.3f);
  const auto target=follow.Step(torso,native,true);
  Check(Near(target,fp::Multiply(torso,entry)),"entry aim preserved and root spin counted once");
  const auto correction=motion.Step(target,true,angle,1,1/120.0f,&native);
  const auto output=fp::Multiply(correction,native);
  Check(Near(output,target),"native yaw does not double spin");
  Check(fp::DistanceSquared(fp::Rotate(last,{0,1,0}),fp::Rotate(output,{0,1,0}))<0.002f,"360 spin crosses 180 without discontinuity");last=output;
 }
 // A fixed native view must also follow full world-space yaw/pitch/roll spins.
 for(int axis=0;axis<3;++axis){
  fp::ParkourView spin;fp::BodyMotion rotation;fp::Quat prior=entry;
  for(int degree=0;degree<=360;++degree){
   const float angle=degree*fp::Pi/180;
   const fp::Quat torso{axis==0?std::sin(angle/2):0,axis==1?std::sin(angle/2):0,axis==2?std::sin(angle/2):0,std::cos(angle/2)};
   const auto target=spin.Step(torso,entry,true);
   const auto output=fp::Multiply(rotation.Step(target,true,angle,1,1/120.0f,&entry),entry);
   Check(fp::DistanceSquared(fp::Rotate(output,{0,1,0}),fp::Rotate(target,{0,1,0}))<0.002f,"fixed native view follows all spin axes");
   Check(fp::DistanceSquared(fp::Rotate(output,{0,1,0}),fp::Rotate(prior,{0,1,0}))<0.002f,"fixed-view spin has no half-turn snap");prior=output;
  }
 }
 follow.Step({},entry,false);const auto other=fp::FromYawPitch(-1,0.1f);
 Check(Near(follow.Step(fp::FromYawPitch(2,0),other,true),other),"new action rebases entry view after recovery");
 for(int i=0;i<120;++i)motion.Step({},false,0,0,1/60.0f);
 Check(Near(motion.applied,{0,0,0,1}),"normal camera restored after sampling ends");
 std::puts("Parkour catalogue, guarded provider reads, playback expiry, and full-turn camera tests passed.");
}
