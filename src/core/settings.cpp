#include "settings.hpp"
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <vector>
namespace magicscan {
namespace {
constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunName = L"MagicScan";
}
bool startup_enabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    DWORD type = 0, bytes = 0;
    const LONG result = RegQueryValueExW(key, kRunName, nullptr, &type, nullptr, &bytes);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ && bytes > sizeof(wchar_t);
}
bool set_startup_enabled(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    LONG result = ERROR_SUCCESS;
    if (enabled) {
        std::vector<wchar_t> path(32768);
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!length || length >= path.size()) { RegCloseKey(key); return false; }
        const std::wstring command = L"\\\"" + std::wstring(path.data(), length) + L"\\\"";
        result = RegSetValueExW(key, kRunName, 0, REG_SZ,
             reinterpret_cast<const BYTE*>(command.c_str()),
             static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kRunName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
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
    s.start_with_windows=startup_enabled();
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
