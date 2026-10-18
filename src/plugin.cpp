#include "plugin_api.h"
#include "camera_math.h"
#include "head_anchor.h"
#include "camera_comfort.h"
#include "settings.h"
#include "combat_camera.h"
#include "body_motion.h"
#include "parkour_tracking.h"
#include "head_visibility.h"
#include "imgui.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdlib>

namespace {
const fp::GameBuild* gameBuild=nullptr;
constexpr size_t kFxSlot=0x288/sizeof(void*), kTableSlots=256;
using ApplyFx=void(__fastcall*)(void*, float*, float*, float*);
std::uintptr_t base=0;
std::atomic<bool> enabled{false}, faulted{false}, active{false};
ApplyFx originalFx=nullptr;
void** originalTable=nullptr;
// Preserve the MSVC RTTI locator at vtable[-1].
alignas(16) void* clonedStorage[kTableSlots+1]{};
wchar_t iniPath[MAX_PATH]{}, logPath[MAX_PATH]{};
float eyeHeight=1.65f, forwardOffset=0.25f, fovDegrees=70.0f;
bool lastKeyDown=false;
int toggleKey=VK_OEM_PERIOD;
ULONGLONG menuSeen=0;
SRWLOCK settingsLock=SRWLOCK_INIT;
bool trackHead=true,hideHead=true;
bool combatFraming=true,followCombatMotion=true,followLeapOfFaith=true,followParkourSpins=true;
float combatEyeHeight=-0.06f,combatForward=0.02f,combatFov=80.0f;
std::atomic<int> combatStatus{0};
using SamplePose=std::uintptr_t(__fastcall*)(std::uintptr_t,std::uintptr_t,std::uintptr_t);
SamplePose originalSamplePose=nullptr;
std::atomic<std::uintptr_t> sampledPlayer{0};
std::atomic<bool> parkourObserverReady{false};
std::atomic<std::uint64_t> parkourActiveClip{0};
SRWLOCK playbackLock=SRWLOCK_INIT;
fp::ParkourPlayback parkourPlayback;
fp::ParkourCapture parkourCapture;
float headEyeHeight=0.12f,headForward=0.16f;
std::atomic<ULONGLONG> lastApplied{0};
std::atomic<int> anchorMode{0};
std::atomic<unsigned> cameraEpoch{0};
std::atomic<bool> collisionFault{false},collisionRetracting{false},collisionReady{false};
bool collisionEnabled=true,autoThirdPerson=false;
float collisionPadding=0.12f,collisionRadius=0.16f,headHalfLife=0.07f;
float maxLookYaw=80*fp::Pi/180,maxLookDown=60*fp::Pi/180,maxLookUp=70*fp::Pi/180;
SRWLOCK logLock=SRWLOCK_INIT;

void Log(const char* message) {
    AcquireSRWLockExclusive(&logLock);
    FILE* f=nullptr;
    if (_wfopen_s(&f,logPath,L"a")==0 && f) {
        SYSTEMTIME t{}; GetLocalTime(&t);
        fprintf(f,"%02u:%02u:%02u %s\n",t.wHour,t.wMinute,t.wSecond,message);
        fclose(f);
    }
    ReleaseSRWLockExclusive(&logLock);
}

template<class T> bool Read(std::uintptr_t address,T& out) {
    if (address<0x10000) return false;
    __try { out=*reinterpret_cast<const T*>(address); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool CopyTable(void** table) {
    __try {
        memcpy(clonedStorage,table-1,sizeof(clonedStorage));
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SwapTable(void* camera,void** expected,void** replacement) {
    __try {
        return InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(camera),replacement,expected)==expected;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* LocalCamera() {
    std::uintptr_t manager=0,entries=0,cam=0;
    std::uint16_t count=0;
    if (!gameBuild || !Read(base+gameBuild->camera,manager) || !manager ||
        !Read(manager+0x40,entries) || !entries ||
        !Read(manager+0x4A,count) || count==0 || count>16 ||
        !Read(entries,cam)) return nullptr;
    return reinterpret_cast<void*>(cam);
}
// Observe pose sampling without changing its arguments, result, or animation state.
// The DLL is pinned before installing these shared dispatch slots.
std::uintptr_t __fastcall ObservePose(std::uintptr_t node,std::uintptr_t pose,std::uintptr_t context) {
    const auto result=originalSamplePose(node,pose,context);
    const auto player=sampledPlayer.load();
    if(!enabled.load()||!player)return result;
    const auto generation=cameraEpoch.load();
    fp::PoseClipSample sample{};
    if(fp::ReadPoseClip([](std::uintptr_t address,auto& value){return Read(address,value);},node,context,player,sample)) {
        AcquireSRWLockExclusive(&playbackLock);
        parkourPlayback.Observe(sample,generation,GetTickCount64());
        parkourCapture.Observe(sample);
        ReleaseSRWLockExclusive(&playbackLock);
    }
    return result;
}
bool InstallParkourObserver() {
    // Version-specific entry points, verified against captured code and live dispatch tables.
    if(!gameBuild||!gameBuild->parkourPose)return false;
    const auto poseRva=gameBuild->parkourPose;
    constexpr unsigned char signature[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41,0x56,0x48,0x83,0xEC,0x30,0x0F,0xB7,0x49,0x64,0x4D,0x8B,0x48,0x20,0x49,0x8B,0xD8,0x49,0x8B,0x81,0x60,0x02,0x00,0x00,0x45,0x8B,0x40,0x44,0xC1,0xE1,0x03,0x48,0x8B,0x34,0x01,0x48,0x8B,0xFA,0x0F,0xB6,0x53,0x12,0x48,0x8B};
    for(size_t i=0;i<sizeof(signature);++i){unsigned char b=0;if(!Read(base+poseRva+i,b)||b!=signature[i])return false;}
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!Read(base,dos)||!Read(base+static_cast<unsigned>(dos.e_lfanew),nt)||nt.FileHeader.NumberOfSections>96)return false;
    const auto sections=base+static_cast<unsigned>(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
    std::uintptr_t textStart=0,textEnd=0;
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER section{};
        if(!Read(sections+i*sizeof(section),section))return false;
        if(memcmp(section.Name,".text",5)==0) {
            const auto rva=static_cast<std::uintptr_t>(section.VirtualAddress);
            const auto size=static_cast<std::uintptr_t>(section.Misc.VirtualSize);
            if(rva>=gameBuild->imageSize||size>gameBuild->imageSize-rva)return false;
            textStart=base+rva;textEnd=textStart+size;
        }
    }
    if(base+poseRva<textStart||base+poseRva+sizeof(signature)>textEnd)return false;
    void** slots[32]{};size_t count=0;
    // Only aligned references in .rdata surrounded by game-code function pointers.
    // Swapping an aligned dispatch pointer avoids rewriting executing instructions.
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER section{};
        if(!Read(sections+i*sizeof(section),section))return false;
        if(memcmp(section.Name,".rdata",6)!=0)continue;
        const auto start=static_cast<std::uintptr_t>(section.VirtualAddress);
        const auto size=static_cast<std::uintptr_t>(section.Misc.VirtualSize);
        if(start>=gameBuild->imageSize||size>gameBuild->imageSize-start)return false;
        for(std::uintptr_t offset=8;offset+16<=size;offset+=8) {
            std::uintptr_t value=0,before=0,after=0;
            if(!Read(base+start+offset,value))return false;
            if(value!=base+poseRva)continue;
            if(!Read(base+start+offset-8,before)||!Read(base+start+offset+8,after)||
               before<textStart||before>=textEnd||after<textStart||after>=textEnd||count==32)return false;
            slots[count++]=reinterpret_cast<void**>(base+start+offset);
        }
    }
    if(!count)return false;
    originalSamplePose=reinterpret_cast<SamplePose>(base+poseRva);
    size_t installed=0;
    for(size_t i=0;i<count;++i) {
        DWORD protection=0,ignored=0;
        HANDLE process=nullptr;
        bool writable=VirtualProtect(slots[i],sizeof(void*),PAGE_READWRITE,&protection)!=FALSE;
        if(!writable) {
            const auto localError=GetLastError();
            // 1.5.1 can reject local protection changes. Match ACUFixes151's
            // real-process-handle path, but retain the atomic dispatch exchange.
            process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION,
                FALSE,GetCurrentProcessId());
            if(process) {
                writable=VirtualProtectEx(process,slots[i],sizeof(void*),PAGE_EXECUTE_READWRITE,&protection)!=FALSE;
                if(!writable)writable=VirtualProtectEx(process,slots[i],sizeof(void*),PAGE_EXECUTE_WRITECOPY,&protection)!=FALSE;
            }
            const auto fallbackError=writable?ERROR_SUCCESS:GetLastError();
            char detail[192]{};
            sprintf_s(detail,"Parkour observer: slot RVA=%llX local protection error=%lu; process-handle fallback=%s error=%lu.",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(slots[i])-base),localError,
                writable?"ready":"failed",fallbackError);
            Log(detail);
        }
        if(!writable){if(process)CloseHandle(process);continue;}
        const bool swapped=InterlockedCompareExchangePointer(slots[i],reinterpret_cast<void*>(&ObservePose),
            reinterpret_cast<void*>(originalSamplePose))==reinterpret_cast<void*>(originalSamplePose);
        const bool restored=process?
            VirtualProtectEx(process,slots[i],sizeof(void*),protection,&ignored)!=FALSE:
            VirtualProtect(slots[i],sizeof(void*),protection,&ignored)!=FALSE;
        if(process)CloseHandle(process);
        if(swapped)++installed;
        else Log("Parkour observer: dispatch slot changed before atomic exchange.");
        if(!restored)Log("Parkour observer: dispatch page protection could not be restored.");
    }
    char message[160]{};
    sprintf_s(message,"Parkour observer: %zu/%zu pose dispatch slots installed; %zu candidate clips. Full gameplay validation pending.",installed,count,std::size(fp::ParkourClips));
    Log(message);
    return installed==count;
}
std::uint64_t ActiveParkourClip(std::uintptr_t entity,unsigned generation) {
    AcquireSRWLockShared(&playbackLock);
    const auto clip=parkourPlayback.Active(entity,generation,GetTickCount64());
    ReleaseSRWLockShared(&playbackLock);
    return clip;
}
bool MenuOpen() {
    std::uintptr_t manager=0,stack=0;
    std::uint16_t count=0;
    if (!gameBuild || !Read(base+gameBuild->menu,manager) || !manager ||
        !Read(manager+0x30,stack)) return true;
    if (!stack) return false;
    return !Read(stack+0x1A,count) || count!=0;
}
bool GameFocused() {
    DWORD pid=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    return pid==GetCurrentProcessId();
}
bool SupportedBuild() {
    auto* module=GetModuleHandleW(L"ACU.exe");
    if (!module) return false;
    base=reinterpret_cast<std::uintptr_t>(module);
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!Read(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew<=0 || dos.e_lfanew>0x100000 ||
        !Read(base+static_cast<unsigned>(dos.e_lfanew),nt)) return false;
    if (!(nt.Signature==IMAGE_NT_SIGNATURE &&
        nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
        nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC)) return false;
    gameBuild=fp::SelectBuild(nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage,nt.OptionalHeader.CheckSum);
    if(gameBuild) { Log("Selected executable profile:"); Log(gameBuild->name); }
    return gameBuild!=nullptr;
}
bool InGameImage(void* p) {
    const auto a=reinterpret_cast<std::uintptr_t>(p);
    return gameBuild && a>=base && a<base+gameBuild->imageSize;
}
float Setting(const wchar_t* key,float fallback,float low,float high) {
    wchar_t text[64]{};
    GetPrivateProfileStringW(L"Camera",key,L"",text,64,iniPath);
    if (!text[0]) return fallback;
    wchar_t* end=nullptr;
    const float value=wcstof(text,&end);
    if (end==text || *end!=0 || !std::isfinite(value) || value<low || value>high) return fallback;
    return value;
}
void UpdateHotkey() {
    const bool down=(GetAsyncKeyState(toggleKey)&0x8000)!=0;
    if (down && !lastKeyDown && GetTickCount64()-menuSeen>250 && GameFocused() && !MenuOpen() && !faulted.load()) {
        const bool next=!enabled.load();
        enabled.store(next);
        cameraEpoch.fetch_add(1);
        Log(next ? "Toggle: first person requested." : "Toggle: third person restored.");
    }
    lastKeyDown=down;
}

struct HiddenVisual { std::uintptr_t address=0,owner=0; std::uint64_t asset=0; std::uint16_t savedViewFlags=0; };
HiddenVisual hiddenVisuals[32]{};
// Refresh only known renderer object implementations. No per-frame code patch.
bool SetViewFlags(std::uintptr_t visual,std::uint16_t viewFlags) {
    using Refresh=void(__fastcall*)(void*);
    std::uintptr_t lod=0,table=0,method=0;
    std::uint16_t flags=0;
    if(!Read(visual+0x54,flags)||!Read(visual+0x40,lod)||!lod||
       !Read(lod,table)||!table||!Read(table+0x118,method)||
       (method!=base+gameBuild->refreshChildren&&method!=base+gameBuild->refresh))return false;
    const auto next=fp::RestoreMainView(flags,viewFlags);
    if(next==flags)return true;
    __try {
        *reinterpret_cast<std::uint16_t*>(visual+0x54)=next;
        reinterpret_cast<Refresh>(method)(reinterpret_cast<void*>(lod));
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        __try {*reinterpret_cast<std::uint16_t*>(visual+0x54)=flags;}
        __except(EXCEPTION_EXECUTE_HANDLER) {}
        faulted.store(true);enabled.store(false);return false;
    }
}
void UpdateHeadVisibility() {
    const auto camera=reinterpret_cast<std::uintptr_t>(LocalCamera());
    std::uintptr_t entity=0,components=0;
    std::uint16_t count=0;
    if(!camera || !Read(camera+0x10,entity) || !entity ||
       !Read(entity+0x78,components) || !components ||
       !Read(entity+0x82,count) || count>128) return;
    const auto last=lastApplied.load();
    const bool hide=hideHead && enabled.load() && !faulted.load() &&
        GameFocused() && !MenuOpen() && last!=0 && GetTickCount64()-last<200;
    bool present[32]{};
    for(std::uint16_t i=0;i<count;++i) {
        std::uintptr_t visual=0,vt=0,owner=0,shared=0;
        std::uint64_t asset=0;
        std::uint16_t flags=0;
        if(!Read(components+8*i,visual) || !visual || !Read(visual,vt) ||
           vt!=base+gameBuild->visual || !Read(visual+0x10,owner) || owner!=entity ||
           !Read(visual+0x38,shared) || !shared || !Read(shared+0x10,asset) || !Read(visual+0x54,flags)) continue;
        static std::uint64_t reportedAssets[128]{};
        static unsigned reportedAssetCount=0;
        bool reportedAsset=false;
        for(unsigned k=0;k<reportedAssetCount;++k)if(reportedAssets[k]==asset){reportedAsset=true;break;}
        if(!reportedAsset&&reportedAssetCount<std::size(reportedAssets)) {
            reportedAssets[reportedAssetCount++]=asset;
            char message[160]{};
            sprintf_s(message,"Player visual: asset=%llu head=%u flags=%04X",static_cast<unsigned long long>(asset),fp::KnownHeadAsset(asset)?1u:0u,static_cast<unsigned>(flags));
            Log(message);
        }
        if(!fp::KnownHeadAsset(asset))continue;
        int record=-1;
        for(int k=0;k<32;++k) {
            if(hiddenVisuals[k].address==visual && hiddenVisuals[k].owner==entity &&
               hiddenVisuals[k].asset==asset) { record=k; present[k]=true; break; }
        }
        if(!hide) {
            if(record>=0 && SetViewFlags(visual,hiddenVisuals[record].savedViewFlags)) hiddenVisuals[record]={};
            continue;
        }
        if(record<0 && !(flags&1)) {
            for(int k=0;k<32;++k) if(!hiddenVisuals[k].address) {
                record=k; hiddenVisuals[k]={visual,entity,asset,static_cast<std::uint16_t>(flags&fp::MainViewMask)}; present[k]=true; break;
            }
        }
        // Preserve meshes that were already hidden before first person.
        if(record>=0 && !(flags&1) && !SetViewFlags(visual,fp::HideMainView(flags))) {
            static bool reported=false;
            if(!reported){Log("Selective head hiding unavailable: unknown renderer or refresh failure.");reported=true;}
        }
    }
    // Never restore through a stale pointer after a respawn/outfit replacement.
    for(int k=0;k<32;++k) if(hiddenVisuals[k].address && !present[k]) hiddenVisuals[k]={};
}


// ACU 1.5.0 ray-query ABI, independently declared from documented layouts.
struct alignas(16) RayHit {
    float point[4],normal[4];
    std::uintptr_t physics,entityReference;
    unsigned char remainder[0x30];
};
static_assert(sizeof(RayHit)==0x60);
struct alignas(16) RayQuery { unsigned char bytes[0x680]; };
bool CastClearance(std::uintptr_t world,std::uintptr_t player,
                   fp::Vec3 origin,fp::Vec3 direction,float length,float& allowed) {
    using Construct=void*(__fastcall*)(void*);
    using Cast=char(__fastcall*)(void*,void*,const float*,const float*,float,unsigned char,char);
    using Destroy=void(__fastcall*)(void*);
    RayQuery query{};
    alignas(16) float start[4]={origin.x,origin.y,origin.z,0};
    alignas(16) float ray[4]={direction.x,direction.y,direction.z,0};
    bool constructed=false,ok=false;
    __try {
        reinterpret_cast<Construct>(base+gameBuild->rayCtor)(&query);
        constructed=true;
        // The documented gunshot solid-geometry filter avoids querying all trigger types.
        const std::uint64_t flags=0x1A00805247ULL;
        memcpy(query.bytes+0x650,&flags,sizeof(flags));
        reinterpret_cast<Cast>(base+gameBuild->rayCast)(&query,reinterpret_cast<void*>(world),start,ray,length,1,0);
        std::uintptr_t hits=0;
        std::uint16_t count=0;
        memcpy(&hits,query.bytes+8,8);
        memcpy(&count,query.bytes+0x12,2);
        ok=count<=1024 && (count==0 || hits!=0);
        for(std::uint16_t i=0;ok && i<count;++i) {
            RayHit hit{};
            if(!Read(hits+sizeof(RayHit)*i,hit)) {ok=false;break;}
            std::uintptr_t hitEntity=0;
            if(hit.entityReference) Read(hit.entityReference,hitEntity);
            if(hitEntity==player) continue;
            const fp::Vec3 p{hit.point[0],hit.point[1],hit.point[2]};
            if(!fp::Finite(p)) {ok=false;break;}
            allowed=std::min(allowed,fp::SafeHitDistance(origin,direction,length,p,collisionPadding));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {ok=false;}
    if(constructed) {
        __try {reinterpret_cast<Destroy>(base+gameBuild->rayDestroy)(query.bytes+8);}
        __except(EXCEPTION_EXECUTE_HANDLER) {ok=false;}
    }
    return ok;
}
bool ProtectEye(std::uintptr_t entity,fp::Vec3 original,fp::Vec3 desired,fp::Quat q,
                float dt,float& lastClearance,fp::Vec3& output) {
    std::uintptr_t world=0;
    if(!Read(entity+0x60,world)||!world)return false;
    const fp::Vec3 delta=fp::Sub(desired,original);
    const float length=fp::Length(delta);
    if(!std::isfinite(length)||length>20.0f)return false;
    if(length<0.001f){output=original;lastClearance=0;return true;}
    const fp::Vec3 direction=fp::Scale(delta,1/length);
    fp::Vec3 forward{};
    if(!fp::EyePosition({0,0,0},q,0,1,forward))return false;
    const fp::Vec3 right=fp::Rotate(q,{1,0,0});
    const fp::Vec3 up=fp::Rotate(q,{0,0,1});
    // Five parallel rays approximate near-plane clearance. This is not a sphere sweep.
    const fp::Vec3 r=fp::Scale(right,collisionRadius),u=fp::Scale(up,collisionRadius);
    const fp::Vec3 offsets[]={{0,0,0},fp::Add(r,u),fp::Sub(r,u),
       fp::Sub(u,r),fp::Scale(fp::Add(r,u),-1)};
    float allowed=length;
    for(const auto offset:offsets)
        if(!CastClearance(world,entity,fp::Add(original,offset),direction,length,allowed))return false;
    // Store obstruction distance from the requested eye rather than world position:
    // following motion must not inherit the third-person camera's smoothing lag.
    const float obstruction=length-allowed;
    if(lastClearance<0)lastClearance=obstruction;
    if(obstruction>lastClearance)lastClearance=obstruction;
    else lastClearance=fp::CollisionDistance(-lastClearance,-obstruction,dt)*-1;
    const float travel=std::clamp(length-lastClearance,0.0f,length);
    output=fp::Add(original,fp::Scale(direction,travel));
    collisionRetracting.store(obstruction>0.001f);
    collisionReady.store(true);
    return true;
}

// Diagnostic only. An unknown receiver is not evidence of being grounded.
std::uintptr_t NavigationState(std::uintptr_t entity) {
    std::uintptr_t components=0,anim=0,human=0,holder=0,bags=0,bag=0,receivers=0,node=0,vt=0;
    std::uint8_t index=0;
    std::uint16_t count=0,bagCount=0,receiverCount=0;
    if(!Read(entity+0x78,components)||!components||!Read(entity+0x82,count)||count>128||
       !Read(entity+0x158,index)||index>=count||!Read(components+8*index,anim)||!anim||
       !Read(anim+0xC58,human)||!human||!Read(human+0x710,holder)||!holder||
       !Read(holder+0x3C4,bags)||!bags||!Read(holder+0x3CE,bagCount)||bagCount<=366||bagCount>2048||
       !Read(bags+366*8,bag)||!bag||!Read(bag+8,receivers)||!receivers||
       !Read(bag+0x12,receiverCount)||!receiverCount||receiverCount>128||
       !Read(receivers+8,node)||!node||!Read(node,vt)||!InGameImage(reinterpret_cast<void*>(vt)))return 0;
    return vt-base;
}

bool ApplyEye(void* self,float* position,float* rotation,float* fov,const fp::Quat& cleanRotation) {
    parkourActiveClip.store(0);
    if (!position || !rotation || !fov || self!=LocalCamera() || MenuOpen()) {sampledPlayer.store(0);return false;}
    std::uintptr_t entity=0;
    fp::Vec3 root{},old{},bodyForward{};
    fp::Quat q=cleanRotation;
    const auto address=reinterpret_cast<std::uintptr_t>(self);
    if (!Read(address+0x10,entity) || !entity ||
        !Read(entity+0x50,root) || !Read(entity+0x30,bodyForward) ||
        !Read(reinterpret_cast<std::uintptr_t>(position),old)) return false;
    if(!fp::Finite(old)||!fp::Finite(root))return false;
    const auto read=[](std::uintptr_t addr,auto& value){return Read(addr,value);};
    bool groundRoll=false,leap=false;
    const auto combatState=fp::ReadCombat(read,base,entity,&groundRoll,&leap,*gameBuild);
    const bool followLeap=followLeapOfFaith&&leap;
    sampledPlayer.store(entity);
    const auto parkourClip=followParkourSpins&&parkourObserverReady.load()?ActiveParkourClip(entity,cameraEpoch.load()):0;
    const bool followParkour=parkourClip!=0&&!followLeap;
    parkourActiveClip.store(followParkour?parkourClip:0);
    // Native cinematic pullback is expected during a verified leap.
    if(fp::DistanceSquared(old,root)>144.0f&&!followLeap)return false;
    struct State {
        std::uintptr_t entity=0;
        unsigned epoch=0;
        LONGLONG tick=0;
        fp::HeadFilter filter;
        fp::LookController look;
        fp::CameraFallback fallback;
        fp::CombatBlend combat;
        fp::BodyMotion motion;
        fp::ParkourView parkourView;
        ULONGLONG diagnosticTime=0;
        float obstruction=-1;
        float peakPullback=0,peakHeadLag=0,peakYawStep=0;
        float lastOutputYaw=0;
        bool haveOutput=false;
    };
    static thread_local State state;
    LARGE_INTEGER tick{},frequency{};
    QueryPerformanceCounter(&tick);QueryPerformanceFrequency(&frequency);
    const unsigned epoch=cameraEpoch.load();
    const double elapsed=state.tick&&frequency.QuadPart>0 ?
        static_cast<double>(tick.QuadPart-state.tick)/static_cast<double>(frequency.QuadPart):0;
    if(state.entity!=entity||state.epoch!=epoch||elapsed>0.25||elapsed<0) {
        state=State{};state.entity=entity;state.epoch=epoch;
    }
    const float dt=state.tick?static_cast<float>(std::clamp(elapsed,0.0,0.1)):0;
    state.tick=tick.QuadPart;
    if(!state.look.Step(q,bodyForward,maxLookYaw,maxLookDown,maxLookUp,q))return false;
    fp::Vec3 head{};
    const bool hasHead=trackHead&&fp::ReadHead(read,base,entity,head,*gameBuild);
    const fp::Vec3 anchor=state.filter.Step(root,head,hasHead,dt,headHalfLife,eyeHeight-headEyeHeight,
        fp::FollowRoll(followCombatMotion,groundRoll,hasHead)||((followLeap||followParkour)&&hasHead));
    const bool headAvailable=trackHead&&state.filter.Available();
    fp::Vec3 eye{};
    combatStatus.store(static_cast<int>(combatState));
    const float combatAmount=state.combat.Step(combatState,combatFraming,dt);
    const float framedHeight=fp::CombatLerp(headEyeHeight,combatEyeHeight,combatAmount);
    const float framedForward=fp::CombatLerp(headForward,combatForward,combatAmount);
    if(!fp::LevelEyePosition(anchor,q,framedHeight,framedForward,eye))return false;
    // Combat membership is not a roll/spin detector. Never track ordinary attack
    // bends or reuse combat's 0.45-second hold for orientation.
    const float motionAmount=(fp::FollowRoll(followCombatMotion,groundRoll,hasHead)||((followLeap||followParkour)&&hasHead))?1.0f:0.0f;
    fp::Quat torso{};
    const bool hasTorso=hasHead&&motionAmount>0.001f&&fp::ReadTorso(read,base,entity,torso,*gameBuild);
    const auto motionPose=state.parkourView.Step(torso,q,hasTorso&&followParkour);
    const auto bodyRotation=state.motion.Step(followParkour?motionPose:torso,hasTorso,state.look.previousBody,motionAmount,dt,(followLeap||followParkour)?&q:nullptr);
    // Keep the filtered anchor, including during roll recovery. Raw torso/head
    // displacement must not drag the camera through an attack animation.
    eye=fp::Add(anchor,fp::Rotate(bodyRotation,fp::Sub(eye,anchor)));
    q=fp::Multiply(bodyRotation,q);
    anchorMode.store(hasHead?2:3);
    const fp::Vec3 requestedEye=eye;
    if(collisionEnabled&&!collisionFault.load()) {
        fp::Vec3 protectedEye{};
        if(!ProtectEye(entity,fp::ClearanceOrigin(root,anchor),eye,q,dt,state.obstruction,protectedEye)) {
            collisionFault.store(true);
            lastApplied.store(0);
            return false;
        }
        eye=protectedEye;
    } else if(collisionEnabled) {
        lastApplied.store(0);
        return false; // failed query: retain the engine camera instead of a blind override
    }
    const bool wasFallback=state.fallback.active;
    const bool useNative=state.fallback.Step(fp::Length(fp::Sub(eye,requestedEye)),
        state.look.relative,headAvailable,dt,autoThirdPerson);
    if(useNative!=wasFallback) {
        char reason[256]{};
        sprintf_s(reason,"Camera fallback %s: collision=%d rearLook=%d missingHead=%d pullback=%.3f yaw=%.1f navRva=%llX",
            useNative?"entered":"cleared",
            fp::Length(fp::Sub(eye,requestedEye))>0.25f?1:0,
            std::abs(state.look.relative)>85*fp::Pi/180?1:0,headAvailable?0:1,
            fp::Length(fp::Sub(eye,requestedEye)),state.look.relative*180/fp::Pi,
            static_cast<unsigned long long>(NavigationState(entity)));
        Log(reason);
    }
    // Aggregate every applied frame: one-second snapshots otherwise miss short snaps.
    const float outputYaw=fp::Wrap(state.look.previousBody+state.look.relative);
    if(state.haveOutput)state.peakYawStep=std::max(state.peakYawStep,std::abs(fp::Wrap(outputYaw-state.lastOutputYaw)));
    state.haveOutput=true;state.lastOutputYaw=outputYaw;
    state.peakPullback=std::max(state.peakPullback,fp::Length(fp::Sub(eye,requestedEye)));
    if(hasHead)state.peakHeadLag=std::max(state.peakHeadLag,fp::Length(fp::Sub(anchor,head)));
    const auto now=GetTickCount64();
    // Bounded diagnostic: enough for two dives, no unbounded per-frame logging.
    static thread_local unsigned diveSamples=0;
    static thread_local ULONGLONG diveSampleTime=0;
    if(followLeap&&diveSamples<600&&now-diveSampleTime>=16) {
        diveSampleTime=now;++diveSamples;
        char sample[1024]{};
        sprintf_s(sample,"DiveSample n=%u t=%llu dt=%.4f head=%d torso=%d fallback=%d root=%.4f,%.4f,%.4f rawHead=%.4f,%.4f,%.4f anchor=%.4f,%.4f,%.4f eye=%.4f,%.4f,%.4f nativeQ=%.5f,%.5f,%.5f,%.5f torsoQ=%.5f,%.5f,%.5f,%.5f outputQ=%.5f,%.5f,%.5f,%.5f pullback=%.4f",
            diveSamples,static_cast<unsigned long long>(now),dt,hasHead?1:0,hasTorso?1:0,useNative?1:0,
            root.x,root.y,root.z,head.x,head.y,head.z,anchor.x,anchor.y,anchor.z,eye.x,eye.y,eye.z,
            cleanRotation.x,cleanRotation.y,cleanRotation.z,cleanRotation.w,
            torso.x,torso.y,torso.z,torso.w,q.x,q.y,q.z,q.w,fp::Length(fp::Sub(eye,requestedEye)));
        Log(sample);
    }
    if(now-state.diagnosticTime>=1000) {
        state.diagnosticTime=now;
        char message[384]{};
        sprintf_s(message,"Look: navRva=%llX rawYaw=%.1f bodyYaw=%.1f relative=%.1f pitch=%.1f limit=%d peakYawStep=%.1f peakPullback=%.3f peakHeadLag=%.3f combat=%d combatBlend=%.2f motion=%.2f torso=%d groundRoll=%d leap=%d",
            static_cast<unsigned long long>(NavigationState(entity)),
            state.look.previousYaw*180/fp::Pi,state.look.previousBody*180/fp::Pi,
            state.look.relative*180/fp::Pi,state.look.pitch*180/fp::Pi,state.look.atLimit?1:0,
            state.peakYawStep*180/fp::Pi,state.peakPullback,state.peakHeadLag,
            static_cast<int>(combatState),combatAmount,motionAmount,hasTorso?1:0,groundRoll?1:0,leap?1:0);
        Log(message);
        state.peakYawStep=state.peakPullback=state.peakHeadLag=0;
    }
    if(useNative){lastApplied.store(0);return false;}
    __try {
        position[0]=eye.x;position[1]=eye.y;position[2]=eye.z;
        rotation[0]=q.x;rotation[1]=q.y;rotation[2]=q.z;rotation[3]=q.w;
        const float nativeFov=*fov;
        const float normalFov=fovDegrees>0?fovDegrees*fp::Pi/180:nativeFov;
        if(std::isfinite(normalFov)&&normalFov>0&&normalFov<fp::Pi)
            *fov=fp::CombatFov(normalFov,combatFov*fp::Pi/180,combatAmount);
        lastApplied.store(GetTickCount64());
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        faulted.store(true);enabled.store(false);return false;
    }
}
void __fastcall CameraFx(void* self,float* position,float* rotation,float* fov) {
    // Capture orientation before camera FX; avoid inheriting landing shake/roll.
    fp::Quat clean{};
    const bool haveClean=Read(reinterpret_cast<std::uintptr_t>(rotation),clean);
    originalFx(self,position,rotation,fov);
    if (!haveClean || !enabled.load() || faulted.load() || !GameFocused()) {parkourActiveClip.store(0);sampledPlayer.store(0);return;}
    AcquireSRWLockShared(&settingsLock);
    const bool applied=ApplyEye(self,position,rotation,fov,clean);
    ReleaseSRWLockShared(&settingsLock);
    if (applied && !active.exchange(true))
        Log("First-person camera callback reached and position applied.");
}
void InstallOnCurrentCamera() {
    void* camera=LocalCamera();
    if (!camera || (reinterpret_cast<std::uintptr_t>(camera)%alignof(void*))!=0) return;
    void** table=nullptr;
    if (!Read(reinterpret_cast<std::uintptr_t>(camera),table) || !table ||
        table==clonedStorage+1) return;
    if (!InGameImage(table)) return; // Do not replace another mod's vtable.
    if (originalTable && originalTable!=table) return;
    if (!originalTable) {
        void* fn=nullptr;
        if (!Read(reinterpret_cast<std::uintptr_t>(table+kFxSlot),fn) || !InGameImage(fn) ||
            !CopyTable(table)) return;
        originalFx=reinterpret_cast<ApplyFx>(fn);
        clonedStorage[kFxSlot+1]=reinterpret_cast<void*>(&CameraFx);
        originalTable=table;
    }
    if (SwapTable(camera,table,clonedStorage+1))
        Log("Local player camera attached. Use the configured first-person toggle key.");
}
bool BindGui(loader::Gui& gui) {
    if(!gui.context||!gui.allocate||!gui.release)return false;
    ImGui::SetCurrentContext(gui.context);
    ImGui::SetAllocatorFunctions(gui.allocate,gui.release,gui.userData);
    return true;
}
void KeyName(int key,char (&name)[64]) {
    UINT scan=MapVirtualKeyW(static_cast<UINT>(key),MAPVK_VK_TO_VSC);
    if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_HOME||
       key==VK_END||key==VK_PRIOR||key==VK_NEXT||key==VK_DELETE)scan|=0x100;
    if(!GetKeyNameTextA(static_cast<LONG>(scan<<16),name,64))sprintf_s(name,"Key %d",key);
}
void Menu(loader::Gui& gui) {
    if(!BindGui(gui))return;
    menuSeen=GetTickCount64();
    static bool initialized=false,capturing=false,held[256]{};
    static fp::Settings draft;
    static char status[160]{};
    if(!initialized) {
        draft.key=toggleKey;draft.fov=fovDegrees;draft.eyeHeight=headEyeHeight;
        draft.forward=headForward;draft.smoothing=headHalfLife;draft.padding=collisionPadding;
        draft.radius=collisionRadius;draft.hideHead=hideHead;draft.collision=collisionEnabled;
        draft.automatic=autoThirdPerson;
        draft.followParkour=followParkourSpins;draft.followLeap=followLeapOfFaith;draft.followMotion=followCombatMotion;draft.combat=combatFraming;draft.combatHeight=combatEyeHeight;
        draft.combatForward=combatForward;draft.combatFov=combatFov;initialized=true;
    }
    ImGui::PushID("ACUnityFirstPerson");
    ImGui::TextUnformatted("First Person 0.8.6");
    bool on=enabled.load();
    ImGui::BeginDisabled(faulted.load());
    if(ImGui::Checkbox("First person enabled",&on)) {
        enabled.store(on);cameraEpoch.fetch_add(1);
        Log(on?"Menu: first person requested.":"Menu: third person restored.");
    }
    ImGui::EndDisabled();
    if(faulted.load()||collisionFault.load())ImGui::TextWrapped("A camera error occurred. Restart the game to recover.");
    ImGui::Separator();
    char name[64]{};KeyName(draft.key,name);
    ImGui::Text("Toggle key: %s",name);
    if(ImGui::Button(capturing?"Cancel key change":"Change toggle key")) {
        capturing=!capturing;
        for(int key=0;key<256;++key)held[key]=(GetAsyncKeyState(key)&0x8000)!=0;
    }
    if(capturing&&GameFocused()) {
        ImGui::TextWrapped("Press a keyboard key. Escape cancels. Insert is reserved for the loader menu.");
        for(int key=8;key<256;++key) {
            const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
            if(down&&!held[key]) {
                if(key==VK_ESCAPE){capturing=false;break;}
                if(fp::BindableKey(key)){draft.key=key;capturing=false;break;}
            }
            held[key]=down;
        }
    }
    ImGui::TextWrapped("Avoid keys already used by the game or other mods. Period (.) remains the default.");
    bool nativeFov=draft.fov==0;
    if(ImGui::Checkbox("Use game field of view",&nativeFov))draft.fov=nativeFov?0.0f:70.0f;
    ImGui::BeginDisabled(nativeFov);
    ImGui::SliderFloat("Vertical field of view",&draft.fov,nativeFov?0.0f:40.0f,nativeFov?0.0f:100.0f,"%.0f degrees",ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::SliderFloat("Eye height adjustment",&draft.eyeHeight,-0.2f,0.4f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Eye forward adjustment",&draft.forward,-0.1f,0.3f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Head smoothing",&draft.smoothing,0.01f,0.3f,"%.3f seconds",ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Follow parkour spins",&draft.followParkour);
    if(draft.followParkour)
        ImGui::TextWrapped("For the smoothest parkour, let go of the mouse during moves and let Arno take over. Mouse control stays available whenever you need it.");
    ImGui::Checkbox("Follow Leap of Faith",&draft.followLeap);
    ImGui::Checkbox("Follow recognized rolls",&draft.followMotion);
    ImGui::TextWrapped("Rotate the camera with recognized ground and combat rolls. Ordinary attacks and parries keep your normal aim.");
    ImGui::Checkbox("Combat framing",&draft.combat);
    if(ImGui::TreeNode("Combat view")) {
        ImGui::TextWrapped("Lowers the eye and widens the view during combat to show more arms and weapon movement. Changes blend smoothly. Field of view never narrows an already wider view.");
        ImGui::SliderFloat("Combat eye height",&draft.combatHeight,-0.2f,0.2f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Combat forward adjustment",&draft.combatForward,-0.1f,0.3f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Combat vertical field of view",&draft.combatFov,40,100,"%.0f degrees",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TreePop();
    }
    ImGui::Checkbox("Hide head in main view (experimental shadows)",&draft.hideHead);
    ImGui::Checkbox("Wall clearance",&draft.collision);
    ImGui::Checkbox("Automatically fall back to third person",&draft.automatic);
    ImGui::TextWrapped("Automatic fallback uses third person for rearward looks or large wall corrections. Leave off to stay in first person.");
    if(ImGui::TreeNode("Advanced wall clearance")) {
        ImGui::SliderFloat("Wall padding",&draft.padding,0.05f,0.3f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Probe radius",&draft.radius,0.03f,0.25f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TreePop();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(capturing);
    if(ImGui::Button("Save & apply")) {
        if(fp::SaveSettings(iniPath,draft)) {
            AcquireSRWLockExclusive(&settingsLock);
            toggleKey=draft.key;fovDegrees=draft.fov;headEyeHeight=draft.eyeHeight;
            headForward=draft.forward;headHalfLife=draft.smoothing;collisionPadding=draft.padding;
            collisionRadius=draft.radius;hideHead=draft.hideHead;collisionEnabled=draft.collision;
            autoThirdPerson=draft.automatic;
            followParkourSpins=draft.followParkour;followLeapOfFaith=draft.followLeap;followCombatMotion=draft.followMotion;combatFraming=draft.combat;combatEyeHeight=draft.combatHeight;
            combatForward=draft.combatForward;combatFov=draft.combatFov;cameraEpoch.fetch_add(1);
            ReleaseSRWLockExclusive(&settingsLock);
            lastKeyDown=(GetAsyncKeyState(toggleKey)&0x8000)!=0;
            strcpy_s(status,"Saved and applied. No restart needed.");Log("Menu settings saved and applied.");
        } else strcpy_s(status,"Could not save settings. Changes were not applied.");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Reset defaults")){draft=fp::Settings{};capturing=false;strcpy_s(status,"Defaults selected. Use Save & apply to keep them.");}
    ImGui::TextWrapped("Camera changes apply when saved. Enable/disable takes effect immediately.");
    if(status[0])ImGui::TextWrapped("%s",status);
    ImGui::PopID();
}
void Frame(loader::Gui& gui) {
    if(BindGui(gui)&&(ImGui::GetIO().WantCaptureKeyboard||ImGui::GetIO().WantCaptureMouse))menuSeen=GetTickCount64();
    if (!faulted.load()) {
        UpdateHotkey();
        InstallOnCurrentCamera();
    }
    UpdateHeadVisibility();
    static bool reportedCollisionReady=false;
    if(collisionReady.load()&&!reportedCollisionReady){reportedCollisionReady=true;Log("Native collision rays completed successfully.");}
    static bool reportedCollisionFault=false;
    if(collisionFault.load()&&!reportedCollisionFault){reportedCollisionFault=true;Log("Collision query failed: retaining engine camera for this session.");}
    // Log outside the sampling lock and cap work per GUI frame.
    for(unsigned i=0;i<16;++i) {
        fp::ParkourCapture::Entry entry{};
        AcquireSRWLockExclusive(&playbackLock);
        const bool captured=parkourCapture.Pop(entry);
        ReleaseSRWLockExclusive(&playbackLock);
        if(!captured)break;
        const auto* known=fp::FindParkourClip(entry.handle);
        char message[192]{};
        sprintf_s(message,"Parkour sampled: clip=%llu token=%u family=%s",static_cast<unsigned long long>(entry.handle),entry.token,known?known->family:"unmapped");
        Log(message);
    }
    static ULONGLONG nextParkourDiagnostic=0;
    static unsigned diagnosticReports=0;
    const auto diagnosticTime=GetTickCount64();
    if(parkourObserverReady.load()&&diagnosticReports<360&&diagnosticTime>=nextParkourDiagnostic) {
        nextParkourDiagnostic=diagnosticTime+5000;++diagnosticReports;
        AcquireSRWLockShared(&playbackLock);
        const auto d=parkourPlayback;
        ReleaseSRWLockShared(&playbackLock);
        char message[256]{};
        sprintf_s(message,"Parkour counters: samples=%llu matches=%llu late=%llu last=%llu token=%u latest=%u matched=%u active=%llu",static_cast<unsigned long long>(d.samples),static_cast<unsigned long long>(d.matches),static_cast<unsigned long long>(d.lateSamples),static_cast<unsigned long long>(d.lastHandle),d.lastToken,d.latestFrame,d.matchedFrame,static_cast<unsigned long long>(parkourActiveClip.load()));
        Log(message);
    }
    static std::uint64_t reportedParkour=0;
    static unsigned parkourReports=0;
    const auto clip=parkourActiveClip.load();
    if(clip!=reportedParkour&&parkourReports<256) {
        reportedParkour=clip;++parkourReports;
        const auto* entry=fp::FindParkourClip(clip);
        char message[160]{};sprintf_s(message,"Parkour playback: clip=%llu family=%s",static_cast<unsigned long long>(clip),entry?entry->family:"none");Log(message);
    }
    static int reported=0;
    const int mode=anchorMode.load();
    if(mode && mode!=reported) {
        reported=mode;
        Log(mode==2?"Anchor: smoothed animated head.":"Anchor: briefly held/eased fallback (head unavailable).");
    }
}
bool Ready(loader::Interface&) {
    if (!SupportedBuild()) { Log("Unsupported executable; plugin dormant, no camera writes."); return false; }
    // A game object may still dispatch to the hook during loader unload.
    // Keep this DLL resident until process exit. Restart is required to remove it.
    HMODULE pinned=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&CameraFx),&pinned)) return false;
    toggleKey=static_cast<int>(GetPrivateProfileIntW(L"Camera",L"ToggleKey",VK_OEM_PERIOD,iniPath));
    if(!fp::BindableKey(toggleKey))toggleKey=VK_OEM_PERIOD;
    eyeHeight=Setting(L"EyeHeight",1.65f,0.5f,2.5f);
    forwardOffset=Setting(L"ForwardOffset",0.25f,-0.5f,0.75f);
    fovDegrees=Setting(L"VerticalFOV",70.0f,0.0f,100.0f);
    if (fovDegrees>0 && fovDegrees<40) fovDegrees=70.0f;
    followParkourSpins=GetPrivateProfileIntW(L"Camera",L"FollowParkourSpins",1,iniPath)!=0;
    followLeapOfFaith=GetPrivateProfileIntW(L"Camera",L"FollowLeapOfFaith",1,iniPath)!=0;
    followCombatMotion=GetPrivateProfileIntW(L"Camera",L"FollowCombatMotion",1,iniPath)!=0;
    combatFraming=GetPrivateProfileIntW(L"Camera",L"CombatFraming",1,iniPath)!=0;
    combatEyeHeight=Setting(L"CombatEyeHeight",-0.06f,-0.2f,0.2f);
    combatForward=Setting(L"CombatForward",0.02f,-0.1f,0.3f);
    combatFov=Setting(L"CombatVerticalFOV",80,40,100);
    trackHead=GetPrivateProfileIntW(L"Camera",L"TrackAnimatedHead",1,iniPath)!=0;
    hideHead=GetPrivateProfileIntW(L"Camera",L"HideHead",1,iniPath)!=0;
    headEyeHeight=Setting(L"HeadEyeHeight",0.12f,-0.2f,0.4f);
    headForward=Setting(L"HeadForward",0.16f,-0.1f,0.3f);
    headHalfLife=Setting(L"HeadSmoothingHalfLife",0.07f,0.01f,0.3f);
    maxLookYaw=Setting(L"MaxLookYaw",80,30,85)*fp::Pi/180;
    maxLookDown=Setting(L"MaxLookDown",60,20,75)*fp::Pi/180;
    maxLookUp=Setting(L"MaxLookUp",70,20,85)*fp::Pi/180;
    autoThirdPerson=GetPrivateProfileIntW(L"Camera",L"AutoThirdPerson",0,iniPath)!=0;
    collisionEnabled=GetPrivateProfileIntW(L"Camera",L"CollisionGuard",1,iniPath)!=0;
    collisionPadding=Setting(L"CollisionPadding",0.12f,0.05f,0.3f);
    collisionRadius=Setting(L"CollisionRadius",0.16f,0.03f,0.25f);
    parkourObserverReady.store(InstallParkourObserver());
    if(!parkourObserverReady.load())Log("Parkour observer unavailable: normal camera and existing roll/leap tracking retained.");
    Log("Prototype 0.8.6 ready: configurable key and camera settings in plugin menu. Starts OFF.");
    return true;
}
}

extern "C" __declspec(dllexport) bool ACUPluginStart(loader::Interface& api,loader::Info& info) {
    wchar_t path[MAX_PATH]{};
    HMODULE thisModule=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ACUPluginStart),&thisModule)) return false;
    const DWORD length=GetModuleFileNameW(thisModule,path,MAX_PATH);
    if (!length || length>=MAX_PATH) return false;
    wchar_t* slash=wcsrchr(path,L'\\');
    if (!slash) return false;
    slash[1]=0;
    if (swprintf_s(iniPath,L"%sACUnityFirstPerson.ini",path)<0 ||
        swprintf_s(logPath,L"%sACUnityFirstPerson.log",path)<0) return false;
    std::uint64_t version=0;
    if (!Read(reinterpret_cast<std::uintptr_t>(&api),version) || version<loader::kApi) return false;
    info={loader::kApi,0x00000805,&Ready,&Menu,&Frame,nullptr,nullptr};
    return true;
}
