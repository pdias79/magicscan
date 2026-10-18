#pragma once
#include <windows.h>
namespace magicscan {
// GUI-only, nonmodal About window; reuses the app icon and CMake version.
void show_about(HWND owner);
}
