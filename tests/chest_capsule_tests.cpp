#include "chest_capsule.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
 const fp::TorsoPoints torso{{0,0,0},{-0.2f,0,0.6f},{0.2f,0,0.6f}};
 const fp::Vec3 head{0,0,0.75f};
 float d=0;
 Check(fp::ChestClearance(torso,head,{0,0,0.3f},d)&&d<0,"chest centre penetrates");
 Check(fp::ChestClearance(torso,head,{0,0.20f,0.3f},d)&&d>0,"eye outside chest");
 Check(fp::ChestClearance(torso,head,{0,0,-0.12f},d)&&d<0,"hip cap");
 Check(fp::ChestClearance(torso,head,{0,0.15f,0.6f},d)&&d<0,"shoulder cap");
 Check(fp::ChestClearance(torso,head,{0,0,0.70f},d)&&d>0,"head carve-out overrides chest cap");
 Check(fp::ChestClearance(torso,head,{0,0.14f,0},d)&&d>0,"hips narrower than shoulders");
 Check(!fp::ChestClearance({},head,{},d),"degenerate pose rejected");
 Check(!fp::ChestClearance(torso,head,{NAN,0,0},d),"nonfinite eye rejected");
 // Rigid transforms must not change the volume, including upside-down rolls.
 for(int degrees=0;degrees<=360;degrees+=15){
  auto q=fp::FromYawPitch(0.7f,degrees*fp::Pi/180);
  auto move=[&](fp::Vec3 p){return fp::Add(fp::Rotate(q,p),{10,20,30});};
  fp::TorsoPoints moved{move(torso.hips),move(torso.left),move(torso.right)};
  Check(fp::ChestClearance(moved,move(head),move({0,0.1f,0.3f}),d)&&d<0,"rotated chest retains penetration");
  Check(fp::ChestClearance(moved,move(head),move({0,0.25f,0.3f}),d)&&d>0,"rotated chest retains clearance");
 }
 const fp::Vec3 shallow{0,0.13f,0.3f};
 const auto corrected=fp::CorrectChest(torso,head,shallow,true);
 Check(fp::ChestClearance(torso,head,corrected,d)&&d>0,"small overlap corrected outside torso");
 Check(fp::Length(fp::Sub(corrected,shallow))<=0.08f,"correction bounded");
 Check(fp::DistanceSquared(fp::CorrectChest(torso,head,shallow,false),shallow)==0,"invalid pose never moves eye");
 Check(fp::Length(fp::Sub(fp::CorrectChest(torso,head,{0,0,0.3f},true),{0,0,0.3f}))<0.081f,"axis correction finite and bounded");
 for(int degrees=0;degrees<=360;degrees+=15){
  auto q=fp::FromYawPitch(0.7f,degrees*fp::Pi/180);
  auto move=[&](fp::Vec3 p){return fp::Add(fp::Rotate(q,p),{10,20,30});};
  fp::TorsoPoints moved{move(torso.hips),move(torso.left),move(torso.right)};
  auto output=fp::CorrectChest(moved,move(head),move(shallow),true);
  Check(fp::ChestClearance(moved,move(head),output,d)&&d>0,"rotated shallow overlap corrected");
 }
 fp::ChestGuard guard;
 Check(!guard.Scope(false,0.016f),"ordinary locomotion does not sample chest");
 Check(guard.Scope(true,0.016f),"manoeuvre enables guard");
 for(int i=0;i<60;++i)
  Check(!guard.Step(torso,head,{0,0.13f,0.3f},true,0.016f),"shallow grazing never takes camera ownership");
 Check(!guard.Step(torso,head,{0,0,0.3f},true,0.06f),"brief deep overlap does not fall back");
 Check(!guard.Step(torso,head,corrected,true,0.016f),"corrected eye resets penetration duration");
 Check(!guard.Step(torso,head,{0,0,0.3f},true,0.06f),"new overlap starts fresh timer");
 Check(guard.Step(torso,head,{0,0,0.3f},true,0.06f),"sustained unresolved overlap triggers fallback");
 Check(guard.Step(torso,head,{0,0.15f,0.3f},true,0.016f),"boundary hysteresis retains fallback");
 Check(guard.Step({},head,{},false,0.016f),"short gap preserves existing decision");
 Check(!guard.Step(torso,head,{0,0.20f,0.3f},true,0.016f),"clear margin releases latch");
 for(int i=0;i<60;++i)
  Check(!guard.Step({},head,{},false,0.016f),"missing torso cannot create fallback");
 Check(guard.Scope(false,0.1f)&&guard.Scope(false,0.1f),"recovery continues sampling");
 Check(!guard.Scope(false,0.1f)&&!guard.blocked,"scope expires and resets");
 fp::CameraFallback fallback;
 Check(fallback.Step(0,0,true,0.016f,false,true),"torso safety applies with automatic wall fallback off");
 for(int i=0;i<7;++i)Check(fallback.Step(0,0,true,0.1f,false,false),"fallback holds clear time");
 Check(!fallback.Step(0,0,true,0.1f,false,false),"fallback releases after stable clearance");
 std::puts("Chest geometry, carve-out, rotation, hysteresis, read gaps and fallback passed.");
}
