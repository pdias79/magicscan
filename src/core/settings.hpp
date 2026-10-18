#pragma once
#include <string>
namespace magicscan {
struct UserSettings {
    bool auto_save = true;
    bool auto_clipboard = true;
    int default_gamma = 100; // 20 to 300, where 100 is 1.00
    std::wstring scan_folder;
};
std::wstring settings_path();
UserSettings load_settings();
bool save_settings(const UserSettings& settings);
}
