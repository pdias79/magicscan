#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace magicscan {
bool export_jpeg(const std::wstring& path, const std::vector<uint8_t>& bgr,
                 int width, int height);
bool export_pdf(const std::wstring& path, const std::vector<uint8_t>& bgr,
                int width, int height);
}
