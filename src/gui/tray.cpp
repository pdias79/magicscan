#include "../core/monitor.hpp"
#include "../service/stream.hpp"
#include "live_preview.hpp"
#include "resource.hpp"
#include "about.hpp"
#include "preferences.hpp"
#include "../core/settings.hpp"
#include "../usb/access_diagnostics.hpp"
#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {
constexpr UINT kTrayCallback = WM_APP + 1;
constexpr UINT kWorkerEnded = WM_APP + 2;
constexpr UINT kScannerConnectionChanged = WM_APP + 3;
constexpr UINT kScannerIssueChanged = WM_APP + 5;
constexpr UINT kCommandToggle = 1001;
constexpr UINT kCommandOpen = 1002;
constexpr UINT kCommandQuit = 1003;
constexpr UINT kCommandAbout = 1004;
constexpr UINT kCommandPreferences = 1005;
constexpr UINT kTrayId = 1;
struct App {
    HWND window = nullptr;
    NOTIFYICONDATAW tray{};
    std::atomic_bool stop{false};
    std::atomic_bool running{false};
    std::atomic_bool connected{false};
    magicscan::ScannerIssue issue = magicscan::ScannerIssue::None;
    std::thread worker;
    std::wstring scans;
    magicscan::UserSettings settings;
};
App app;

void update_tip() {
    app.tray.uFlags = NIF_TIP;
    const wchar_t* text = !app.running ? L"MagicScan - surveillance arrêtée" :
        app.connected ? L"MagicScan - scanner connecté" :
        magicscan::scanner_issue_status(app.issue);
    lstrcpynW(app.tray.szTip, text, ARRAYSIZE(app.tray.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &app.tray);
}
void stop_worker() {
    app.stop = true;
    if (app.worker.joinable()) {
        CancelSynchronousIo(static_cast<HANDLE>(app.worker.native_handle()));
        app.worker.join();
    }
    app.running = false;
    app.connected = false;
    app.issue=magicscan::ScannerIssue::None;
    update_tip();
}
void start_worker() {
    if (app.running) return;
    if (app.worker.joinable()) app.worker.join();
    app.stop = false;
    app.running = true;
    app.connected = false;
    app.issue=magicscan::ScannerIssue::None;
    magicscan::live_preview_set_options(app.settings.auto_clipboard,app.settings.default_gamma);
    update_tip();
    app.worker = std::thread([] {
        magicscan::ScanEvents events{};
        events.on_start = [] { magicscan::live_preview_start(); };
        events.on_connected = [] {
            PostMessageW(app.window,kScannerConnectionChanged,1,0);
        };
        events.on_issue = [](int issue) {
            PostMessageW(app.window,kScannerIssueChanged,static_cast<WPARAM>(issue),0);
        };
        events.on_disconnected = [] {
            PostMessageW(app.window,kScannerConnectionChanged,0,0);
        };
        events.on_band = [](const std::vector<uint8_t>& pixels, uint32_t width, uint32_t lines) {
            magicscan::live_preview_band(pixels,width,lines);
        };
        events.on_complete = [](const std::wstring& file) {
            magicscan::live_preview_complete(file);
        };
        // Preserve the legacy direct path only if the service was absent at
        // startup. Once a service session succeeds, never silently switch to
        // direct PhysicalDrive access (which fails for standard users).
        int remote = magicscan::service::monitor_remote(
            app.scans, app.settings.auto_save, &app.stop, &events);
        if (remote == -1 && !app.stop) {
            const int code = magicscan::monitor_scans(
                app.scans, false, &app.stop, &events, app.settings.auto_save);
            if (!app.stop && code != 0)
                PostMessageW(app.window, kWorkerEnded, static_cast<WPARAM>(code), 0);
            return;
        }
        while (!app.stop && remote != 0) {
            // An interrupted scan is discarded by the stream client. Keep
            // monitoring and reconnect when the service starts again.
            events.on_disconnected();
            for (int i = 0; i < 10 && !app.stop; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (app.stop) break;
            remote = magicscan::service::monitor_remote(
                app.scans, app.settings.auto_save, &app.stop, &events);
        }
    });
}
void menu(HWND hwnd) {
    HMENU m = CreatePopupMenu();
    if (!m) return;
    AppendMenuW(m, MF_STRING, kCommandToggle,
                app.running ? L"Mettre en pause" : L"Reprendre la surveillance");
    AppendMenuW(m, MF_STRING, kCommandOpen, L"Ouvrir le dossier des scans");
    AppendMenuW(m, MF_STRING, kCommandPreferences, L"Paramètres...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kCommandAbout, L"À propos...");
    AppendMenuW(m, MF_STRING, kCommandQuit, L"Quitter");
    POINT p{};
    GetCursorPos(&p);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, p.x, p.y, 0, hwnd, nullptr);
    DestroyMenu(m);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static UINT taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    if (msg == taskbar_created) {
        Shell_NotifyIconW(NIM_ADD, &app.tray);
        update_tip();
        return 0;
    }
    switch (msg) {
    case WM_CREATE:
        app.window = hwnd;
        magicscan::live_preview_initialize(hwnd);
        app.tray.cbSize = sizeof(app.tray);
        app.tray.hWnd = hwnd;
        app.tray.uID = kTrayId;
        app.tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        app.tray.uCallbackMessage = kTrayCallback;
        app.tray.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(IDI_MAGICSCAN), IMAGE_ICON, 0, 0,
            LR_DEFAULTSIZE | LR_SHARED));
        if (!app.tray.hIcon) app.tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        lstrcpynW(app.tray.szTip, L"MagicScan", ARRAYSIZE(app.tray.szTip));
        Shell_NotifyIconW(NIM_ADD, &app.tray);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app.tray.hIcon));
        start_worker();
        return 0;
    case magicscan::kPreferencesChanged:
        stop_worker();
        app.settings=magicscan::load_settings();
        app.scans=app.settings.scan_folder;
        start_worker();
        return 0;
    case kScannerIssueChanged:
        if (app.running && !app.connected) {
            app.issue=static_cast<magicscan::ScannerIssue>(wp);
            update_tip();
        }
        return 0;
    case kScannerConnectionChanged:
        if (app.running) {
            app.connected = (wp != 0);
            if (app.connected) app.issue=magicscan::ScannerIssue::None;
            update_tip();
        }
        return 0;
    case magicscan::kLivePreviewMessage:
        magicscan::live_preview_dispatch(msg);
        return 0;
    case kTrayCallback:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) menu(hwnd);
        else if (lp == WM_LBUTTONDBLCLK) {
            ShellExecuteW(hwnd, L"open", app.scans.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case kCommandToggle:
            if (app.running) stop_worker(); else start_worker();
            return 0;
        case kCommandOpen:
            ShellExecuteW(hwnd, L"open", app.scans.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case kCommandPreferences:
            magicscan::show_preferences(hwnd,app.settings);
            return 0;
        case kCommandAbout:
            magicscan::show_about(hwnd);
            return 0;
        case kCommandQuit:
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case kWorkerEnded:
        stop_worker();
        MessageBoxW(hwnd, L"La surveillance du scanner s’est arrêtée après une erreur.\n"
                         L"Vérifiez la connexion et les autorisations avant de reprendre.",
                    L"MagicScan", MB_ICONWARNING | MB_OK);
        return 0;
    case WM_DESTROY:
        stop_worker();
        Shell_NotifyIconW(NIM_DELETE, &app.tray);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    // Session-scoped named mutex: atomic even when two shortcuts start at once.
    // A second launch never starts the USB worker or creates another tray icon.
    HANDLE single_instance = CreateMutexW(nullptr, FALSE, L"Local\\MagicScanTraySingleInstance");
    if (!single_instance) {
        MessageBoxW(nullptr, L"Impossible de vérifier si MagicScan est déjà lancé.",
                    L"MagicScan", MB_OK | MB_ICONERROR);
        return 1;
    }
    const DWORD mutex_error = GetLastError();
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        // A running instance already owns the tray icon and scanner monitor.
        // Do not display a notification or initialize another instance.
        CloseHandle(single_instance);
        return 0;
    }
    struct MutexGuard {
        HANDLE handle;
        ~MutexGuard() { CloseHandle(handle); }
    } guard{single_instance};

    if (command_line && *command_line) {
        app.scans = command_line;
        if (app.scans.size() >= 2 && app.scans.front() == L'"' &&
            app.scans.back() == L'"')
            app.scans = app.scans.substr(1, app.scans.size()-2);
    } else {
        wchar_t documents[MAX_PATH]{};
        DWORD chars = GetEnvironmentVariableW(L"USERPROFILE", documents, MAX_PATH);
        if (chars && chars < MAX_PATH)
            app.scans = std::wstring(documents) + L"\\Pictures\\MagicScan";
        else
            app.scans = L"C:\\Scans";
    }
    app.settings=magicscan::load_settings();
    if (!app.settings.scan_folder.empty()) app.scans=app.settings.scan_folder;
    else {
        app.settings.scan_folder=app.scans;
        magicscan::save_settings(app.settings);
    }
    std::error_code ec;
    if (app.settings.auto_save) std::filesystem::create_directories(app.scans, ec);
    if (app.settings.auto_save && ec) {
        MessageBoxW(nullptr, L"Impossible de créer le dossier de destination des scans.", L"MagicScan",
                    MB_OK | MB_ICONERROR);
        return 2;
    }
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpszClassName = L"MagicScanTrayWindow";
    cls.lpfnWndProc = window_proc;
    if (!RegisterClassExW(&cls)) return 1;
    HWND hwnd = CreateWindowExW(0, cls.lpszClassName, L"MagicScan - zone de notification", 0,
                                0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
