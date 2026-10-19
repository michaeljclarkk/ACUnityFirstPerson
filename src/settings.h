#pragma once
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <string>
namespace fp {
struct Settings {
 int key=VK_OEM_PERIOD;
 float fov=70,eyeHeight=0.12f,forward=0.16f,smoothing=0.07f,padding=0.12f,radius=0.16f;
 bool hideHead=true,collision=true,automatic=false;
 bool combat=true,followMotion=true,followLeap=true,followParkour=true;
 bool gait=true;
 float combatHeight=-0.06f,combatForward=0.02f,combatFov=80.0f;
};
inline bool BindableKey(int k) {
 return (k>='A'&&k<='Z')||(k>='0'&&k<='9')||(k>=VK_F1&&k<=VK_F12)||
 (k>=VK_NUMPAD0&&k<=VK_DIVIDE)||k==VK_SPACE||k==VK_TAB||k==VK_RETURN||
 k==VK_BACK||k==VK_DELETE||k==VK_HOME||k==VK_END||k==VK_PRIOR||k==VK_NEXT||
 k==VK_OEM_PERIOD||(k>=VK_LEFT&&k<=VK_DOWN);
}
inline bool Range(float v,float low,float high){return std::isfinite(v)&&v>=low&&v<=high;}
inline bool ValidSettings(const Settings& s) {
 return BindableKey(s.key)&&(s.fov==0||Range(s.fov,40,100))&&
 Range(s.eyeHeight,-0.2f,0.4f)&&Range(s.forward,-0.1f,0.3f)&&
 Range(s.smoothing,0.01f,0.3f)&&Range(s.padding,0.05f,0.3f)&&Range(s.radius,0.03f,0.25f)&&Range(s.combatHeight,-0.2f,0.2f)&&
 Range(s.combatForward,-0.1f,0.3f)&&Range(s.combatFov,40,100);
}
// Preserve unrelated settings; replace the original only after every write succeeds.
inline bool SaveSettings(const wchar_t* path,const Settings& s) {
 if(!ValidSettings(s))return false;
 const std::wstring temporary=std::wstring(path)+L".tmp";
 const DWORD attributes=GetFileAttributesW(path);
 if(attributes!=INVALID_FILE_ATTRIBUTES) {
  if(!CopyFileW(path,temporary.c_str(),FALSE))return false;
 } else {
  const DWORD error=GetLastError();
  if(error!=ERROR_FILE_NOT_FOUND)return false;
  HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  CloseHandle(file);
 }
 bool ok=true;
 const auto write=[&](const wchar_t* name,float value) {
  wchar_t text[48]{};swprintf_s(text,L"%.6f",static_cast<double>(value));
  if(!WritePrivateProfileStringW(L"Camera",name,text,temporary.c_str()))ok=false;
 };

 wchar_t key[16]{};swprintf_s(key,L"%d",s.key);
 if(!WritePrivateProfileStringW(L"Camera",L"ToggleKey",key,temporary.c_str()))ok=false;
 write(L"VerticalFOV",s.fov);write(L"HeadEyeHeight",s.eyeHeight);write(L"HeadForward",s.forward);
 write(L"HeadSmoothingHalfLife",s.smoothing);write(L"CollisionPadding",s.padding);write(L"CollisionRadius",s.radius);
 const auto flag=[&](const wchar_t* name,bool value){if(!WritePrivateProfileStringW(L"Camera",name,value?L"1":L"0",temporary.c_str()))ok=false;};
 flag(L"FollowParkourSpins",s.followParkour);
 flag(L"FollowLeapOfFaith",s.followLeap);
 flag(L"FollowCombatMotion",s.followMotion);
 flag(L"CombatFraming",s.combat);
 write(L"CombatEyeHeight",s.combatHeight);write(L"CombatForward",s.combatForward);write(L"CombatVerticalFOV",s.combatFov);
 flag(L"HideHead",s.hideHead);flag(L"CollisionGuard",s.collision);flag(L"AutoThirdPerson",s.automatic);
 flag(L"CameraGait",s.gait);
 WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
 if(ok)ok=MoveFileExW(temporary.c_str(),path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
 if(!ok)DeleteFileW(temporary.c_str());
 return ok;
}
}
