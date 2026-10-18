#pragma once
#include <string>
namespace magicscan {
bool copy_png_to_clipboard(const std::wstring& file, std::string& error);
}
