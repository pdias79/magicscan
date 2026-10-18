#include "about.hpp"
#include "resource.hpp"
#include "version.hpp"
#include <shellapi.h>
#include <initializer_list>
#include <string>

namespace magicscan {
namespace {
constexpr int kOpenGithub = 3101;
constexpr int kClose = 3102;
HWND about_window = nullptr;

LRESULT CALLBACK about_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        HICON icon = static_cast<HICON>(LoadImageW(instance,
            MAKEINTRESOURCEW(IDI_MAGICSCAN), IMAGE_ICON, 64, 64, LR_SHARED));
        if (icon) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            HWND image = CreateWindowExW(0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | SS_ICON, 24, 27, 72, 72,
                hwnd, nullptr, instance, nullptr);
            if (image) SendMessageW(image, STM_SETICON, reinterpret_cast<WPARAM>(icon), 0);
        }
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        const std::wstring label = std::wstring(L"MagicScan  —  version ") + MAGICSCAN_VERSION;
        HWND title = CreateWindowExW(0, L"STATIC", label.c_str(),
            WS_CHILD | WS_VISIBLE, 110, 26, 310, 26, hwnd, nullptr, instance, nullptr);
        HWND description = CreateWindowExW(0, L"STATIC",
            L"Numérisation USB et aperçu des documents IRIScan Book.",
            WS_CHILD | WS_VISIBLE, 110, 63, 330, 40, hwnd, nullptr, instance, nullptr);
        HWND github = CreateWindowExW(0, L"STATIC", MAGICSCAN_GITHUB,
            WS_CHILD | WS_VISIBLE, 24, 121, 420, 25, hwnd, nullptr, instance, nullptr);
        HWND open = CreateWindowExW(0, L"BUTTON", L"Ouvrir GitHub",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            162, 164, 140, 36, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpenGithub)),
            instance, nullptr);
        HWND close = CreateWindowExW(0, L"BUTTON", L"Fermer",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            312, 164, 100, 36, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kClose)),
            instance, nullptr);
        for (HWND control : {title, description, github, open, close})
            if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(25, 25, 25));
        return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    }
    case WM_COMMAND:
        if (LOWORD(wp) == kOpenGithub) {
            ShellExecuteW(hwnd, L"open", MAGICSCAN_GITHUB, nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        if (LOWORD(wp) == kClose) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        about_window = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

void show_about(HWND owner) {
    if (about_window) {
        ShowWindow(about_window, SW_SHOW);
        SetForegroundWindow(about_window);
        return;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW cls{};
        cls.hInstance = instance;
        cls.lpszClassName = L"MagicScanAboutWindow";
        cls.lpfnWndProc = about_proc;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        registered = RegisterClassW(&cls) != 0;
    }
    if (!registered) return;
    HWND visible_owner = owner && IsWindowVisible(owner) ? owner : nullptr;
    about_window = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"MagicScanAboutWindow", L"À propos de MagicScan",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 255,
        visible_owner, nullptr, instance, nullptr);
}
} // namespace magicscan
