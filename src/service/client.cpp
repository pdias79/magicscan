#include "client.hpp"
#include "protocol.hpp"
#include <windows.h>
#include <iostream>

namespace magicscan::service {
int query_status() {
    // Avoid indefinite waits if the service is stopping or stalled.
    if (!WaitNamedPipeW(kPipePath, 2000)) {
        std::cerr << "Service MagicScan USB indisponible (erreur Windows "
                  << GetLastError() << ").\n";
        return 2;
    }
    const HANDLE pipe = CreateFileW(kPipePath, GENERIC_READ, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Connexion au service refusee (erreur Windows "
                  << GetLastError() << ").\n";
        return 2;
    }
    char result = '?';
    DWORD read = 0;
    const BOOL ok = ReadFile(pipe, &result, 1, &read, nullptr);
    CloseHandle(pipe);
    if (!ok || read != 1 || (result != kReady && result != kUnavailable)) {
        std::cerr << "Reponse inattendue du service MagicScan USB.\n";
        return 2;
    }
    if (result == kReady) {
        std::cout << "Service MagicScan USB : scanner identifie (NOVA OK).\n";
        return 0;
    }
    std::cout << "Service MagicScan USB : scanner absent ou inaccessible.\n";
    return 1;
}
}
