#pragma once
#include <cstdint>
#include <initializer_list>
namespace fp {
struct GameBuild {
 const char* name;
 std::uint32_t timestamp,imageSize,checksum;
 std::uintptr_t camera,menu,skeleton,visual,refresh,refreshChildren;
 std::uintptr_t rayCtor,rayCast,rayDestroy,leapGetter,roll,leap;
 std::uintptr_t combat[12];
};
inline constexpr GameBuild Build150{
 "1.5.0",0x54DB5826,0x07825000,0x01D87E65,
 0x521AAD0,0x51EACE8,0x2E76630,0x309C7A0,0x21E9480,0x21E9550,
 0x25EC5A0,0x25FD7D0,0x10340,0x194D00,0x1A69680,0x1ADD2D0,
 {0x19BAA40,0x19AA430,0x19ABA10,0x19AB450,0x19AAD80,0x19AAA90,
  0x19A98D0,0x1999D30,0x19B39D0,0x19B3460,0x19AF160,0x19AEA00}};
// ACUFixes 1.5.1 port plus live-code verification of renderer/getter functions.
inline constexpr GameBuild Build151{
 "1.5.1",0x69945EEF,0x082A9000,0x023BF3F2,
 0x521CA40,0x51ECC68,0x2E77620,0x309DB90,0x21E97F0,0x21E98C0,
 0x25EC7C0,0x25FD8C0,0x10540,0x194A70,0x1A68AC0,0x1ADC860,
 {0x19BA590,0x19A9C00,0x19AB260,0x19AAC20,0x19AA550,0x19AA260,
  0x199C2D0,0x19B80A0,0x19B3140,0x19B2BD0,0x19AE9B0,0x19AE250}};
inline constexpr const GameBuild* SelectBuild(std::uint32_t timestamp,std::uint32_t size,std::uint32_t checksum) {
 for(const auto* build:{&Build150,&Build151})
  if(build->timestamp==timestamp&&build->imageSize==size&&build->checksum==checksum)return build;
 return nullptr;
}
}
