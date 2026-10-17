#pragma once
#include <string>
namespace magicscan {
// Experimental: wait for a new image band and save its RGB pixels to PNG.
// No vendor OUT commands or writes to scanner storage.
bool capture_first_band(const std::wstring& output_png, std::string& error);
}
