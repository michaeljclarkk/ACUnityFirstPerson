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

 for(const auto id:{161784706208ULL,161784706209ULL,161784706233ULL,
                   161784706238ULL,161784706244ULL,161784706249ULL,
                   161784706267ULL,161784706272ULL,161784706290ULL,
                   161784706295ULL,120094244881ULL,147798842982ULL})
  Check(fp::KnownHeadAsset(id),"raised/lowered hoods and detail meshes use head visibility path");
 for(const auto id:{153024721356ULL,153024721357ULL,153024721358ULL})
  Check(!fp::KnownHeadAsset(id),"McFarlane dress, body and belt stay visible");
 // Binary lookup requires strict ordering; duplicates can hide catalogue errors.
 for(std::size_t i=1;i<std::size(fp::HoodAssets);++i)
  Check(fp::HoodAssets[i-1]<fp::HoodAssets[i],"hood catalogue sorted and unique");
 for(const auto id:{158617568238ULL,161893649233ULL,151641737506ULL,
                   151641737507ULL,151641748093ULL,151641748094ULL,
                   153992919196ULL,163859797422ULL,264721071237ULL,
                   264721071265ULL,264721070936ULL,264721071094ULL,
                   264721071467ULL,264721071493ULL})
  Check(fp::KnownHeadAsset(id),"standard, legacy and Dead Kings hoods use shadow-preserving path");
 for(const auto id:{69571753756ULL,69571753758ULL,153024721357ULL,0ULL})
  Check(!fp::KnownHoodAsset(id),"NPC hoods, body meshes and unknown resources excluded");
 Check(fp::KnownHoodAsset(fp::HoodAssets[0])&&
       fp::KnownHoodAsset(fp::HoodAssets[std::size(fp::HoodAssets)-1]),"hood lookup includes both endpoints");
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
