#include "settings.h"
#include <cstdlib>
#include <cstdio>
void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
 fp::Settings s;
 Check(fp::ValidSettings(s),"defaults valid");
 Check(s.gait,"gait camera defaults on");
 Check(!fp::BindableKey(VK_INSERT)&&!fp::BindableKey(VK_ESCAPE)&&!fp::BindableKey(VK_LBUTTON),"reserved keys rejected");
 Check(fp::BindableKey('P')&&fp::BindableKey(VK_F8),"letters/function keys supported");
 wchar_t directory[MAX_PATH]{},path[MAX_PATH]{};
 Check(GetTempPathW(MAX_PATH,directory)!=0,"temporary directory");
 Check(GetTempFileNameW(directory,L"acu",0,path)!=0,"isolated test file");
 Check(WritePrivateProfileStringW(L"Camera",L"OtherModSetting",L"preserve",path)!=FALSE,"seed unrelated setting");
 s.key=VK_F8;s.fov=82;s.hideHead=false;s.automatic=true;
 s.followParkour=false;s.followLeap=false;s.followMotion=false;s.combat=false;s.combatHeight=-0.12f;s.combatForward=-0.04f;s.combatFov=85;
 s.gait=false;
 Check(fp::SaveSettings(path,s),"save succeeds");
 Check(GetPrivateProfileIntW(L"Camera",L"FollowParkourSpins",1,path)==0,"parkour tracking disabled persists");
 Check(GetPrivateProfileIntW(L"Camera",L"ToggleKey",0,path)==VK_F8,"key persisted as integer");
 Check(GetPrivateProfileIntW(L"Camera",L"HideHead",1,path)==0,"boolean persisted");
 Check(GetPrivateProfileIntW(L"Camera",L"AutoThirdPerson",0,path)==1,"fallback persisted");
 Check(GetPrivateProfileIntW(L"Camera",L"CombatFraming",1,path)==0,"combat flag persists");
 Check(GetPrivateProfileIntW(L"Camera",L"FollowCombatMotion",1,path)==0,"motion option persists");
 Check(GetPrivateProfileIntW(L"Camera",L"FollowLeapOfFaith",1,path)==0,"leap setting persisted");
 Check(GetPrivateProfileIntW(L"Camera",L"CameraGait",1,path)==0,"gait camera persists");
 wchar_t value[64]{};
 GetPrivateProfileStringW(L"Camera",L"CombatEyeHeight",L"",value,64,path);
 Check(std::abs(wcstof(value,nullptr)+0.12f)<0.001f,"combat height persists");
 GetPrivateProfileStringW(L"Camera",L"CombatForward",L"",value,64,path);
 Check(std::abs(wcstof(value,nullptr)+0.04f)<0.001f,"combat forward persists");
 Check(GetPrivateProfileIntW(L"Camera",L"CombatVerticalFOV",0,path)==85,"combat FOV persists");
 GetPrivateProfileStringW(L"Camera",L"VerticalFOV",L"",value,64,path);
 Check(std::abs(wcstof(value,nullptr)-82)<0.001f,"float round trip");
 GetPrivateProfileStringW(L"Camera",L"OtherModSetting",L"",value,64,path);
 Check(wcscmp(value,L"preserve")==0,"unrelated setting preserved");
 s.combatHeight=-0.3f;
 Check(!fp::SaveSettings(path,s),"unsafe combat offset rejected");
 s.combatHeight=-0.12f;
 s.key=VK_INSERT;
 Check(!fp::SaveSettings(path,s),"invalid settings rejected");
 Check(GetPrivateProfileIntW(L"Camera",L"ToggleKey",0,path)==VK_F8,"failed save preserves original");
 s.key='V';s.fov=0;
 Check(fp::SaveSettings(path,s),"native FOV permitted");
 Check(GetPrivateProfileIntW(L"Camera",L"VerticalFOV",-1,path)==0,"native FOV persists");
 s.followParkour=true;s.followMotion=true;s.gait=true;
 Check(fp::SaveSettings(path,s),"roll tracking can be enabled again");
 Check(GetPrivateProfileIntW(L"Camera",L"FollowParkourSpins",0,path)==1,"parkour tracking enabled persists");
 Check(GetPrivateProfileIntW(L"Camera",L"CameraGait",0,path)==1,"gait camera can be re-enabled");
 Check(GetPrivateProfileIntW(L"Camera",L"FollowCombatMotion",0,path)==1,"enabled roll tracking persists");
 Check(GetFileAttributesW((std::wstring(path)+L".tmp").c_str())==INVALID_FILE_ATTRIBUTES,"no temporary file left");
 DeleteFileW(path);
 std::puts("Settings validation, key restrictions, persistence and config preservation passed.");
}
