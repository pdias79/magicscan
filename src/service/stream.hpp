#pragma once
#include <atomic>
#include <string>
#include "../usb/capture.hpp"

namespace magicscan::service {
// True if the service accepted and processed the stream; false on IPC failure.
// The caller owns all PNG output, clipboard and GUI operations.
bool monitor_remote(const std::wstring& directory, bool auto_save,
                    std::atomic_bool* stop, const ScanEvents* events);
}
