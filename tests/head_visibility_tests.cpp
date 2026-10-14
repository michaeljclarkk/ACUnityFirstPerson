#include "head_visibility.h"
#include <cstdio>
#include <cstdlib>
void Check(bool v,const char* m){if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
int main(){
 for(unsigned value=0;value<65536;++value){
  const auto flags=static_cast<std::uint16_t>(value);
  const auto hidden=fp::HideMainView(flags);
  Check((hidden&6)==2,"primary submission suppressed without override");
  Check((hidden&~6)==(flags&~6),"global hidden/shadow/reflection and other flags preserved");
  Check(fp::RestoreMainView(hidden,flags)==flags,"original view flags restored exactly");
  const auto changed=static_cast<std::uint16_t>(hidden^0x8039);
  const auto restored=fp::RestoreMainView(changed,flags);
  Check((restored&~6)==(changed&~6),"unrelated engine changes survive restoration");
 }
 std::puts("Exhaustive view-flag preservation and restoration passed.");
}
