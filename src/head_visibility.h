#pragma once
#include <cstdint>
#include "hood_assets.h"
namespace fp {
// Explicitly named Arno head resources from installed forge metadata. Include
// both direct meshes and LOD selectors: outfit variants can reference either.
// Do not infer neighbouring handles or hide whole outfit/body components.
inline bool KnownHeadAsset(std::uint64_t asset) {
 if(KnownHoodAsset(asset))return true;
 constexpr std::uint64_t ids[]={

  79129654667ULL, // CN_U_FR_Arno_Head_LOD0
  79129654705ULL, // CN_U_FR_Arno_Eyelash_LOD0
  87057018236ULL, // CN_U_FR_Arno_EyeTeeth_LOD0
  97923931425ULL, // CN_U_Arno_Hair_D_LOD0
  99766053298ULL, // CN_U_Arno_Hair_D_LOD1
  99877565340ULL, // CN_U_Arno_Hair_D_LOD0
  99877565356ULL, // CN_U_Arno_Hair_D_LOD1
  106022247923ULL, // CN_U_Arno_Hair_D_LOD3
  106875813386ULL, // CN_U_Arno_Hair_D_LOD2
  157366720363ULL, // CN_U_Arno_HairPonytail_LOD0
  157366720369ULL, // CN_U_Arno_HairPonytail_LOD1
  157366720423ULL, // CN_U_Arno_HairPonytail_LOD0
  157366720427ULL, // CN_U_Arno_HairPonytail_LOD1
  157366720431ULL, // CN_U_Arno_HairPonytail_LOD2
  160518756451ULL, // CN_U_Arno_Prisoner_Hair_Beard_LOD0
  166173352463ULL, // CN_U_Arno_Eyelash
  166173352493ULL, // CN_U_Arno_EyeTeeth
  192344261111ULL, // CN_U_FR_Arno_Head_nextday_scar_LOD0
  192344261116ULL, // CN_U_FR_Arno_Head_cut_LOD0
  192344261121ULL, // CN_U_FR_Arno_Head_scar_LOD0
  211488955670ULL, // CN_U_FR_Arno_Head
  211488955677ULL, // CN_U_FR_Arno_Head_LOD1
  211488955694ULL, // CN_U_FR_Arno_Head_LOD2
  211488955699ULL, // CN_U_FR_Arno_Head_LOD3
 };
 for(auto id:ids)if(id==asset)return true;
 return false;
}
// Runtime trace for the supported executable: Visual+54 bits 1/2 become
// renderer+10 bits 19/20. In 21BD060, bit19 blocks the primary queue unless
// bit20 overrides it. Shadow and reflection queues have separate gates.
// These names describe observed submission behavior, not recovered engine names.
constexpr std::uint16_t MainViewMask=0x6;
inline std::uint16_t HideMainView(std::uint16_t flags){return (flags&~MainViewMask)|0x2;}
inline std::uint16_t RestoreMainView(std::uint16_t flags,std::uint16_t saved){return (flags&~MainViewMask)|(saved&MainViewMask);}
}
