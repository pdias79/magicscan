#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
namespace magicscan {
// GUI-only live view. Worker threads may call these functions; the HWND is
// created and painted exclusively by the tray's UI thread.
void live_preview_initialize(HWND ui_window);
void live_preview_set_options(bool auto_clipboard, int default_gamma);
void live_preview_start();
void live_preview_band(const std::vector<uint8_t>& bgr, uint32_t width, uint32_t height);
void live_preview_complete(const std::wstring& file);
void live_preview_dispatch(UINT message);
constexpr UINT kLivePreviewMessage = WM_APP + 20;
}
