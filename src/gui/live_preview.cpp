#include "live_preview.hpp"
#include "resource.hpp"
#include "about.hpp"
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
constexpr int kToolbarHeight = 90;
struct Editor {
    double zoom = 1.0, ox = 20.0, oy = 20.0;
    POINT begin{}, end{}, last{};
    bool selecting = false, panning = false, selected = false, auto_fit = true;
};
Editor editor;
int image_x(int x) { return static_cast<int>(std::floor((x-editor.ox)/editor.zoom)); }
int image_y(int y) { return static_cast<int>(std::floor((y-editor.oy)/editor.zoom)); }
RECT selected_rect() {
    // Selection endpoints are stored in image pixel coordinates, not screen space.
    int x1=std::clamp(static_cast<int>(editor.begin.x),0,static_cast<int>(image_width));
    int x2=std::clamp(static_cast<int>(editor.end.x),0,static_cast<int>(image_width));
    int y1=std::clamp(static_cast<int>(editor.begin.y),0,static_cast<int>(image_height));
    int y2=std::clamp(static_cast<int>(editor.end.y),0,static_cast<int>(image_height));
    return {std::min(x1,x2),std::min(y1,y2),std::max(x1,x2),std::max(y1,y2)};
}
void fit_view(HWND hwnd) {
    if (!image_width || !image_height) return;
    RECT client{}; GetClientRect(hwnd,&client);
    const int w=client.right-client.left, h=client.bottom-client.top-kToolbarHeight;
    if (w<=0 || h<=0) return;
    editor.zoom=std::min(static_cast<double>(w-24)/image_width,
                         static_cast<double>(h-24)/image_height);
    editor.zoom=std::clamp(editor.zoom,0.02,15.0);
    editor.ox=(w-image_width*editor.zoom)/2;
    editor.oy=kToolbarHeight+(h-image_height*editor.zoom)/2;
}
void zoom_at(POINT p,double factor) {
    const double previous=editor.zoom;
    editor.zoom=std::clamp(previous*factor,0.02,15.0);
    editor.ox=p.x-(p.x-editor.ox)*editor.zoom/previous;
    editor.oy=p.y-(p.y-editor.oy)*editor.zoom/previous;
    editor.auto_fit=false;
}

std::wstring saved_path;

constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 76;
constexpr int kToolbarLeft = 12;
constexpr int kToolbarGap = 8;
enum class Command { Copy, Save, Clear, Fit, ZoomIn, ZoomOut };
struct Tool {
    Command command;
    const wchar_t* glyph;
    const wchar_t* label;
};
constexpr Tool kTools[] = {
    {Command::Copy, L"\uE8C8", L"Copier"},
    {Command::Save, L"\uE74E", L"Enregistrer"},
    {Command::Clear, L"\uE711", L"Effacer"},
    {Command::Fit, L"\uE9A6", L"Ajuster"},
    {Command::ZoomIn, L"\uE8A3", L"Zoom +"},
    {Command::ZoomOut, L"\uE71F", L"Zoom -"},
};
int hovered_tool = -1;
int pressed_tool = -1;
int tool_at(POINT p) {
    if (p.y < 7 || p.y >= 7+kButtonHeight) return -1;
    for (int i=0;i<static_cast<int>(sizeof(kTools)/sizeof(kTools[0]));++i) {
        const int x=kToolbarLeft+i*(kButtonWidth+kToolbarGap);
        if (p.x>=x && p.x<x+kButtonWidth) return i;
    }
    return -1;
}
void draw_toolbar(HDC dc, int width) {
    RECT background{0,0,width,kToolbarHeight};
    HBRUSH bar=CreateSolidBrush(RGB(238,241,246));
    FillRect(dc,&background,bar);
    DeleteObject(bar);
    HPEN separator=CreatePen(PS_SOLID,1,RGB(203,210,220));
    HGDIOBJ previous=SelectObject(dc,separator);
    MoveToEx(dc,0,kToolbarHeight-1,nullptr);
    LineTo(dc,width,kToolbarHeight-1);
    SelectObject(dc,previous);
    DeleteObject(separator);

    HFONT icon_font=CreateFontW(-31,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Segoe MDL2 Assets");
    HFONT label_font=CreateFontW(-16,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Segoe UI");
    SetBkMode(dc,TRANSPARENT);
    for (int i=0;i<static_cast<int>(sizeof(kTools)/sizeof(kTools[0]));++i) {
        const int x=kToolbarLeft+i*(kButtonWidth+kToolbarGap);
        RECT button{x,7,x+kButtonWidth,7+kButtonHeight};
        HBRUSH fill=CreateSolidBrush(i==pressed_tool?RGB(193,216,245):
            i==hovered_tool?RGB(217,232,250):RGB(255,255,255));
        HPEN border=CreatePen(PS_SOLID,1,RGB(193,202,214));
        HGDIOBJ old_brush=SelectObject(dc,fill);
        HGDIOBJ old_pen=SelectObject(dc,border);
        RoundRect(dc,button.left,button.top,button.right,button.bottom,12,12);
        SelectObject(dc,old_pen);
        SelectObject(dc,old_brush);
        DeleteObject(border); DeleteObject(fill);
        if (icon_font) {
            HGDIOBJ old_font=SelectObject(dc,icon_font);
            RECT glyph{x+2,12,x+kButtonWidth-2,49};
            SetTextColor(dc,RGB(35,88,147));
            DrawTextW(dc,kTools[i].glyph,-1,&glyph,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,old_font);
        }
        if (label_font) {
            HGDIOBJ old_font=SelectObject(dc,label_font);
            RECT label{x+2,54,x+kButtonWidth-2,79};
            SetTextColor(dc,RGB(31,43,56));
            DrawTextW(dc,kTools[i].label,-1,&label,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,old_font);
        }
    }
    if (icon_font) DeleteObject(icon_font);
    if (label_font) DeleteObject(label_font);
}


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
        MessageBoxW(hwnd,L"Impossible d’enregistrer l’image PNG sélectionnée.",L"MagicScan",MB_ICONERROR);
}

void run_tool(HWND hwnd, Command command) {
    switch (command) {
    case Command::Copy:
        if (!copy_selected(hwnd))
            MessageBoxW(hwnd,L"Impossible de copier la sélection.",L"MagicScan",MB_ICONWARNING);
        break;
    case Command::Save: save_selected(hwnd); break;
    case Command::Clear:
        editor.selected=false;
        editor.selecting=false;
        InvalidateRect(hwnd,nullptr,FALSE);
        break;
    case Command::Fit:
        editor.auto_fit=true;
        fit_view(hwnd);
        InvalidateRect(hwnd,nullptr,FALSE);
        break;
    case Command::ZoomIn:
    case Command::ZoomOut: {
        RECT rc{}; GetClientRect(hwnd,&rc);
        POINT center{(rc.left+rc.right)/2,(kToolbarHeight+rc.bottom)/2};
        zoom_at(center,command==Command::ZoomIn?1.2:1.0/1.2);
        InvalidateRect(hwnd,nullptr,FALSE);
        break;
    }
    }
}
LRESULT CALLBACK preview_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC screen_dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        const int paint_width=client.right-client.left;
        const int paint_height=client.bottom-client.top;
        HDC buffer_dc = paint_width>0 && paint_height>0
            ? CreateCompatibleDC(screen_dc) : nullptr;
        HBITMAP backing_bitmap = buffer_dc
            ? CreateCompatibleBitmap(screen_dc,paint_width,paint_height) : nullptr;
        HGDIOBJ old_bitmap = backing_bitmap ? SelectObject(buffer_dc,backing_bitmap) : nullptr;
        HDC dc = old_bitmap ? buffer_dc : screen_dc;
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        const int saved_clip=SaveDC(dc);
        IntersectClipRect(dc,0,kToolbarHeight,paint_width,paint_height);
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
            DrawTextW(dc, L"En attente des bandes du scanner...", -1, &client,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RestoreDC(dc,saved_clip);
        draw_toolbar(dc,paint_width);
        if (old_bitmap) {
            BitBlt(screen_dc,0,0,paint_width,paint_height,buffer_dc,0,0,SRCCOPY);
            SelectObject(buffer_dc,old_bitmap);
        }
        if (backing_bitmap) DeleteObject(backing_bitmap);
        if (buffer_dc) DeleteDC(buffer_dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        if (p.y<kToolbarHeight) {
            pressed_tool=tool_at(p);
            if (pressed_tool>=0) { SetCapture(hwnd); InvalidateRect(hwnd,nullptr,FALSE); }
            return 0;
        }
        SetFocus(hwnd);
        editor.begin=editor.end={image_x(GET_X_LPARAM(lp)),image_y(GET_Y_LPARAM(lp))};
        editor.selecting=true; editor.selected=false;
        SetCapture(hwnd);
        return 0;
    }
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        editor.last={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        editor.panning=true;
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE: {
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        int hover=tool_at(p);
        if (hover!=hovered_tool) { hovered_tool=hover; InvalidateRect(hwnd,nullptr,FALSE); }
        if (editor.selecting) {
            editor.end={image_x(GET_X_LPARAM(lp)),image_y(GET_Y_LPARAM(lp))};
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        if (editor.panning) {
            editor.ox+=p.x-editor.last.x; editor.oy+=p.y-editor.last.y;
            editor.last=p; editor.auto_fit=false;
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        if (pressed_tool>=0) {
            int clicked=pressed_tool;
            pressed_tool=-1;
            if (GetCapture()==hwnd) ReleaseCapture();
            InvalidateRect(hwnd,nullptr,FALSE);
            if (tool_at(p)==clicked) run_tool(hwnd,kTools[clicked].command);
            return 0;
        }
        if (editor.selecting) {
            editor.end={image_x(GET_X_LPARAM(lp)),image_y(GET_Y_LPARAM(lp))};
            editor.selecting=false; editor.selected=true;
            ReleaseCapture(); InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    }
    case WM_RBUTTONUP:
        if (editor.panning) { editor.panning=false; ReleaseCapture(); }
        return 0;
    case WM_CAPTURECHANGED:
        pressed_tool=-1;
        editor.selecting=false;
        editor.panning=false;
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd,&p);
        zoom_at(p,GET_WHEEL_DELTA_WPARAM(wp)>0?1.2:1.0/1.2);
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    }
    case WM_KEYDOWN: {
        if (wp==VK_ESCAPE) {
            editor.selected=false;
            editor.selecting=false;
            if (!editor.panning && GetCapture()==hwnd) ReleaseCapture();
            InvalidateRect(hwnd,nullptr,FALSE);
            return 0;
        }
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
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == 0x1F00) { show_about(hwnd); return 0; }
        break;
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
    preview = CreateWindowExW(0, L"MagicScanLivePreview", L"MagicScan - Prévisualisation et édition",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
                              nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (preview) {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        HICON large = static_cast<HICON>(LoadImageW(instance,
            MAKEINTRESOURCEW(IDI_MAGICSCAN), IMAGE_ICON, 32, 32, LR_SHARED));
        HICON small = static_cast<HICON>(LoadImageW(instance,
            MAKEINTRESOURCEW(IDI_MAGICSCAN), IMAGE_ICON, 16, 16, LR_SHARED));
        SendMessageW(preview, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large));
        SendMessageW(preview, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
        HMENU system_menu = GetSystemMenu(preview, FALSE);
        if (system_menu) {
            AppendMenuW(system_menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(system_menu, MF_STRING, 0x1F00, L"À propos de MagicScan...");
        }
    }
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
            hovered_tool=-1;
            pressed_tool=-1;
            ensure_window();
            if (preview) SetWindowTextW(preview,L"MagicScan - numérisation en cours...");
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
                SetWindowTextW(preview,L"MagicScan - numérisation terminée (PNG enregistré)");
                InvalidateRect(preview,nullptr,FALSE);
            }
            break;
        }
    }
}
} // namespace magicscan
