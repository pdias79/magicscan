#pragma once
#include <atomic>
#include <string>
#include "../usb/capture.hpp"

namespace magicscan::service {
// -1: service unavailable; 0: stopped by user; 1: IPC lost/error.
// The caller owns all PNG output, clipboard and GUI operations.
int monitor_remote(const std::wstring& directory, bool auto_save,
                    std::atomic_bool* stop, const ScanEvents* events);
}
