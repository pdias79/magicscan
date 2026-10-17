#include "usb/probe.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string arg = argc > 1 ? argv[1] : "--help";
    if (arg == "--help" || arg == "-h") {
        std::cout << "MagicScan 0.1 (USB discovery prototype)\n"
                  << "Usage: magicscan --probe\n"
                  << "The probe only sends the read-only NOVA identification command.\n";
        return 0;
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
