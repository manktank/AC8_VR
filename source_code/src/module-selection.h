#pragma once
#include <filesystem>
#include <system_error>
namespace ac8_modules {
struct Selection {bool core=false,hud=false,targets=false,glow=false,cinematics=false,loading=false,pause=false,plane=false;};
template<class Exists> inline Selection select(Exists exists){
 Selection s{};s.core=exists(L"AC8_VR-Core.lua");if(!s.core)return s;
 s.hud=exists(L"AC8_VR-HUD_Layout.lua");s.targets=exists(L"AC8_VR-Targets.lua");s.glow=exists(L"AC8_VR-HUD_Glow.lua");
 s.cinematics=exists(L"AC8_VR-Cinematics_Hangar_Presentation.lua");s.loading=exists(L"AC8_VR-Loading.lua");
 s.pause=true;s.plane=exists(L"AC8_VR-AircraftPresets.lua");return s;
}
inline Selection from_directory(const std::filesystem::path& root){
 return select([&](const wchar_t* name){std::error_code ec;return std::filesystem::is_regular_file(root/L"scripts"/name,ec)&&!ec;});
}
}
