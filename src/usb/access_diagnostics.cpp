#include "access_diagnostics.hpp"
#include <string>
namespace magicscan {
namespace {
bool contains(const std::string& s,const char* term) {
    return s.find(term)!=std::string::npos;
}
}
ScannerIssue classify_scanner_issue(const std::string& detail) {
    if (contains(detail,"Windows error 5") ||
        contains(detail,"Windows error 1314") ||
        contains(detail,"Access is denied"))
        return ScannerIssue::AccessDenied;
    if (contains(detail,"Windows error 32") ||
        contains(detail,"Windows error 33"))
        return ScannerIssue::DeviceBusy;
    if (contains(detail,"Windows error 1167") ||
        contains(detail,"Windows error 21") ||
        contains(detail,"Windows error 1117") ||
        contains(detail,"Windows error 2") ||
        contains(detail,"Windows error 3"))
        return ScannerIssue::Disconnected;
    if (contains(detail,"No USB PhysicalDrive") ||
        contains(detail,"Scanner identification failed: No USB"))
        return ScannerIssue::NotFound;
    return ScannerIssue::Other;
}
const wchar_t* scanner_issue_status(ScannerIssue issue) {
    switch(issue) {
    case ScannerIssue::NotFound: return L"MagicScan - scanner non détecté";
    case ScannerIssue::AccessDenied: return L"MagicScan - accès USB refusé";
    case ScannerIssue::DeviceBusy: return L"MagicScan - scanner occupé";
    case ScannerIssue::Disconnected: return L"MagicScan - scanner déconnecté";
    case ScannerIssue::Other: return L"MagicScan - erreur USB, nouvelle tentative";
    case ScannerIssue::None: return L"MagicScan - scanner connecté";
    }
    return L"MagicScan";
}
std::string scanner_issue_advice(ScannerIssue issue) {
    switch(issue) {
    case ScannerIssue::NotFound:
        return "Scanner absent. Check USB cable, scanner power and Windows Device Manager.";
    case ScannerIssue::AccessDenied:
        return "Windows denied access to the scanner raw device. Try an elevated PowerShell session, and check the device permissions. MagicScan does not change any ACL or security settings.";
    case ScannerIssue::DeviceBusy:
        return "Scanner in use by another program. Close competing scanner software and retry.";
    case ScannerIssue::Disconnected:
        return "The scanner disconnected or its device path changed. Reconnect it; monitoring will retry.";
    case ScannerIssue::Other:
        return "Scanner read failed. Inspect the Windows error code; MagicScan retries automatically.";
    case ScannerIssue::None:
        return "Scanner detected and accessible.";
    }
    return "Unknown scanner condition.";
}
}
