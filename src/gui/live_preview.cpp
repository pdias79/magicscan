#include "live_preview.hpp"
#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace magicscan {
namespace {
struct Event {
    enum class Type { Start, Band, Complete } type;
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    std::wstring file;
};
std::mutex queue_mutex;
std::deque<Event> pending;
HWND dispatcher = nullptr;
HWND preview = nullptr;
std::vector<uint8_t> accumulated;
uint32_t image_width = 0;
uint32_t image_height = 0;
bool completed = false;
std::wstring saved_path;

LRESULT CALLBACK preview_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        if (image_width && image_height && !accumulated.empty()) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(image_width);
            info.bmiHeader.biHeight = -static_cast<LONG>(image_height); // top down
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 24;
            info.bmiHeader.biCompression = BI_RGB;
            const int w = client.right-client.left;
            const int h = client.bottom-client.top;
            const double scale = std::min(static_cast<double>(w)/image_width,
                                          static_cast<double>(h)/image_height);
            const int draw_w = std::max(1, static_cast<int>(image_width*scale));
            const int draw_h = std::max(1, static_cast<int>(image_height*scale));
            SetStretchBltMode(dc, HALFTONE);
            StretchDIBits(dc, (w-draw_w)/2, (h-draw_h)/2, draw_w, draw_h,
                          0, 0, image_width, image_height,
                          accumulated.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        } else {
            DrawTextW(dc, L"Waiting for scan bands...", -1, &client,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        preview = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void ensure_window() {
    if (preview) {
        ShowWindow(preview, SW_SHOW);
        SetForegroundWindow(preview);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"MagicScanLivePreview";
        cls.lpfnWndProc = preview_proc;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&cls) != 0;
    }
    if (!registered) return;
    preview = CreateWindowExW(0, L"MagicScanLivePreview", L"MagicScan - live scan",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
                              nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}
void send(Event event) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        pending.push_back(std::move(event));
    }
    if (dispatcher) PostMessageW(dispatcher, kLivePreviewMessage, 0, 0);
}
} // namespace

void live_preview_initialize(HWND ui_window) { dispatcher = ui_window; }
void live_preview_start() {
    Event e{};
    e.type=Event::Type::Start;
    send(std::move(e));
}
void live_preview_band(const std::vector<uint8_t>& bgr, uint32_t width, uint32_t height) {
    Event e{};
    e.type=Event::Type::Band;
    e.pixels=bgr;
    e.width=width;
    e.height=height;
    send(std::move(e));
}
void live_preview_complete(const std::wstring& file) {
    Event e{};
    e.type=Event::Type::Complete;
    e.file=file;
    send(std::move(e));
}
void live_preview_dispatch(UINT message) {
    if (message != kLivePreviewMessage) return;
    std::deque<Event> events;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        events.swap(pending);
    }
    for (auto& e : events) {
        switch (e.type) {
        case Event::Type::Start:
            accumulated.clear();
            image_width=image_height=0;
            completed=false;
            saved_path.clear();
            ensure_window();
            if (preview) SetWindowTextW(preview,L"MagicScan - scanning...");
            break;
        case Event::Type::Band:
            // 24-bit BGR rows are DWORD-aligned for 2592-pixel scanner strips.
            if (e.width!=2592 || e.height!=192 ||
                e.pixels.size()!=static_cast<size_t>(e.width)*e.height*3)
                break;
            if (image_width && image_width!=e.width) break;
            image_width=e.width;
            image_height+=e.height;
            accumulated.insert(accumulated.end(),e.pixels.begin(),e.pixels.end());
            if (preview) InvalidateRect(preview,nullptr,FALSE);
            break;
        case Event::Type::Complete:
            saved_path=e.file;
            completed=true;
            if (preview) {
                SetWindowTextW(preview,L"MagicScan - scan complete (PNG saved)");
                InvalidateRect(preview,nullptr,FALSE);
            }
            break;
        }
    }
}
} // namespace magicscan
