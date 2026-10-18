#include "export_formats.hpp"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace magicscan {
namespace {
bool jpeg_data(const std::vector<uint8_t>& pixels, int width, int height,
               std::vector<uint8_t>& encoded) {
    if (width <= 0 || height <= 0 ||
        static_cast<uint64_t>(width) * height * 3 != pixels.size()) return false;
    Gdiplus::GdiplusStartupInput startup{};
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok)
        return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bitmap(width, height, PixelFormat24bppRGB);
        Gdiplus::Rect rect(0, 0, width, height);
        Gdiplus::BitmapData locked{};
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite,
                            PixelFormat24bppRGB, &locked) == Gdiplus::Ok) {
            for (int y = 0; y < height; ++y)
                std::memcpy(static_cast<uint8_t*>(locked.Scan0) +
                            static_cast<ptrdiff_t>(y) * locked.Stride,
                            pixels.data() + static_cast<size_t>(y) * width * 3,
                            static_cast<size_t>(width) * 3);
            bitmap.UnlockBits(&locked);
            UINT count = 0, size = 0;
            if (Gdiplus::GetImageEncodersSize(&count, &size) == Gdiplus::Ok && size) {
                std::vector<uint8_t> codecs(size);
                auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(codecs.data());
                if (Gdiplus::GetImageEncoders(count, size, info) == Gdiplus::Ok) {
                    for (UINT i = 0; i < count; ++i) {
                        if (!info[i].MimeType || wcscmp(info[i].MimeType, L"image/jpeg")) continue;
                        IStream* stream = nullptr;
                        if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) break;
                        ULONG quality = 90;
                        Gdiplus::EncoderParameters params{};
                        params.Count = 1;
                        params.Parameter[0].Guid = Gdiplus::EncoderQuality;
                        params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
                        params.Parameter[0].NumberOfValues = 1;
                        params.Parameter[0].Value = &quality;
                        if (bitmap.Save(stream, &info[i].Clsid, &params) == Gdiplus::Ok) {
                            HGLOBAL memory = nullptr;
                            STATSTG stats{};
                            if (GetHGlobalFromStream(stream, &memory) == S_OK &&
                                stream->Stat(&stats, STATFLAG_NONAME) == S_OK &&
                                stats.cbSize.QuadPart <= 0xFFFFFFFFull) {
                                const auto* data = static_cast<const uint8_t*>(GlobalLock(memory));
                                if (data) {
                                    encoded.assign(data, data + stats.cbSize.QuadPart);
                                    GlobalUnlock(memory);
                                    ok = !encoded.empty();
                                }
                            }
                        }
                        stream->Release();
                        break;
                    }
                }
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}
bool write_file(const std::wstring& path, const std::vector<uint8_t>& data) {
    std::ofstream out{std::filesystem::path(path), std::ios::binary | std::ios::trunc};
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return out.good();
}
} // namespace

bool export_jpeg(const std::wstring& path, const std::vector<uint8_t>& bgr,
                 int width, int height) {
    std::vector<uint8_t> data;
    return jpeg_data(bgr, width, height, data) && write_file(path, data);
}

bool export_pdf(const std::wstring& path, const std::vector<uint8_t>& bgr,
                int width, int height) {
    std::vector<uint8_t> jpeg;
    if (!jpeg_data(bgr, width, height, jpeg)) return false;
    // Single A4 page. Scale without cropping or stretching the original.
    const double paper_w = width > height ? 842.0 : 595.0;
    const double paper_h = width > height ? 595.0 : 842.0;
    const double scale = std::min((paper_w - 36.0) / width,
                                  (paper_h - 36.0) / height);
    const double draw_w = width * scale, draw_h = height * scale;
    std::ostringstream content;
    content.imbue(std::locale::classic());
    content << std::fixed << std::setprecision(3)
            << "q\n" << draw_w << " 0 0 " << draw_h << ' '
            << (paper_w - draw_w) / 2 << ' ' << (paper_h - draw_h) / 2
            << " cm\n/Im0 Do\nQ\n";
    const std::string commands = content.str();
    std::ofstream out{std::filesystem::path(path), std::ios::binary | std::ios::trunc};
    if (!out) return false;
    std::vector<std::streamoff> offsets(6);
    out << "%PDF-1.4\n";
    auto object = [&](int id, const std::string& body) {
        offsets[id] = static_cast<std::streamoff>(out.tellp());
        out << id << " 0 obj\n" << body << "\nendobj\n";
    };
    object(1, "<< /Type /Catalog /Pages 2 0 R >>");
    object(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    std::ostringstream page;
    page.imbue(std::locale::classic());
    page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
         << paper_w << ' ' << paper_h
         << "] /Resources << /XObject << /Im0 4 0 R >> >> /Contents 5 0 R >>";
    object(3, page.str());
    offsets[4] = static_cast<std::streamoff>(out.tellp());
    out << "4 0 obj\n<< /Type /XObject /Subtype /Image /Width " << width
        << " /Height " << height
        << " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length "
        << jpeg.size() << " >>\nstream\n";
    out.write(reinterpret_cast<const char*>(jpeg.data()),
              static_cast<std::streamsize>(jpeg.size()));
    out << "\nendstream\nendobj\n";
    offsets[5] = static_cast<std::streamoff>(out.tellp());
    out << "5 0 obj\n<< /Length " << commands.size() << " >>\nstream\n"
        << commands << "endstream\nendobj\n";
    const auto xref = static_cast<std::streamoff>(out.tellp());
    out << "xref\n0 6\n0000000000 65535 f \n";
    for (int i = 1; i <= 5; ++i)
        out << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
    out << "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"
        << xref << "\n%%EOF\n";
    return out.good();
}
} // namespace magicscan
