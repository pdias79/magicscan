#include "../core/monitor.hpp"
#include "live_preview.hpp"
#include "resource.hpp"
#include "about.hpp"
#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace {
constexpr UINT kTrayCallback = WM_APP + 1;
constexpr UINT kWorkerEnded = WM_APP + 2;
constexpr UINT kCommandToggle = 1001;
constexpr UINT kCommandOpen = 1002;
constexpr UINT kCommandQuit = 1003;
constexpr UINT kCommandAbout = 1004;
constexpr UINT kTrayId = 1;
struct App {
    HWND window = nullptr;
    NOTIFYICONDATAW tray{};
    std::atomic_bool stop{false};
    std::atomic_bool running{false};
    std::thread worker;
    std::wstring scans;
};
App app;

void update_tip() {
    app.tray.uFlags = NIF_TIP;
    const wchar_t* text = app.running ? L"MagicScan - surveillance active" : L"MagicScan - surveillance arrêtée";
    lstrcpynW(app.tray.szTip, text, ARRAYSIZE(app.tray.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &app.tray);
}
void stop_worker() {
    app.stop = true;
    if (app.worker.joinable()) app.worker.join();
    app.running = false;
    update_tip();
}
void start_worker() {
    if (app.running) return;
    if (app.worker.joinable()) app.worker.join();
    app.stop = false;
    app.running = true;
    update_tip();
    app.worker = std::thread([] {
        magicscan::ScanEvents events{};
        events.on_start = [] { magicscan::live_preview_start(); };
        events.on_band = [](const std::vector<uint8_t>& pixels, uint32_t width, uint32_t lines) {
            magicscan::live_preview_band(pixels,width,lines);
        };
        events.on_complete = [](const std::wstring& file) {
            magicscan::live_preview_complete(file);
        };
        const int code = magicscan::monitor_scans(app.scans, true, &app.stop, &events);
        if (!app.stop && code != 0)
            PostMessageW(app.window, kWorkerEnded, static_cast<WPARAM>(code), 0);
    });
}
void menu(HWND hwnd) {
    HMENU m = CreatePopupMenu();
    if (!m) return;
    AppendMenuW(m, MF_STRING, kCommandToggle,
                app.running ? L"Mettre en pause" : L"Reprendre la surveillance");
    AppendMenuW(m, MF_STRING, kCommandOpen, L"Ouvrir le dossier des scans");
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
    std::error_code ec;
    std::filesystem::create_directories(app.scans, ec);
    if (ec) {
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
