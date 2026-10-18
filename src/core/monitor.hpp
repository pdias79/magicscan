#pragma once
#include <atomic>
#include <string>
#include "../usb/capture.hpp"
namespace magicscan {
int monitor_scans(const std::wstring& directory, bool clipboard,
                  std::atomic_bool* stop = nullptr,
                  const ScanEvents* events = nullptr);
}
