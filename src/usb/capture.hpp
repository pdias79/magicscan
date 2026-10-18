#pragma once
#include <string>
namespace magicscan {
// Experimental: wait for a new image band and save its RGB pixels to PNG.
// No vendor OUT commands or writes to scanner storage.
// Experimental multi-strip acquisition: scan button starts/stops a session.
// Saves assembled PNG only after draining pending strips.
bool capture_page(const std::wstring& output_png, std::string& error,
                  bool wait_for_start_forever = false);
bool capture_first_band(const std::wstring& output_png, std::string& error);
}
