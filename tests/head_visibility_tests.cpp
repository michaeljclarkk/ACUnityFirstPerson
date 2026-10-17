#include "head_visibility.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
void Check(bool v,const char* m){if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
int main(){
 // The same facial geometry can be referenced directly or through an outfit's
 // LOD selector. Both must be hidden while unrelated body assets stay visible.
 for(const auto id:{87057018236ULL,79129654705ULL,166173352493ULL,
                   166173352463ULL,211488955670ULL,211488955677ULL,
                   192344261121ULL,150741245608ULL,157366720363ULL})
  Check(fp::KnownHeadAsset(id),"direct and selector facial/head variants recognized");
 for(const auto id:{0ULL,79129654721ULL,166173352511ULL,192344261832ULL})
  Check(!fp::KnownHeadAsset(id),"unknown, ambiguous and entity assets excluded");

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
