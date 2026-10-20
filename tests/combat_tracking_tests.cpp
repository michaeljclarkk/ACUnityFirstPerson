#include "parkour_tracking.h"
#include "body_motion.h"
#include <cstdio>
#include <cstdlib>
void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool Near(fp::Quat a,fp::Quat b){return fp::DistanceSquared(fp::Rotate(a,{0,1,0}),fp::Rotate(b,{0,1,0}))<0.00001f&&fp::DistanceSquared(fp::Rotate(a,{0,0,1}),fp::Rotate(b,{0,0,1}))<0.00001f;}
int main(){
 std::uint64_t previous=0;unsigned actions=0;
 for(const auto& clip:fp::CombatClips){Check(clip.handle>previous,"unique sorted combat handles");Check(fp::FindCombatClip(clip.handle)==&clip,"all combat handles searchable");previous=clip.handle;actions+=clip.follow?1:0;}
 Check(actions==1713&&std::size(fp::CombatClips)==2907,"reviewed catalogue coverage");
 for(const auto id:{32322695966ULL,32322695970ULL,49422516381ULL,49422516387ULL,50582703166ULL,54448417390ULL,106205824533ULL})
  Check(fp::CombatPlayback::Matches(id),"rear turn, focus turn, directional hit, dodge and roll coverage");
 for(const auto id:{29769213047ULL,31096111208ULL,0ULL,31011512705ULL})
  Check(!fp::CombatPlayback::Matches(id),"strafe, idle, unknown and parkour cannot lock combat view");
 constexpr std::uintptr_t player=0x3000;constexpr std::uint64_t turn=32322695966ULL,hit=50582703166ULL;
 fp::CombatPlayback p;
 p.Observe({player,turn,10},1,1000);
 Check(p.Active(player,1,1000)==turn,"combat sampled action activates");
 Check(!p.Active(player+1,1,1000)&&!p.Active(player,2,1000),"player and toggle epoch isolate combat");
 p.Observe({player,31096111208ULL,11},1,1016);
 Check(p.Active(player,1,1016)==turn,"idle layer does not cancel concurrent action");
 p.Observe({player,31096111208ULL,12},1,1032);
 Check(p.Active(player,1,1032)==turn,"unrelated cache tokens do not cancel a reaction");
 p.Observe({player,hit,10},1,1010);Check(p.Active(player,1,1033)==turn,"older timestamp cannot replace current reaction");
 p.Observe({player,hit,13},1,1048);Check(p.Active(player,1,1048)==hit,"chained hit reaction recognized");
 Check(!p.Active(player,1,1149)&&!p.Active(player,1,1000),"stopped or backwards clock expires tracking");
 p.Observe({player,hit,0xFFFFFFFF},2,2000);p.Observe({player,turn,0},2,2016);
 Check(p.Active(player,2,2016)==turn,"frame wrap accepts reaction turn");
 p.Observe({player,hit,0xFFFFFFFF},3,3000);p.Observe({player,31096111208ULL,0xFFFFFFFF},3,3050);
 Check(!p.Active(player,3,3101),"cache sentinel cannot latch combat camera");
 // Replay mixed context tokens from the user's gameplay log. Facial/idle
 // layers must not poison ordering for a subsequently sampled hurt/turn clip.
 p.Observe({player,31096111208ULL,1393476043},4,4000);
 p.Observe({player,67528838877ULL,0xFFFFFFFF},4,4016);
 Check(p.Active(player,4,4016)==67528838877ULL,"recorded rear hurt survives unrelated context token");
 p.Observe({player,133835168375ULL,743623600},4,4032);
 p.Observe({player,32322695978ULL,0xFFFFFFFF},4,4048);
 Check(p.Active(player,4,4048)==32322695978ULL,"recorded reaction turn survives facial context token");
 Check(!p.Active(player,4,4149),"mixed-token reaction still expires after sampling stops");
 // Opposing, discontinuous mouse movement must have zero influence after entry,
 // including pitch changes and crossing 180 degrees during a reaction turn.
 for(const float rate:{30.0f,60.0f,144.0f}) {
  fp::CombatView still,mouse;const auto entry=fp::FromYawPitch(0.4f,0.2f);
  still.Step(fp::FromYawPitch(0,0),entry,true,0);mouse.Step(fp::FromYawPitch(0,0),entry,true,0);
  for(int i=1;i<=static_cast<int>(rate*2);++i){
   const float angle=2*fp::Pi*i/(rate*2);const auto torso=fp::FromYawPitch(angle,0.25f*std::sin(angle));
   const auto native=fp::FromYawPitch(i%2?2.8f:-2.8f,i%3?0.9f:-0.9f);
   const auto expected=still.Step(torso,entry,true,1/rate);
   const auto output=mouse.Step(torso,native,true,1/rate);
   Check(Near(output,expected),"combat target independent of mouse yaw/pitch at all frame rates");
   const auto correction=fp::Multiply(output,fp::InverseUnit(native));
   Check(Near(fp::Multiply(correction,native),expected),"camera composition cancels native view exactly");
  }
  const auto resumed=fp::FromYawPitch(-1,0.5f);
  Check(Near(mouse.Step({},resumed,false,1/rate),resumed),"lost/disabled action releases view");
  Check(Near(mouse.Step(fp::FromYawPitch(2,0),resumed,true,0),resumed),"new action rebases with no entry snap");
 }
 std::puts("Combat mapping, playback expiry, and mouse priority tests passed.");
}
