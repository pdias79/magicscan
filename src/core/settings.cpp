#include "settings.hpp"
#include <windows.h>
#include <filesystem>
#include <algorithm>
namespace magicscan {
std::wstring settings_path() {
    wchar_t root[MAX_PATH]{};
    DWORD length=GetEnvironmentVariableW(L"APPDATA",root,MAX_PATH);
    if (!length || length>=MAX_PATH) {
        length=GetEnvironmentVariableW(L"USERPROFILE",root,MAX_PATH);
        if (!length || length>=MAX_PATH) return L"magicscan.ini";
    }
    std::filesystem::path folder=std::filesystem::path(root)/L"MagicScan";
    std::error_code ec;
    std::filesystem::create_directories(folder,ec);
    return (folder/L"settings.ini").wstring();
}
UserSettings load_settings() {
    UserSettings s{};
    const std::wstring path=settings_path();
    s.auto_save=GetPrivateProfileIntW(L"Acquisition",L"EnregistrementAutomatique",1,path.c_str())!=0;
    s.auto_clipboard=GetPrivateProfileIntW(L"Acquisition",L"PressePapiers",1,path.c_str())!=0;
    s.default_gamma=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Image",L"Gamma",100,path.c_str())),20,300);
    wchar_t folder[32768]{};
    GetPrivateProfileStringW(L"Acquisition",L"Dossier",L"",folder,32768,path.c_str());
    s.scan_folder=folder;
    return s;
}
bool save_settings(const UserSettings& s) {
    const std::wstring path=settings_path();
    const auto write=[&](const wchar_t* section,const wchar_t* key,const std::wstring& value) {
        return WritePrivateProfileStringW(section,key,value.c_str(),path.c_str())!=FALSE;
    };
    return write(L"Acquisition",L"EnregistrementAutomatique",s.auto_save?L"1":L"0")
        && write(L"Acquisition",L"PressePapiers",s.auto_clipboard?L"1":L"0")
        && write(L"Acquisition",L"Dossier",s.scan_folder)
        && write(L"Image",L"Gamma",std::to_wstring(s.default_gamma));
}
}
