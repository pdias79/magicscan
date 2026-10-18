#pragma once
#include <string>
namespace magicscan {
// Console-based continuous scan watcher. Retains the current access requirements.
int monitor_scans(const std::wstring& directory, bool clipboard);
}
