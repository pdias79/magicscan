#include "monitor.hpp"
#include "../usb/capture.hpp"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <chrono>
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
        // Convert the saved PNG to a native bitmap owned by the Windows clipboard.
        Gdiplus::Bitmap image(file.c_str());
        if (image.GetLastStatus() == Gdiplus::Ok &&
            image.GetHBITMAP(Gdiplus::Color(255,255,255,255), &bitmap) != Gdiplus::Ok) {
            bitmap = nullptr;
        }
    }
    Gdiplus::GdiplusShutdown(token);
    if (!bitmap) {
        error = "Could not open the saved PNG for clipboard transfer";
        return false;
    }
    if (!OpenClipboard(nullptr)) {
        error = "OpenClipboard failed: Windows error " + std::to_string(GetLastError());
        DeleteObject(bitmap);
        return false;
    }
    bool ok = false;
    if (EmptyClipboard() && SetClipboardData(CF_BITMAP, bitmap)) {
        bitmap = nullptr; // Windows clipboard now owns this bitmap.
        ok = true;
    } else {
        error = "SetClipboardData failed: Windows error " + std::to_string(GetLastError());
    }
    CloseClipboard();
    if (bitmap) DeleteObject(bitmap);
    return ok;
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
                std::cout << "Image copied to Windows clipboard.\n" << std::flush;
            else
                std::cerr << "Scan saved, but clipboard copy failed: " << error << "\n";
        }
        std::cout << "Waiting for next scan.\n" << std::flush;
    }
}
} // namespace magicscan
