#pragma once
#include <cstdint>
namespace fp {
// Runtime trace for the supported executable: Visual+54 bits 1/2 become
// renderer+10 bits 19/20. In 21BD060, bit19 blocks the primary queue unless
// bit20 overrides it. Shadow and reflection queues have separate gates.
// These names describe observed submission behavior, not recovered engine names.
constexpr std::uint16_t MainViewMask=0x6;
inline std::uint16_t HideMainView(std::uint16_t flags){return (flags&~MainViewMask)|0x2;}
inline std::uint16_t RestoreMainView(std::uint16_t flags,std::uint16_t saved){return (flags&~MainViewMask)|(saved&MainViewMask);}
}
