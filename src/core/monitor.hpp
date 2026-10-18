#pragma once
#include <atomic>
#include <string>
namespace magicscan {
int monitor_scans(const std::wstring& directory, bool clipboard,
                  std::atomic_bool* stop = nullptr);
}
