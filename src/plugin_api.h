#pragma once
#include <Windows.h>
#include <cstddef>
#include <cstdint>
// Minimal independently declared ABI for ACUFixes Plugin Loader 0.9.2.
// Field positions and calling signatures are interoperability requirements.
// No ACUFixes implementation or headers are compiled into this project.
struct ImGuiContext;
namespace loader {
struct Interface;
struct Gui {
    ImGuiContext* context;
    void* (*allocate)(size_t,void*);
    void (*release)(void*,void*);
    void* userData;
};
static_assert(sizeof(Gui)==0x20);
struct Info {
    std::uint64_t apiVersion;
    std::uint64_t pluginVersion;
    bool (*onReady)(Interface&);
    void (*onMenu)(Gui&);
    void (*onFrame)(Gui&);
    void (*onCompatible)(Interface&);
    void (*onUnpacked)();
};
static_assert(sizeof(Info) == 0x38);
static_assert(offsetof(Info, onFrame) == 0x20);
inline constexpr std::uint64_t kApi = 0x00090200;
}
