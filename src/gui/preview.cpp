#include "preview.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace magicscan {
namespace {
using namespace Gdiplus;
struct Viewer {
    std::unique_ptr<Bitmap> image;
    double zoom = 1.0;
    double ox = 20, oy = 20;
    POINT begin{}, end{}, last{};
    bool selecting = false, panning = false, has_selection = false;
};
Viewer* viewer(HWND hwnd) { return reinterpret_cast<Viewer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); }
int sx(const Viewer& v, int x) { return static_cast<int>(std::floor((x - v.ox) / v.zoom)); }
int sy(const Viewer& v, int y) { return static_cast<int>(std::floor((y - v.oy) / v.zoom)); }
Rect selection(Viewer& v) {
    int w = static_cast<int>(v.image->GetWidth()), h = static_cast<int>(v.image->GetHeight());
    int x1 = std::clamp(sx(v, v.begin.x), 0, w), x2 = std::clamp(sx(v, v.end.x), 0, w);
    int y1 = std::clamp(sy(v, v.begin.y), 0, h), y2 = std::clamp(sy(v, v.end.y), 0, h);
    return Rect(std::min(x1,x2), std::min(y1,y2), std::abs(x2-x1), std::abs(y2-y1));
}
std::unique_ptr<Bitmap> crop(Viewer& v) {
    Rect r = v.has_selection ? selection(v) : Rect(0, 0, v.image->GetWidth(), v.image->GetHeight());
    if (r.Width <= 0 || r.Height <= 0) return {};
    return std::unique_ptr<Bitmap>(v.image->Clone(r, PixelFormat32bppARGB));
}
void copy_selection(HWND hwnd, Viewer& v) {
    auto bitmap = crop(v);
    if (!bitmap || bitmap->GetLastStatus() != Ok) return;
    HBITMAP hbitmap = nullptr;
    if (bitmap->GetHBITMAP(Color(255,255,255,255), &hbitmap) != Ok) return;
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        if (SetClipboardData(CF_BITMAP, hbitmap)) hbitmap = nullptr;
        CloseClipboard();
    }
    if (hbitmap) DeleteObject(hbitmap);
}
bool png_encoder(CLSID& clsid) {
    UINT count = 0, bytes = 0;
    if (GetImageEncodersSize(&count, &bytes) != Ok || bytes == 0) return false;
    std::vector<BYTE> buffer(bytes);
    auto* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    if (GetImageEncoders(count, bytes, codecs) != Ok) return false;
    for (UINT i = 0; i < count; ++i) {
        if (codecs[i].MimeType && wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            clsid = codecs[i].Clsid; return true;
        }
    }
    return false;
}
void save_selection(HWND hwnd, Viewer& v) {
    auto bitmap = crop(v);
    if (!bitmap || bitmap->GetLastStatus() != Ok) return;
    wchar_t path[MAX_PATH] = L"scan.png";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path; dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return;
    CLSID clsid{};
    if (!png_encoder(clsid) || bitmap->Save(path, &clsid) != Ok)
        MessageBoxW(hwnd, L"Cannot save PNG.", L"MagicScan", MB_ICONERROR);
}
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Viewer* v = viewer(hwnd);
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(
            reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{}; GetClientRect(hwnd, &client);
        HBRUSH bg = CreateSolidBrush(RGB(42, 42, 42)); FillRect(hdc, &client, bg); DeleteObject(bg);
        if (v && v->image) {
            Graphics graphics(hdc);
            graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            graphics.DrawImage(v->image.get(), static_cast<REAL>(v->ox), static_cast<REAL>(v->oy),
                               static_cast<REAL>(v->image->GetWidth() * v->zoom),
                               static_cast<REAL>(v->image->GetHeight() * v->zoom));
            if (v->has_selection || v->selecting) {
                Rect r = selection(*v);
                Pen pen(Color(255, 0, 155, 220), 2);
                graphics.DrawRectangle(&pen, static_cast<REAL>(v->ox + r.X*v->zoom),
                    static_cast<REAL>(v->oy + r.Y*v->zoom),
                    static_cast<REAL>(r.Width*v->zoom), static_cast<REAL>(r.Height*v->zoom));
            }
        }
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_LBUTTONDOWN:
        if (v) { v->begin = v->end = {GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
                 v->selecting = true; v->has_selection = false; SetCapture(hwnd); }
        return 0;
    case WM_RBUTTONDOWN:
        if (v) { v->last = {GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; v->panning = true; SetCapture(hwnd); }
        return 0;
    case WM_MOUSEMOVE:
        if (v && v->selecting) { v->end = {GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; InvalidateRect(hwnd,nullptr,FALSE); }
        if (v && v->panning) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            v->ox += p.x - v->last.x; v->oy += p.y - v->last.y; v->last = p;
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (v && v->selecting) {
            v->end = {GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            v->selecting = false; v->has_selection = true; ReleaseCapture();
            InvalidateRect(hwnd,nullptr,FALSE);
        } return 0;
    case WM_RBUTTONUP:
        if (v && v->panning) { v->panning = false; ReleaseCapture(); } return 0;
    case WM_MOUSEWHEEL:
        if (v) {
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ScreenToClient(hwnd,&point);
            double previous = v->zoom;
            v->zoom = std::clamp(previous * (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1.2 : 1/1.2), 0.05, 15.0);
            v->ox = point.x - (point.x-v->ox)*v->zoom/previous;
            v->oy = point.y - (point.y-v->oy)*v->zoom/previous;
            InvalidateRect(hwnd,nullptr,FALSE);
        } return 0;
    case WM_KEYDOWN:
        if (v && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (wp == 'C') copy_selection(hwnd,*v);
            else if (wp == 'S') save_selection(hwnd,*v);
            else if (wp == 'A') { v->has_selection = false; InvalidateRect(hwnd,nullptr,FALSE); }
        } return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
} // namespace

int show_preview(const std::wstring& file_path) {
    GdiplusStartupInput input{};
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &input, nullptr) != Ok) return 1;
    Viewer state;
    state.image = std::make_unique<Bitmap>(file_path.c_str());
    if (state.image->GetLastStatus() != Ok) { GdiplusShutdown(token); return 1; }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.hInstance = instance; wc.lpfnWndProc = window_proc;
    wc.lpszClassName = L"MagicScanNativePreview";
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0,wc.lpszClassName,
        L"MagicScan - Drag: select  |  Right drag: pan  |  Wheel: zoom  |  Ctrl+C: copy  |  Ctrl+S: save",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1100,820,
        nullptr,nullptr,instance,&state);
    if (!hwnd) { GdiplusShutdown(token); return 1; }
    MSG message{};
    while (GetMessageW(&message,nullptr,0,0)>0) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    GdiplusShutdown(token);
    return 0;
}
} // namespace magicscan
