#include "preferences.hpp"
#include "resource.hpp"
#include <shlobj.h>
#include <algorithm>
#include <string>
namespace magicscan {
namespace {
constexpr int kSave=6101, kClipboard=6102, kFolder=6103, kGamma=6104,
              kApply=6105, kCancel=6106, kBrowse=6107;
HWND preferences_window=nullptr;
UserSettings initial{};
HWND field(HWND parent,int id) { return GetDlgItem(parent,id); }
void set_font(HWND control) {
    if (control) SendMessageW(control,WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
}
HWND control(HWND hwnd,const wchar_t* type,const wchar_t* label,DWORD style,
             int x,int y,int w,int h,int id=0) {
    HWND c=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    set_font(c); return c;
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        control(hwnd,L"BUTTON",L"Enregistrer automatiquement les scans sur le disque",
                BS_AUTOCHECKBOX|WS_TABSTOP,20,20,435,28,kSave);
        control(hwnd,L"BUTTON",L"Copier automatiquement dans le presse-papiers",
                BS_AUTOCHECKBOX|WS_TABSTOP,20,55,435,28,kClipboard);
        control(hwnd,L"STATIC",L"Dossier de sauvegarde :",0,20,99,340,24);
        control(hwnd,L"EDIT",initial.scan_folder.c_str(),
                WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,20,125,356,30,kFolder);
        control(hwnd,L"BUTTON",L"Parcourir...",BS_PUSHBUTTON|WS_TABSTOP,
                386,125,104,30,kBrowse);
        control(hwnd,L"STATIC",L"Gamma par défaut (20 à 300 ; 100 = neutre) :",0,20,176,430,23);
        control(hwnd,L"EDIT",std::to_wstring(initial.default_gamma).c_str(),
                WS_BORDER|ES_NUMBER|WS_TABSTOP,20,201,120,29,kGamma);
        control(hwnd,L"BUTTON",L"Enregistrer",BS_DEFPUSHBUTTON|WS_TABSTOP,
                254,260,110,36,kApply);
        control(hwnd,L"BUTTON",L"Annuler",BS_PUSHBUTTON|WS_TABSTOP,
                376,260,110,36,kCancel);
        SendMessageW(field(hwnd,kSave),BM_SETCHECK,initial.auto_save?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(field(hwnd,kClipboard),BM_SETCHECK,initial.auto_clipboard?BST_CHECKED:BST_UNCHECKED,0);
        return 0;
    }
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case kBrowse: {
            BROWSEINFOW info{};
            info.hwndOwner=hwnd;
            info.lpszTitle=L"Choisir le dossier de numérisation";
            info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE selected=SHBrowseForFolderW(&info);
            if (selected) {
                wchar_t path[MAX_PATH]{};
                if (SHGetPathFromIDListW(selected,path))
                    SetWindowTextW(field(hwnd,kFolder),path);
                CoTaskMemFree(selected);
            }
            return 0;
        }
        case kApply: {
            UserSettings settings=initial;
            settings.auto_save=SendMessageW(field(hwnd,kSave),BM_GETCHECK,0,0)==BST_CHECKED;
            settings.auto_clipboard=SendMessageW(field(hwnd,kClipboard),BM_GETCHECK,0,0)==BST_CHECKED;
            wchar_t path[32768]{};
            GetWindowTextW(field(hwnd,kFolder),path,32768);
            settings.scan_folder=path;
            wchar_t gamma[20]{};
            GetWindowTextW(field(hwnd,kGamma),gamma,20);
            settings.default_gamma=std::clamp(_wtoi(gamma),20,300);
            if (settings.scan_folder.empty()) {
                MessageBoxW(hwnd,L"Indiquez un dossier de sauvegarde.",L"MagicScan",MB_ICONWARNING);
                return 0;
            }
            if (!save_settings(settings)) {
                MessageBoxW(hwnd,L"Impossible d’enregistrer les paramètres.",L"MagicScan",MB_ICONERROR);
                return 0;
            }
            HWND parent=GetWindow(hwnd,GW_OWNER);
            DestroyWindow(hwnd);
            if (parent) PostMessageW(parent,kPreferencesChanged,0,0);
            return 0;
        }
        case kCancel: DestroyWindow(hwnd); return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: preferences_window=nullptr; return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
void show_preferences(HWND owner,const UserSettings& current) {
    if (preferences_window) {
        SetForegroundWindow(preferences_window); return;
    }
    initial=current;
    static bool registered=false;
    if (!registered) {
        WNDCLASSW cls{};
        cls.hInstance=GetModuleHandleW(nullptr);
        cls.lpszClassName=L"MagicScanPreferences";
        cls.lpfnWndProc=proc;
        cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        cls.hbrBackground=static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        registered=RegisterClassW(&cls)!=0;
    }
    if (!registered) return;
    preferences_window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"MagicScanPreferences",
        L"Paramètres de MagicScan",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,530,355,
        owner,nullptr,GetModuleHandleW(nullptr),nullptr);
}
}
