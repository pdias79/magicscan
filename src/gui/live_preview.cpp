#include "live_preview.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <commdlg.h>
#include <cmath>
#include <cwchar>
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
struct Editor {
    double zoom = 1.0, ox = 20.0, oy = 20.0;
    POINT begin{}, end{}, last{};
    bool selecting = false, panning = false, selected = false, auto_fit = true;
};
Editor editor;
int image_x(int x) { return static_cast<int>(std::floor((x-editor.ox)/editor.zoom)); }
int image_y(int y) { return static_cast<int>(std::floor((y-editor.oy)/editor.zoom)); }
RECT selected_rect() {
    int x1=std::clamp(image_x(editor.begin.x),0,static_cast<int>(image_width));
    int x2=std::clamp(image_x(editor.end.x),0,static_cast<int>(image_width));
    int y1=std::clamp(image_y(editor.begin.y),0,static_cast<int>(image_height));
    int y2=std::clamp(image_y(editor.end.y),0,static_cast<int>(image_height));
    return {std::min(x1,x2),std::min(y1,y2),std::max(x1,x2),std::max(y1,y2)};
}
void fit_view(HWND hwnd) {
    if (!image_width || !image_height) return;
    RECT client{}; GetClientRect(hwnd,&client);
    const int w=client.right-client.left, h=client.bottom-client.top;
    if (w<=0 || h<=0) return;
    editor.zoom=std::min(static_cast<double>(w-24)/image_width,
                         static_cast<double>(h-24)/image_height);
    editor.zoom=std::clamp(editor.zoom,0.02,15.0);
    editor.ox=(w-image_width*editor.zoom)/2;
    editor.oy=(h-image_height*editor.zoom)/2;
}
void zoom_at(POINT p,double factor) {
    const double previous=editor.zoom;
    editor.zoom=std::clamp(previous*factor,0.02,15.0);
    editor.ox=p.x-(p.x-editor.ox)*editor.zoom/previous;
    editor.oy=p.y-(p.y-editor.oy)*editor.zoom/previous;
    editor.auto_fit=false;
}

std::wstring saved_path;

bool encode_png(const std::wstring& filename, const std::vector<uint8_t>& bgr,
                int width, int height) {
    Gdiplus::GdiplusStartupInput input{};
    ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) return false;
    bool ok=false;
    {
        Gdiplus::Bitmap bitmap(width,height,PixelFormat24bppRGB);
        Gdiplus::Rect rect(0,0,width,height);
        Gdiplus::BitmapData locked{};
        if (bitmap.LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat24bppRGB,&locked)==Gdiplus::Ok) {
            for (int y=0;y<height;++y)
                std::memcpy(static_cast<uint8_t*>(locked.Scan0)+static_cast<ptrdiff_t>(y)*locked.Stride,
                            bgr.data()+static_cast<size_t>(y)*width*3,static_cast<size_t>(width)*3);
            bitmap.UnlockBits(&locked);
            UINT count=0, bytes=0;
            if (Gdiplus::GetImageEncodersSize(&count,&bytes)==Gdiplus::Ok && bytes) {
                std::vector<uint8_t> storage(bytes);
                auto* codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
                if (Gdiplus::GetImageEncoders(count,bytes,codecs)==Gdiplus::Ok) {
                    for (UINT i=0;i<count;++i) {
                        if (codecs[i].MimeType && wcscmp(codecs[i].MimeType,L"image/png")==0) {
                            ok=bitmap.Save(filename.c_str(),&codecs[i].Clsid)==Gdiplus::Ok;
                            break;
                        }
                    }
                }
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}
bool extract_selection(std::vector<uint8_t>& bgr,int& width,int& height) {
    if (!image_width || !image_height || accumulated.empty()) return false;
    RECT r=editor.selected?selected_rect():
        RECT{0,0,static_cast<LONG>(image_width),static_cast<LONG>(image_height)};
    width=r.right-r.left; height=r.bottom-r.top;
    if (width<=0 || height<=0) return false;
    bgr.resize(static_cast<size_t>(width)*height*3);
    for (int y=0;y<height;++y) {
        const size_t source=(static_cast<size_t>(r.top+y)*image_width+r.left)*3;
        std::memcpy(bgr.data()+static_cast<size_t>(y)*width*3,
                    accumulated.data()+source,static_cast<size_t>(width)*3);
    }
    return true;
}
bool copy_selected(HWND hwnd) {
    std::vector<uint8_t> pixels;
    int w=0,h=0;
    if (!extract_selection(pixels,w,h)) return false;
    const size_t stride=(static_cast<size_t>(w)*3+3)&~size_t(3);
    const size_t bytes=stride*static_cast<size_t>(h);
    HGLOBAL block=GlobalAlloc(GMEM_MOVEABLE,sizeof(BITMAPINFOHEADER)+bytes);
    if (!block) return false;
    auto* memory=static_cast<uint8_t*>(GlobalLock(block));
    if (!memory) { GlobalFree(block); return false; }
    BITMAPINFOHEADER header{};
    header.biSize=sizeof(header); header.biWidth=w; header.biHeight=h;
    header.biPlanes=1; header.biBitCount=24; header.biCompression=BI_RGB;
    header.biSizeImage=static_cast<DWORD>(bytes);
    std::memcpy(memory,&header,sizeof(header));
    uint8_t* dst=memory+sizeof(header);
    for (int y=0;y<h;++y) {
        uint8_t* row=dst+static_cast<size_t>(h-y-1)*stride;
        std::memcpy(row,pixels.data()+static_cast<size_t>(y)*w*3,static_cast<size_t>(w)*3);
        if (stride>static_cast<size_t>(w)*3)
            std::memset(row+static_cast<size_t>(w)*3,0,stride-static_cast<size_t>(w)*3);
    }
    GlobalUnlock(block);
    bool ok=false;
    if (OpenClipboard(hwnd)) {
        if (EmptyClipboard() && SetClipboardData(CF_DIB,block)) {
            ok=true; block=nullptr;
        }
        CloseClipboard();
    }
    if (block) GlobalFree(block);
    return ok;
}
void save_selected(HWND hwnd) {
    std::vector<uint8_t> pixels;
    int w=0,h=0;
    if (!extract_selection(pixels,w,h)) return;
    wchar_t file[MAX_PATH]=L"selection.png";
    OPENFILENAMEW dialog{};
    dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=hwnd;
    dialog.lpstrFilter=L"PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile=file; dialog.nMaxFile=MAX_PATH;
    dialog.lpstrDefExt=L"png";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&dialog) && !encode_png(file,pixels,w,h))
        MessageBoxW(hwnd,L"Unable to save the selected PNG.",L"MagicScan",MB_ICONERROR);
}
LRESULT CALLBACK preview_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        if (image_width && image_height && !accumulated.empty()) {
            if (editor.auto_fit) fit_view(hwnd);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(image_width);
            info.bmiHeader.biHeight = -static_cast<LONG>(image_height);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 24;
            info.bmiHeader.biCompression = BI_RGB;
            const int draw_w=std::max(1,static_cast<int>(image_width*editor.zoom));
            const int draw_h=std::max(1,static_cast<int>(image_height*editor.zoom));
            SetStretchBltMode(dc,HALFTONE);
            StretchDIBits(dc,static_cast<int>(editor.ox),static_cast<int>(editor.oy),
                draw_w,draw_h,0,0,image_width,image_height,
                accumulated.data(),&info,DIB_RGB_COLORS,SRCCOPY);
            if (editor.selected || editor.selecting) {
                const RECT r=selected_rect();
                RECT outline{
                    static_cast<LONG>(editor.ox+r.left*editor.zoom),
                    static_cast<LONG>(editor.oy+r.top*editor.zoom),
                    static_cast<LONG>(editor.ox+r.right*editor.zoom),
                    static_cast<LONG>(editor.oy+r.bottom*editor.zoom)
                };
                HPEN pen=CreatePen(PS_SOLID,2,RGB(0,150,220));
                HGDIOBJ old=SelectObject(dc,pen);
                HGDIOBJ oldbrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
                Rectangle(dc,outline.left,outline.top,outline.right,outline.bottom);
                SelectObject(dc,oldbrush); SelectObject(dc,old);
                DeleteObject(pen);
            }
        } else {
            DrawTextW(dc, L"Waiting for scan bands...", -1, &client,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        editor.begin=editor.end={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        editor.selecting=true; editor.selected=false;
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        editor.last={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        editor.panning=true;
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (editor.selecting) {
            editor.end={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        if (editor.panning) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            editor.ox+=p.x-editor.last.x; editor.oy+=p.y-editor.last.y;
            editor.last=p; editor.auto_fit=false;
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (editor.selecting) {
            editor.end={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            editor.selecting=false; editor.selected=true;
            ReleaseCapture(); InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_RBUTTONUP:
        if (editor.panning) { editor.panning=false; ReleaseCapture(); }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd,&p);
        zoom_at(p,GET_WHEEL_DELTA_WPARAM(wp)>0?1.2:1.0/1.2);
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    }
    case WM_KEYDOWN: {
        const bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if (ctrl) {
            if (wp=='C' && !copy_selected(hwnd))
                MessageBoxW(hwnd,L"Could not copy the selection.",L"MagicScan",MB_ICONWARNING);
            if (wp=='S') save_selected(hwnd);
            if (wp=='A') { editor.selected=false; InvalidateRect(hwnd,nullptr,FALSE); }
            return 0;
        }
        double factor=0;
        if (wp==VK_ADD || wp==VK_OEM_PLUS) factor=1.2;
        if (wp==VK_SUBTRACT || wp==VK_OEM_MINUS) factor=1.0/1.2;
        if (factor) {
            RECT rc{}; GetClientRect(hwnd,&rc);
            zoom_at(POINT{(rc.left+rc.right)/2,(rc.top+rc.bottom)/2},factor);
            InvalidateRect(hwnd,nullptr,FALSE);
        }
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
        cls.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        registered = RegisterClassW(&cls) != 0;
    }
    if (!registered) return;
    preview = CreateWindowExW(0, L"MagicScanLivePreview", L"MagicScan - Drag: select | Right drag: pan | Wheel: zoom | Ctrl+C: copy | Ctrl+S: save",
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
            editor=Editor{};
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
            // Auto-fit until the user zooms or pans manually.
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
