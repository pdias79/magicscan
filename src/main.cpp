#include "usb/probe.hpp"
#include "gui/preview.hpp"
#include <windows.h>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string arg = argc > 1 ? argv[1] : "--help";
    if (arg == "--help" || arg == "-h") {
        std::cout << "MagicScan 0.2 (Windows-native prototype)\n"
                  << "Usage:\n"
                  << "  magicscan --probe               Read-only NOVA identification\n"
                  << "  magicscan --preview image.png  Native image viewer/crop tool\n"
                  << "Viewer: left drag to select, right drag to pan, wheel to zoom,\n"
                  << "Ctrl+C to copy selection, Ctrl+S to save PNG, Ctrl+A to select all.\n"
                  << "Continuous scanning is not implemented in this C++ build yet.\n";
        return 0;
    }
    if (arg == "--preview" && argc == 3) {
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[2], -1, nullptr, 0);
        if (!length) { std::cerr << "Invalid UTF-8 image path\n"; return 2; }
        std::wstring path(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[2], -1, path.data(), length);
        return magicscan::show_preview(path);
    }
    if (arg != "--probe" || argc != 2) {
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
