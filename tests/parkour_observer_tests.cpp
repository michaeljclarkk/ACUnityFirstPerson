#include <windows.h>
bool denyLocalProtection=false;
unsigned deniedLocalCalls=0;
BOOL WINAPI TestVirtualProtect(LPVOID address,SIZE_T size,DWORD protection,PDWORD previous) {
 if(denyLocalProtection){++deniedLocalCalls;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
 return VirtualProtect(address,size,protection,previous);
}
#define VirtualProtect TestVirtualProtect
#include "../src/plugin.cpp"
#undef VirtualProtect
void CheckObserver(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
std::uintptr_t forwarded[3]{};
std::uintptr_t __fastcall FakeSample(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c){forwarded[0]=a;forwarded[1]=b;forwarded[2]=c;return 12345;}
int main(){
 for(bool deny:{false,true}) {
 for(const auto* build:{&fp::Build150,&fp::Build151}) {
 const auto poseRva=build->parkourPose;
 // Exercise real guarded installation on a synthetic, isolated PE image.
 auto* image=static_cast<unsigned char*>(VirtualAlloc(nullptr,0xC0000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 CheckObserver(image!=nullptr,"allocate synthetic image");base=reinterpret_cast<std::uintptr_t>(image);
 auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_lfanew=0x100;
 auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+0x100);nt->FileHeader.NumberOfSections=2;nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
 auto* sections=reinterpret_cast<IMAGE_SECTION_HEADER*>(image+0x100+24+sizeof(IMAGE_OPTIONAL_HEADER64));
 memcpy(sections[0].Name,".text",5);sections[0].VirtualAddress=0xB0000;sections[0].Misc.VirtualSize=0x10000;
 memcpy(sections[1].Name,".rdata",6);sections[1].VirtualAddress=0x2000;sections[1].Misc.VirtualSize=0x1000;
 constexpr unsigned char code[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41,0x56,0x48,0x83,0xEC,0x30,0x0F,0xB7,0x49,0x64,0x4D,0x8B,0x48,0x20,0x49,0x8B,0xD8,0x49,0x8B,0x81,0x60,0x02,0x00,0x00,0x45,0x8B,0x40,0x44,0xC1,0xE1,0x03,0x48,0x8B,0x34,0x01,0x48,0x8B,0xFA,0x0F,0xB6,0x53,0x12,0x48,0x8B};memcpy(image+poseRva,code,sizeof(code));
 auto* slots=reinterpret_cast<std::uintptr_t*>(image+0x2000);
 slots[0]=base+0xB1000;slots[1]=base+poseRva;slots[2]=base+0xB1100;slots[3]=base+poseRva;slots[4]=base+0xB1200;
 auto* unrelated=reinterpret_cast<std::uintptr_t*>(image+0x4000);*unrelated=base+poseRva;
 gameBuild=nullptr;CheckObserver(!InstallParkourObserver()&&slots[1]==base+poseRva,"unknown build rejected without writes");
 auto unmapped=*build;unmapped.parkourPose=0;gameBuild=&unmapped;
 CheckObserver(!InstallParkourObserver()&&slots[1]==base+poseRva,"unmapped observer rejected without writes");
 gameBuild=build;image[poseRva]^=1;CheckObserver(!InstallParkourObserver()&&slots[1]==base+poseRva,"code mismatch rejected before writes");image[poseRva]^=1;
 slots[2]=base+0x4000;CheckObserver(!InstallParkourObserver()&&slots[1]==base+poseRva,"non-code neighbours rejected before writes");slots[2]=base+0xB1100;
 DWORD old=0;CheckObserver(VirtualProtect(image+0x2000,0x1000,PAGE_READONLY,&old)!=0,"protect synthetic dispatch table");
 denyLocalProtection=deny;deniedLocalCalls=0;
 CheckObserver(InstallParkourObserver(),"matching dispatch slots install, including denied local protection fallback");
 denyLocalProtection=false;
 CheckObserver(!deny||deniedLocalCalls==2,"fallback exercised for both slots");
 CheckObserver(slots[1]==reinterpret_cast<std::uintptr_t>(&ObservePose)&&slots[3]==reinterpret_cast<std::uintptr_t>(&ObservePose),"all matching slots changed");
 CheckObserver(*unrelated==base+poseRva&&slots[2]==base+0xB1100,"unrelated memory unchanged");
 CheckObserver(memcmp(image+poseRva,code,sizeof(code))==0,"executable instructions unchanged");
 MEMORY_BASIC_INFORMATION info{};VirtualQuery(image+0x2000,&info,sizeof(info));CheckObserver(info.Protect==PAGE_READONLY,"dispatch page protection restored");
 originalSamplePose=&FakeSample;enabled.store(false);
 CheckObserver(ObservePose(10,20,30)==12345&&forwarded[0]==10&&forwarded[1]==20&&forwarded[2]==30,"disabled observer forwards original arguments and result");
 enabled.store(true);sampledPlayer.store(0);
 CheckObserver(ObservePose(40,50,60)==12345&&forwarded[2]==60,"missing local player still forwards callback");
 VirtualFree(image,0,MEM_RELEASE);
 }
 }
 std::puts("Parkour observer version/signature guards, dispatch isolation, and callback forwarding passed.");
}
