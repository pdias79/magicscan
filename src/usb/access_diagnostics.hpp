#pragma once
#include <string>
namespace magicscan {
enum class ScannerIssue {
    None,
    NotFound,
    AccessDenied,
    DeviceBusy,
    Disconnected,
    Other
};
// Classification is based on Windows error codes and probe diagnostics;
// it never requests extra permissions or sends USB/SCSI commands.
ScannerIssue classify_scanner_issue(const std::string& detail);
const wchar_t* scanner_issue_status(ScannerIssue issue);
std::string scanner_issue_advice(ScannerIssue issue);
}
