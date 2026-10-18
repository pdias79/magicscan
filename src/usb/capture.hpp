#pragma once
#include <string>
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>
namespace magicscan {
// Experimental: wait for a new image band and save its RGB pixels to PNG.
// No vendor OUT commands or writes to scanner storage.
// Experimental multi-strip acquisition: scan button starts/stops a session.
// Saves assembled PNG only after draining pending strips.
// Scanner events carry decoded BGR scanlines, never Win32 GUI objects.
struct ScanEvents {
    std::function<void()> on_start;
    // Optional progress heartbeat for cancel/disconnect checks while idle.
    std::function<void()> on_tick;
    // Invoked when a scanner handle is successfully opened; GUI may update status.
    std::function<void()> on_connected;
    // Monitoring reports a dropped scanner until the next successful reopen.
    std::function<void()> on_disconnected;
    // Classified error codes only; USB remains independent of Windows GUI.
    std::function<void(int)> on_issue;
    std::function<void(const std::vector<uint8_t>&, uint32_t, uint32_t)> on_band;
    std::function<void(const std::wstring&)> on_complete;
};
bool capture_page(const std::wstring& output_png, std::string& error,
                  bool wait_for_start_forever = false,
                  const std::atomic_bool* stop = nullptr,
                  const ScanEvents* events = nullptr);
bool capture_first_band(const std::wstring& output_png, std::string& error);
}
