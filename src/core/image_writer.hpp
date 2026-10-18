#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace magicscan {
bool write_png(const std::wstring& file, const std::vector<uint8_t>& planar, std::string& error);
bool write_page_png(const std::wstring& file, const std::vector<uint8_t>& pixels,
                    size_t bands, std::string& error);
}
