#include "plugin_api.h"
#include "imgui.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
void Check(bool ok,const char* msg) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); }
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"DLL argument supplied");
    HMODULE dll=LoadLibraryW(argv[1]);
    Check(dll!=nullptr,"DLL loads with static runtime and no missing dependencies");
    using Start=bool(*)(loader::Interface&,loader::Info&);
    auto start=reinterpret_cast<Start>(GetProcAddress(dll,"ACUPluginStart"));
    Check(start!=nullptr,"loader export present");
    alignas(8) std::uint64_t api[4]{loader::kApi-1,0,0,0};
    struct Guarded { loader::Info info{}; std::uint64_t sentinel=0xABCDEFull; } output;
    Check(!start(*reinterpret_cast<loader::Interface*>(api),output.info),"older loader rejected");
    api[0]=loader::kApi;
    Check(start(*reinterpret_cast<loader::Interface*>(api),output.info),"0.9.2 loader accepted");
    api[0]=0x00090400;
    Check(start(*reinterpret_cast<loader::Interface*>(api),output.info),"0.9.4 loader accepted");
    Check(output.sentinel==0xABCDEFull,"ABI output stays within caller allocation");
    Check(output.info.apiVersion==loader::kApi && output.info.onReady && output.info.onFrame,"required callbacks supplied");
        Check(output.info.onMenu!=nullptr && output.info.onUnpacked==nullptr,"menu callback supplied");
    ImGuiContext* context=ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1280,900);io.DeltaTime=1.0f/60;
    unsigned char* pixels=nullptr;int width=0,height=0;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    loader::Gui gui{context,nullptr,nullptr,nullptr};
    ImGui::GetAllocatorFunctions(&gui.allocate,&gui.release,&gui.userData);
    for(int frame=0;frame<2;++frame) {
        ImGui::NewFrame();ImGui::Begin("Plugin settings");
        output.info.onMenu(gui);ImGui::End();ImGui::Render();
    }
    Check(ImGui::GetDrawData()->TotalVtxCount>0,"plugin menu renders in shared context");
    ImGui::DestroyContext(context);
    Check(!output.info.onReady(*reinterpret_cast<loader::Interface*>(api)),"non-game process rejected before hooking");
    Check(FreeLibrary(dll)!=0,"rejected plugin can unload");
    std::puts("DLL export, ABI compatibility, dependencies, and non-game rejection passed.");
}
