#pragma once
#include <windows.h>

namespace magicscan::service {
// Version 1: fixed, read-only status reply. No arbitrary SCSI commands or paths
// are accepted from unprivileged clients.
inline constexpr wchar_t kServiceName[] = L"MagicScanUsb";
inline constexpr wchar_t kPipePath[] = L"\\\\.\\pipe\\MagicScanUsbStatus-v1";
inline constexpr char kReady = '1';
inline constexpr char kUnavailable = '0';
}
