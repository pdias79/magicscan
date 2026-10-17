#include "usb/probe.hpp"
#include "usb/capture.hpp"
#include "gui/preview.hpp"
#include <windows.h>
#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    const std::wstring arg = argc > 1 ? argv[1] : L"--help";
    if (arg == L"--help" || arg == L"-h") {
        std::cout << "MagicScan 0.2 (Windows-native prototype)\n"
                  << "Usage:\n"
                  << "  magicscan --probe               Read-only NOVA identification\n"
                  << "  magicscan --preview image.png  Native image viewer/crop tool\n"
                  << "  magicscan --capture-band out.png  Read one fresh scanner strip (experimental)\n"
                  << "Viewer: left drag to select, right drag to pan, wheel to zoom,\n"
                  << "Ctrl+C to copy selection, Ctrl+S to save PNG, Ctrl+A to select all.\n"
                  << "Full-page scanning is not implemented in this C++ build yet.\n";
        return 0;
    }
    if (arg == L"--preview" && argc == 3) {
        // Windows provides Unicode command-line arguments directly.
        // No locale-dependent ANSI/UTF-8 conversion is needed.
        return magicscan::show_preview(argv[2]);
    }
    if (arg == L"--capture-band" && argc == 3) {
        std::string error;
        if (!magicscan::capture_first_band(argv[2], error)) {
            std::cerr << "Band capture failed: " << error << "\n";
            return 1;
        }
        std::cout << "Saved first image band to PNG.\n";
        return 0;
    }
    if (arg != L"--probe" || argc != 2) {
        std::cerr << "Unknown option. Try --help\n";
        return 2;
    }
    auto result = magicscan::probe_scanner();
    if (!result.found) {
        std::cerr << "Scanner not found/openable: " << result.error << "\n";
        return 1;
    }
    std::cout << "Device: " << result.path << "\n";
    if (!result.nova) {
        std::cerr << "NOVA identification failed: " << result.error << "\n";
        return 1;
    }
    std::cout << "NOVA OK. Read-only SCSI pass-through succeeded.\n";
    return 0;
}
