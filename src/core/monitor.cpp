#include "monitor.hpp"
#include "../usb/capture.hpp"
#include "../gui/clipboard.hpp"
#include <windows.h>
#include <atomic>
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

} // namespace

int monitor_scans(const std::wstring& directory, bool clipboard, std::atomic_bool* stop,
                  const ScanEvents* events) {
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

    while (!stop || !stop->load()) {
        const std::wstring output = unique_filename(folder);
        if (output.empty()) {
            std::cerr << "Cannot assign a unique scan filename.\n";
            return 1;
        }
        std::string error;
        // Wait indefinitely for the next first SCAN press; a started scan still has
        // a finite safety deadline in capture_page.
        if (!capture_page(output, error, true, stop, events)) {
            if (stop && stop->load()) return 0;
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
    return 0;
}
} // namespace magicscan
