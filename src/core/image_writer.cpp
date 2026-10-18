#include "image_writer.hpp"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <vector>

namespace magicscan {
namespace {
constexpr uint32_t kWidth = 2592;
constexpr uint32_t kLines = 192;
bool png_codec(CLSID& clsid) {
    UINT count=0, size=0;
    if (Gdiplus::GetImageEncodersSize(&count,&size) != Gdiplus::Ok || !size) return false;
    std::vector<uint8_t> storage(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if (Gdiplus::GetImageEncoders(count,size,codecs) != Gdiplus::Ok) return false;
    for (UINT i=0;i<count;++i) {
        if (codecs[i].MimeType && std::wcscmp(codecs[i].MimeType,L"image/png")==0) {
            clsid=codecs[i].Clsid; return true;
        }
    }
    return false;
}
} // namespace
bool write_png(const std::wstring& file, const std::vector<uint8_t>& planar, std::string& error) {
    Gdiplus::GdiplusStartupInput input{};
    ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&input,nullptr) != Gdiplus::Ok) {
        error="Cannot initialize GDI+"; return false;
    }
    bool success=false;
    {
        Gdiplus::Bitmap bitmap(kWidth,kLines,PixelFormat24bppRGB);
        Gdiplus::Rect rect(0,0,kWidth,kLines);
        Gdiplus::BitmapData bits{};
        if (bitmap.LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat24bppRGB,&bits)==Gdiplus::Ok) {
            for (uint32_t y=0;y<kLines;++y) {
                auto* dst = static_cast<uint8_t*>(bits.Scan0) + static_cast<ptrdiff_t>(y)*bits.Stride;
                const auto* row = planar.data() + static_cast<size_t>(y)*kWidth*3;
                for (uint32_t x=0;x<kWidth;++x) {
                    uint32_t source=kWidth-1-x;
                    dst[x*3+0]=row[kWidth*2+source]; // BGR expected by GDI+
                    dst[x*3+1]=row[kWidth+source];
                    dst[x*3+2]=row[source];
                }
            }
            bitmap.UnlockBits(&bits);
            CLSID clsid{};
            if (png_codec(clsid) && bitmap.Save(file.c_str(),&clsid)==Gdiplus::Ok) success=true;
            else error="Failed to encode or save PNG";
        } else error="GDI+ LockBits failed";
    }
    Gdiplus::GdiplusShutdown(token);
    return success;
}

bool write_page_png(const std::wstring& file, const std::vector<uint8_t>& pixels,
                    size_t bands, std::string& error) {
    if (bands == 0 || bands > static_cast<size_t>(std::numeric_limits<INT>::max() / kLines)) {
        error = "Invalid page height"; return false;
    }
    const INT height = static_cast<INT>(bands * kLines);
    Gdiplus::GdiplusStartupInput input{};
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        error = "Cannot initialize GDI+"; return false;
    }
    bool saved = false;
    {
        Gdiplus::Bitmap bitmap(kWidth, height, PixelFormat24bppRGB);
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            error = "Unable to allocate full-page bitmap";
        } else {
            Gdiplus::Rect rect(0, 0, kWidth, height);
            Gdiplus::BitmapData data{};
            if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite,
                                PixelFormat24bppRGB, &data) == Gdiplus::Ok) {
                const size_t stride = static_cast<size_t>(kWidth) * 3;
                for (INT y = 0; y < height; ++y) {
                    uint8_t* destination = static_cast<uint8_t*>(data.Scan0) +
                                           static_cast<ptrdiff_t>(y) * data.Stride;
                    std::memcpy(destination, pixels.data() + static_cast<size_t>(y) * stride, stride);
                }
                bitmap.UnlockBits(&data);
                CLSID encoder{};
                if (png_codec(encoder) && bitmap.Save(file.c_str(), &encoder) == Gdiplus::Ok)
                    saved = true;
                else error = "Could not encode or save page PNG";
            } else error = "Could not lock full-page bitmap";
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return saved;
}
} // namespace magicscan
