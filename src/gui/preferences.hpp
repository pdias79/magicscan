#pragma once
#include <windows.h>
#include "../core/settings.hpp"
namespace magicscan {
constexpr UINT kPreferencesChanged = WM_APP+4;
void show_preferences(HWND owner, const UserSettings& current);
}
