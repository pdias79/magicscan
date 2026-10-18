#include "monitor.hpp"
#include "../usb/capture.hpp"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace magicscan {
namespace {
namespace fs = std::filesystem;

std::wstring unique_filename(const fs::path& directory) {
    SYSTEMTIME current{};
    GetLocalTime(&current);
    std::wostringstream stem;
    stem << L"scan_" << std::setfill(L'0')
         << std::setw(4) << current.wYear
         << std::setw(2) << current.wMonth
         << std::setw(2) << current.wDay << L"_"
         << std::setw(2) << current.wHour
         << std::setw(2) << current.wMinute
         << std::setw(2) << current.wSecond << L"_"
         << std::setw(3) << current.wMilliseconds;
    for (unsigned n = 0; n < 10000; ++n) {
        const std::wstring suffix = n ? L"_" + std::to_wstring(n) : L"";
        const fs::path file = directory / (stem.str() + suffix + L".png");
        std::error_code ec;
        if (!fs::exists(file, ec) && !ec) return file.wstring();
    }
    return {};
}

bool copy_png_to_clipboard(const std::wstring& file, std::string& error) {
    Gdiplus::GdiplusStartupInput startup{};
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) {
        error = "GDI+ initialization failed";
        return false;
    }
    HBITMAP bitmap = nullptr;
    {
        Gdiplus::Bitmap image(file.c_str());
        if (image.GetLastStatus() == Gdiplus::Ok &&
            image.GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &bitmap) != Gdiplus::Ok) {
            bitmap = nullptr;
        }
    }
    Gdiplus::GdiplusShutdown(token);
    if (!bitmap) {
        error = "Could not load saved PNG as a Windows bitmap";
        return false;
    }

    BITMAP dimensions{};
    if (!GetObjectW(bitmap, sizeof(dimensions), &dimensions) ||
        dimensions.bmWidth <= 0 || dimensions.bmHeight <= 0) {
        error = "Cannot determine bitmap dimensions";
        DeleteObject(bitmap);
        return false;
    }
    const uint64_t stride = (uint64_t(dimensions.bmWidth) * 3 + 3) & ~uint64_t(3);
    const uint64_t bytes = stride * uint64_t(dimensions.bmHeight);
    if (bytes > std::numeric_limits<SIZE_T>::max() - sizeof(BITMAPINFOHEADER) ||
        bytes > std::numeric_limits<DWORD>::max()) {
        error = "Bitmap too large for CF_DIB clipboard transfer";
        DeleteObject(bitmap);
        return false;
    }

    // CF_DIB is a BITMAPINFOHEADER followed by bottom-up, padded BGR pixels.
    // The Windows clipboard owns this GMEM_MOVEABLE block after SetClipboardData.
    HGLOBAL dib = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) +
                                             static_cast<SIZE_T>(bytes));
    if (!dib) {
        error = "Could not allocate clipboard image memory";
        DeleteObject(bitmap);
        return false;
    }
    auto* memory = static_cast<uint8_t*>(GlobalLock(dib));
    if (!memory) {
        error = "Could not lock clipboard image memory";
        GlobalFree(dib);
        DeleteObject(bitmap);
        return false;
    }
    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = dimensions.bmWidth;
    header.biHeight = dimensions.bmHeight;
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(bytes);
    std::memcpy(memory, &header, sizeof(header));
    HDC dc = GetDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader = header;
    const int lines = dc ? GetDIBits(dc, bitmap, 0,
        static_cast<UINT>(dimensions.bmHeight), memory + sizeof(header),
        &info, DIB_RGB_COLORS) : 0;
    if (dc) ReleaseDC(nullptr, dc);
    GlobalUnlock(dib);
    DeleteObject(bitmap);
    if (lines != dimensions.bmHeight) {
        error = "GetDIBits could not convert the image to 24-bit DIB";
        GlobalFree(dib);
        return false;
    }

    // Never use OpenClipboard(nullptr) followed by EmptyClipboard: that
    // relinquishes clipboard ownership. Create our own temporary owner HWND.
    HWND owner = CreateWindowExW(0, L"STATIC", L"MagicScan Clipboard",
                                WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!owner) {
        error = "Cannot create clipboard owner window: " + std::to_string(GetLastError());
        GlobalFree(dib);
        return false;
    }
    bool copied = false;
    if (!OpenClipboard(owner)) {
        error = "OpenClipboard failed: Windows error " + std::to_string(GetLastError());
    } else {
        if (!EmptyClipboard()) {
            error = "EmptyClipboard failed: Windows error " + std::to_string(GetLastError());
        } else if (!SetClipboardData(CF_DIB, dib)) {
            error = "SetClipboardData(CF_DIB) failed: Windows error " +
                    std::to_string(GetLastError());
        } else {
            dib = nullptr;  // Clipboard owns the eager DIB data now.
            copied = IsClipboardFormatAvailable(CF_DIB) != 0;
            if (!copied) error = "CF_DIB not available after SetClipboardData";
        }
        CloseClipboard();
    }
    DestroyWindow(owner);
    if (dib) GlobalFree(dib);
    return copied;
}
} // namespace

int monitor_scans(const std::wstring& directory, bool clipboard) {
    if (directory.empty()) {
        std::cerr << "Output directory must not be empty.\n";
        return 2;
    }
    const fs::path folder(directory);
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec || !fs::is_directory(folder, ec)) {
        std::cerr << "Output folder cannot be created or accessed.\n";
        return 2;
    }
    std::wcout << L"Monitoring scans. Saving PNG files to: " << folder.wstring() << L"\n";
    std::cout << "Clipboard: " << (clipboard ? "enabled" : "disabled") << "\n";
    std::cout << "Leave this terminal open; press Ctrl+C to stop.\n" << std::flush;

    while (true) {
        const std::wstring output = unique_filename(folder);
        if (output.empty()) {
            std::cerr << "Cannot assign a unique scan filename.\n";
            return 1;
        }
        std::string error;
        // Wait indefinitely for the next first SCAN press; a started scan still has
        // a finite safety deadline in capture_page.
        if (!capture_page(output, error, true)) {
            std::cerr << "Monitoring interrupted: " << error << "\n";
            return 1;
        }
        std::wcout << L"Saved: " << output << L"\n" << std::flush;
        if (clipboard) {
            if (copy_png_to_clipboard(output, error))
                std::cout << "CF_DIB image placed on Windows clipboard.\n" << std::flush;
            else
                std::cerr << "Scan saved, but clipboard copy failed: " << error << "\n";
        }
        std::cout << "Waiting for next scan.\n" << std::flush;
    }
}
} // namespace magicscan
