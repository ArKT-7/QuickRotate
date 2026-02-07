/*
  Quick Rotate V6 by ArKT | Modern Display Orientation Tool/Utility for Windows
  Copyright (c) 2026 ArKT-7 (https://github.com/ArKT-7/QuickRotate)
  Build: windres QuickRotate.rc -O coff -o QuickRotate_res.o; g++ QuickRotate.cpp QuickRotate_res.o -o QuickRotate.exe -static -nostartfiles -e _WinMain@16 -Os -s -fno-exceptions -fno-rtti -fno-stack-protector -fomit-frame-pointer "-Wl,--gc-sections" -lgdi32 -luser32 -lgdiplus -lshlwapi -lshell32 -lole32 -luuid -ladvapi32 -mwindows
*/

#define _WIN32_WINNT 0x0605
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h> 
#include <gdiplus.h>
#include <cstdio>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "advapi32.lib")

int g_dpi = 96;

int S(int value) {
    if (g_dpi <= 0) g_dpi = 96;
    return MulDiv(value, g_dpi, 96);
}

using namespace Gdiplus;

#define WIN_W  339
#define WIN_H  480
#define BTN_W  290
#define BTN_H  60
#define BTN_SH 40
#define BTN_X  21
#define CORNER_RADIUS 10

#define WM_TRAYICON       (WM_USER + 100)
#define ID_TRAY_RESTORE   2001
#define ID_TRAY_EXIT      2002

#define ID_BTN_LANDSCAPE  100
#define ID_BTN_PORTRAIT   101
#define ID_BTN_FLIPPED    102
#define ID_BTN_FLIPPORT   103
#define ID_BTN_NEXT       105
#define ID_BTN_SETTINGS   110 

#define ID_BTN_BACK       200
#define ID_CHK_TRAY       201
#define ID_CHK_AUTOSTART  202
#define ID_CHK_TRAYMODE   203

#define ID_SC_NEXT        4000
#define ID_SC_LANDSCAPE   4001
#define ID_SC_PORTRAIT    4002
#define ID_SC_FLIPPED     4003
#define ID_SC_FLIPPORT    4004
#define ID_SC_APP         4005

const wchar_t* AppTitle = L"Quick Rotate V6 by ArKT";
const wchar_t* AppClass = L"ArKT_QuickRotate";

HFONT hFontBold = NULL;
HFONT hFontNormal = NULL;
HFONT hFontHeader = NULL;
HFONT hFontMenu = NULL;

HICON hIconSm = NULL;
HICON hIconBig = NULL;

WNDPROC oldBtnProc;
HWND hHover = NULL;
HWND hMainWnd = NULL;

ULONG_PTR gdiplusToken;
int currentScreenRot = -1;
NOTIFYICONDATAW nid = {0};
bool bCloseToTray = true; 
bool bAutoStart = false; 
bool bSettingsMode = false;
bool bUpdateMode = false;
bool bTrayToggleLP = true;
wchar_t iniPath[MAX_PATH];
bool bShortcutsState[6] = {0};

HWND hBtnRot[5];
HWND hBtnSettings; 
HWND hSetControls[10];

struct AutoMemDC {
    HDC hDC, hMemDC; HBITMAP hBM, hOldBM; Graphics* g; int x, y, w, h;
    
    AutoMemDC(HDC hdc, int _x, int _y, int _w, int _h) : hDC(hdc), x(_x), y(_y), w(_w), h(_h) {
        hMemDC = CreateCompatibleDC(hDC);
        hBM = CreateCompatibleBitmap(hDC, w, h);
        hOldBM = (HBITMAP)SelectObject(hMemDC, hBM);
        g = new Graphics(hMemDC);
        g->SetSmoothingMode(SmoothingModeAntiAlias);
        g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g->Clear(Color(240, 240, 240));
    }

    ~AutoMemDC() {
        BitBlt(hDC, x, y, w, h, hMemDC, 0, 0, SRCCOPY);
        delete g;
        SelectObject(hMemDC, hOldBM);
        DeleteObject(hBM);
        DeleteDC(hMemDC);
    }
};

LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l);

HWND CreateMyButton(HWND parent, LPCWSTR text, int id, int x, int y, int w, int h, DWORD extraStyle = 0) {
    HWND btn = CreateWindowW(L"BUTTON", text, WS_CHILD | BS_OWNERDRAW | extraStyle, 
        S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(btn, GWLP_WNDPROC, (LONG_PTR)BtnProc);
    if (!oldBtnProc) oldBtnProc = prev;
    return btn;
}

bool GetAppDataPath(wchar_t* outPath, const wchar_t* appendFile) {
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, outPath))) return false;
    PathAppendW(outPath, L"ArKT_QuickRotate");
    CreateDirectoryW(outPath, NULL);
    if (appendFile) PathAppendW(outPath, appendFile);
    return true;
}

wchar_t MyToLower(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + (L'a' - L'A');
    return c;
}

bool GetStableExePath(wchar_t* outPath) {
    return GetAppDataPath(outPath, L"QuickRotate.exe");
}

void InitSettingsPath() {
    if (!GetAppDataPath(iniPath, L"ArKT_QuickRotate.ini")) {
        GetModuleFileNameW(NULL, iniPath, MAX_PATH); 
        PathRemoveFileSpecW(iniPath); 
        PathAppendW(iniPath, L"ArKT_QuickRotate.ini");
    }
}

bool EnsureInstalled(wchar_t* finalPath, bool forceUpdate) {
    if (GetStableExePath(finalPath)) {
        if (forceUpdate || GetFileAttributesW(finalPath) == INVALID_FILE_ATTRIBUTES) {
            wchar_t current[MAX_PATH]; GetModuleFileNameW(NULL, current, MAX_PATH);
            if (lstrcmpiW(current, finalPath) != 0) {
                CopyFileW(current, finalPath, FALSE);
            }
        }
        return true;
    }
    return false;
}

void UpdateAutoStartRegistry(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            EnsureInstalled(exePath, true); 
            wchar_t cmd[MAX_PATH + 20];
            wsprintfW(cmd, L"\"%s\" -tray", exePath);
            RegSetValueExW(hKey, L"ArKT_QuickRotate", 0, REG_SZ, (LPBYTE)cmd, (lstrlenW(cmd) + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"ArKT_QuickRotate");
        }
        RegCloseKey(hKey);
    }
}

void GetLinkPath(wchar_t* outPath, const wchar_t* name) {
    wchar_t szDesktop[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, szDesktop);
    wsprintfW(outPath, L"%s\\%s.lnk", szDesktop, name);
}

HRESULT CreateLink(LPCWSTR lpszArgs, LPCWSTR lpszDesc, LPCWSTR lpszSuffix) {
    HRESULT hres;
    IShellLink* psl;
    wchar_t exePath[MAX_PATH];
    EnsureInstalled(exePath, false);
    wchar_t szLinkPath[MAX_PATH];
    GetLinkPath(szLinkPath, lpszSuffix);

    if (GetFileAttributesW(szLinkPath) != INVALID_FILE_ATTRIBUTES) return HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;
        psl->SetPath(exePath);
        psl->SetArguments(lpszArgs);
        psl->SetDescription(lpszDesc);
        psl->SetIconLocation(exePath, 0);
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hres)) { hres = ppf->Save(szLinkPath, TRUE); ppf->Release(); }
        psl->Release();
    }
    return hres;
}

void ManageShortcut(int index, bool create) {
    LPCWSTR names[] = {L"Rotate Screen Clockwise", L"Set Landscape", L"Set Portrait", L"Set Flipped Landscape", L"Set Flipped Portrait", L"Quick Rotate"};
    LPCWSTR args[]  = {L"next", L"0", L"90", L"180", L"270", L""};
    
    if (create) {
        CreateLink(args[index], names[index], names[index]);
    } else {
        wchar_t path[MAX_PATH];
        GetLinkPath(path, names[index]);
        DeleteFileW(path);
    }
}

void LoadSettings() {
    bCloseToTray = GetPrivateProfileIntW(L"Settings", L"CloseToTray", 1, iniPath);
    bAutoStart = GetPrivateProfileIntW(L"Settings", L"AutoStart", 0, iniPath);
    bTrayToggleLP = GetPrivateProfileIntW(L"Settings", L"TrayToggleLP", 1, iniPath);
}
void SaveSettings() {
    WritePrivateProfileStringW(L"Settings", L"CloseToTray", bCloseToTray ? L"1" : L"0", iniPath);
    WritePrivateProfileStringW(L"Settings", L"AutoStart", bAutoStart ? L"1" : L"0", iniPath);
    WritePrivateProfileStringW(L"Settings", L"TrayToggleLP", bTrayToggleLP ? L"1" : L"0", iniPath);
}

bool IsNativePortrait() {
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        int w = dm.dmPelsWidth;
        int h = dm.dmPelsHeight;
        if (dm.dmDisplayOrientation == DMDO_90 || dm.dmDisplayOrientation == DMDO_270) {
            int temp = w; w = h; h = temp;
        }
        return w < h;
    }
    return false;
}

void UpdateCurrentRotation() {
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        if (IsNativePortrait()) {
            currentScreenRot = (dm.dmDisplayOrientation + 1) % 4;
        } else {
            currentScreenRot = dm.dmDisplayOrientation;
        }
    }
}

void SetRot(int angle) {
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        if (angle == -1) {
            int current = dm.dmDisplayOrientation;
            int next = (current + 3) % 4; 
            angle = next * 90;
        }
        else {
            if (IsNativePortrait()) {
                int idx = angle / 90;
                angle = ((idx + 3) % 4) * 90;
            }
        }
        int neu = angle / 90;
        int old = dm.dmDisplayOrientation;
        if ((old % 2) != (neu % 2)) {
            int swp = dm.dmPelsWidth;
            dm.dmPelsWidth = dm.dmPelsHeight;
            dm.dmPelsHeight = swp;
            dm.dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT;
        } else dm.dmFields = DM_DISPLAYORIENTATION;
        dm.dmDisplayOrientation = neu;
        ChangeDisplaySettingsW(&dm, 0);
        if (IsNativePortrait()) currentScreenRot = (neu + 1) % 4;
        else currentScreenRot = neu;
    }
}

void GetRoundedRectPath(GraphicsPath* path, Rect r, int d) {
    if (d <= 0) { path->AddRectangle(r); return; }
    path->AddArc(r.X, r.Y, d, d, 180, 90);
    path->AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path->AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path->AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path->CloseFigure();
}

LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_MOUSEMOVE) {
        if (hHover != h) {
            hHover = h;
            InvalidateRect(h, NULL, FALSE); 
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
        }
    }
    else if (m == WM_MOUSELEAVE) {
        hHover = NULL;
        InvalidateRect(h, NULL, FALSE);
    }
    return CallWindowProc(oldBtnProc, h, m, w, l);
}

void ToggleViewMode(HWND h) {
    bSettingsMode = !bSettingsMode;

    if (bSettingsMode) {
        LPCWSTR names[] = {L"Rotate Screen Clockwise", L"Set Landscape", L"Set Portrait", L"Set Flipped Landscape", L"Set Flipped Portrait", L"Quick Rotate"};
        for(int i=0; i<6; i++) {
            wchar_t path[MAX_PATH];
            GetLinkPath(path, names[i]);
            bShortcutsState[i] = (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
        }
    }

    int showMain = bSettingsMode ? SW_HIDE : SW_SHOW;
    for (int i=0; i<5; i++) ShowWindow(hBtnRot[i], showMain);
    ShowWindow(hBtnSettings, showMain);

    int showSet = bSettingsMode ? SW_SHOW : SW_HIDE;
    for (int i=0; i<10; i++) ShowWindow(hSetControls[i], showSet);

    InvalidateRect(h, NULL, TRUE);
}

void RecreateFonts() {
    if (hFontBold) DeleteObject(hFontBold);
    if (hFontNormal) DeleteObject(hFontNormal);
    if (hFontHeader) DeleteObject(hFontHeader);
    if (hFontMenu) DeleteObject(hFontMenu);
    
    hFontBold = CreateFontW(S(22),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,FF_SWISS,L"Segoe UI");
    hFontNormal = CreateFontW(S(17),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,FF_SWISS,L"Segoe UI");
    hFontHeader = CreateFontW(S(19),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,FF_SWISS,L"Segoe UI");
    hFontMenu = CreateFontW(S(18),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,FF_SWISS,L"Segoe UI");
}

void UpdateLayout(HWND h) {
    RecreateFonts();

    for (int i = 0; i < 5; i++) {
        SetWindowPos(hBtnRot[i], NULL, S(BTN_X), S(20 + (i * 75)), S(BTN_W), S(BTN_H), SWP_NOZORDER);
    }
    SetWindowPos(hBtnSettings, NULL, S(BTN_X), S(390), S(BTN_W), S(BTN_SH), SWP_NOZORDER);
    SetWindowPos(hSetControls[0], NULL, S(BTN_X), S(390), S(BTN_W), S(BTN_SH), SWP_NOZORDER);

    SetWindowPos(hSetControls[1], NULL, S(BTN_X), S(20), S(BTN_W), S(30), SWP_NOZORDER);
    SetWindowPos(hSetControls[2], NULL, S(BTN_X), S(60), S(BTN_W), S(30), SWP_NOZORDER);
    SetWindowPos(hSetControls[8], NULL, S(BTN_X), S(100), S(BTN_W), S(30), SWP_NOZORDER);
    SetWindowPos(hSetControls[9], NULL, S(BTN_X), S(160), S(BTN_W), S(30), SWP_NOZORDER);

    int startY = 198;
    for (int i = 0; i < 5; i++) {
        SetWindowPos(hSetControls[3+i], NULL, S(BTN_X), S(startY + (i * 38)), S(BTN_W), S(30), SWP_NOZORDER);
    }
    
    InvalidateRect(h, NULL, TRUE);
}

void DrawProIcon(Graphics& g, int id, int x, int y, int s, Color c, bool isFilled) {
    Pen pen(c, S(2));
    SolidBrush brush(c);

    int shortSide = s * 0.67; 
    int pad = (s - shortSide) / 2;

    if (id == 100 || id == 102) {
        Rect r(x, y + pad, s, shortSide);
        if (isFilled) g.FillRectangle(&brush, r);
        else g.DrawRectangle(&pen, r);
    }
    else if (id == 101 || id == 103) {
        Rect r(x + pad, y, shortSide, s);
        if (isFilled) g.FillRectangle(&brush, r);
        else g.DrawRectangle(&pen, r);
    }
    else if (id == 105) {
        int arrowPad = S(1); 
        Rect r(x + arrowPad, y + arrowPad, s - 2*arrowPad, s - 2*arrowPad);
        pen.SetEndCap(LineCapArrowAnchor);
        g.DrawArc(&pen, r, 0, 290);
    }
    else if (id == ID_TRAY_RESTORE) { 
        Rect r(x + S(1), y + S(-2), s - S(3.5), s - S(-3));
        g.DrawRectangle(&pen, r);
        g.DrawLine(&pen, x + S(1), y + S(4), x + s - S(2), y + S(4));
    }
    else if (id == ID_TRAY_EXIT) {
        int p = S(2); 
        g.DrawLine(&pen, x + p, y + p, x + s - p, y + s - p);
        g.DrawLine(&pen, x + s - p, y + p, x + p, y + s - p);
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            typedef BOOL (WINAPI *tCWMF)(UINT, DWORD);
            tCWMF pChangeWindowMessageFilter = (tCWMF)GetProcAddress(hUser32, "ChangeWindowMessageFilter");
            if (pChangeWindowMessageFilter) {
                pChangeWindowMessageFilter(WM_COMMAND, 1 /* MSGFLT_ADD */);
            }
        }

        HDC hdc = GetDC(h);
        g_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(h, hdc);
        UpdateCurrentRotation();
        RecreateFonts(); 

        LPCWSTR txt[] = {
            L"Landscape\n(Standard)",
            L"Portrait\n(Right 90\u00B0)",
            L"Rotate Clockwise\n(Next \u27F3)",
            L"Flipped Landscape\n(Upside Down)",
            L"Flipped Portrait\n(Left 270\u00B0)"};
        int ids[] = { 100, 101, 105, 102, 103 };

        for (int i = 0; i < 5; i++) {
            hBtnRot[i] = CreateMyButton(h, txt[i], ids[i], BTN_X, 20 + (i * 75), BTN_W, BTN_H, WS_VISIBLE | WS_TABSTOP);
        }

        hBtnSettings = CreateMyButton(h, L"\u2699 Settings", ID_BTN_SETTINGS, BTN_X, 390, BTN_W, BTN_SH, WS_VISIBLE | BS_PUSHBUTTON);
        
        hSetControls[0] = CreateMyButton(h, L"\u2B05 Back", ID_BTN_BACK, BTN_X, 390, BTN_W, BTN_SH, BS_PUSHBUTTON);
        hSetControls[1] = CreateMyButton(h, L"Minimize to Tray on Close", ID_CHK_TRAY, BTN_X, 20, BTN_W, 30);
        hSetControls[2] = CreateMyButton(h, L"Start with Windows", ID_CHK_AUTOSTART, BTN_X, 60, BTN_W, 30);

        LPCWSTR trayText = bTrayToggleLP ? L"Tray Click: Landscape \u2194 Portrait" : L"Tray Click: Rotate Clockwise \u27F3";
        hSetControls[8] = CreateMyButton(h, trayText, ID_CHK_TRAYMODE, BTN_X, 100, BTN_W, 30);
        hSetControls[9] = CreateMyButton(h, L"Shortcut: Quick Rotate App", ID_SC_APP, BTN_X, 160, BTN_W, 30);

        LPCWSTR scTxt[] = {
            L"Shortcut: Rotate Clockwise",
            L"Shortcut: Landscape",
            L"Shortcut: Portrait",
            L"Shortcut: Flipped Landscape",
            L"Shortcut: Flipped Portrait"
        };
        int scIds[] = { ID_SC_NEXT, ID_SC_LANDSCAPE, ID_SC_PORTRAIT, ID_SC_FLIPPED, ID_SC_FLIPPORT };
        
        for (int i = 0; i < 5; i++) {
            hSetControls[3+i] = CreateMyButton(h, scTxt[i], scIds[i], BTN_X, 198 + (i * 38), BTN_W, 30);
        }

        nid.cbSize = sizeof(NOTIFYICONDATAW); nid.hWnd = h; nid.uID = 1001; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIconSm;
        
        wsprintfW(nid.szTip, L"%s", AppTitle);
        return 0;
    }

    case 0x02E0: {
        g_dpi = HIWORD(w);
        RECT* const prcNewWindow = (RECT*)l;
        SetWindowPos(h, NULL, prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateLayout(h); 
        return 0;
    }

    case WM_DISPLAYCHANGE: {
        UpdateCurrentRotation();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)w; SetBkColor(hdcStatic, 0xF0F0F0); SetBkMode(hdcStatic, TRANSPARENT); 
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT m = (LPMEASUREITEMSTRUCT)l;
        if(m->CtlType == ODT_MENU) {
            if(m->itemID == 0) {
                m->itemHeight = S(10); 
            } else {
                m->itemHeight = S(40); 
            }
            m->itemWidth = S(260); 
        }
        return TRUE;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT p = (LPDRAWITEMSTRUCT)l;

        int w, h, x = 0, y = 0;
        if (p->CtlType == ODT_MENU) {
            w = p->rcItem.right - p->rcItem.left;
            h = p->rcItem.bottom - p->rcItem.top;
            x = p->rcItem.left;
            y = p->rcItem.top;
        } else {
            RECT cR; GetClientRect(p->hwndItem, &cR);
            w = cR.right; h = cR.bottom;
        }

        AutoMemDC buf(p->hDC, x, y, w, h);

        if (p->CtlType == ODT_BUTTON) {
            bool pressed = (p->itemState & ODS_SELECTED);
            bool hovered = (p->hwndItem == hHover);
            int btnId = p->CtlID;

            if (btnId >= 200 && btnId != ID_BTN_BACK && btnId != ID_TRAY_RESTORE && btnId != ID_TRAY_EXIT) {
                bool isChecked = false;
                if (btnId == ID_CHK_TRAY) isChecked = bCloseToTray;
                else if (btnId == ID_CHK_AUTOSTART) isChecked = bAutoStart;
                else if (btnId == ID_CHK_TRAYMODE) isChecked = bTrayToggleLP;
                else if (btnId >= 4000) isChecked = bShortcutsState[btnId - 4000];

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, 0); SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(5), 0, w - S(55), h}; DrawTextW(buf.hMemDC, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                int tH = S(22), tW = S(44), tX = w - tW - S(2), tY = (h - tH) / 2;
                GraphicsPath path; path.AddArc(tX, tY, tH, tH, 90, 180); path.AddArc(tX + tW - tH, tY, tH, tH, 270, 180); path.CloseFigure();
                Color tc = isChecked ? (hovered ? Color(0, 140, 235) : Color(0, 120, 215)) : Color(180, 180, 180);
                SolidBrush tb(tc); buf.g->FillPath(&tb, &path);
                SolidBrush wb(Color(255, 255, 255));
                buf.g->FillEllipse(&wb, isChecked ? (tX + tW - S(16) - S(3)) : (tX + S(3)), tY + (tH - S(16)) / 2, S(16), S(16));
            } else {
                Color bg, txt;
                bool active = (btnId - 100 == currentScreenRot) && (btnId < 200);

                if (btnId == ID_BTN_SETTINGS || btnId == ID_BTN_BACK) {
                    if (pressed) { bg = Color(60, 60, 60); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(120, 120, 120); txt = Color(255, 255, 255); }
                    else { bg = Color(80, 80, 80); txt = Color(255, 255, 255); }
                } else {
                    if (active && !hovered) { bg = Color(255, 255, 255); txt = Color(0, 120, 215); }
                    else if (pressed) { bg = Color(0, 80, 160); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(135, 206, 250); txt = Color(0, 60, 140); }
                    else { bg = Color(0, 120, 215); txt = Color(255, 255, 255); }
                }

                GraphicsPath path; Rect r(S(2), S(2), w - S(4), h - S(4));
                GetRoundedRectPath(&path, r, S(CORNER_RADIUS) * 2);
                SolidBrush b(bg); buf.g->FillPath(&b, &path);

                if (btnId >= 100 && btnId <= 105) {
                    DrawProIcon(*buf.g, btnId, S(15), (h - S(20)) / 2, S(20), txt, active);
                }
                
                if (active) {
                    Pen p(hovered ? Color(255, 255, 255) : Color(0, 120, 215), S(3));
                    p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound);
                    int tx = w - S(40), ty = h / 2;
                    buf.g->DrawLine(&p, tx, ty, tx + S(5), ty + S(5)); buf.g->DrawLine(&p, tx + S(5), ty + S(5), tx + S(14), ty - S(6));
                }

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, txt.ToCOLORREF()); SelectObject(buf.hMemDC, hFontBold);
                RECT tr = {0, 0, w, h}; if (pressed) OffsetRect(&tr, S(1), S(1));
                if (btnId >= 200 || btnId == ID_BTN_SETTINGS) DrawTextW(buf.hMemDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                else {
                    RECT cr = tr; DrawTextW(buf.hMemDC, text, -1, &cr, DT_CALCRECT | DT_CENTER | DT_WORDBREAK);
                    tr.top += (h - (cr.bottom - cr.top)) / 2; DrawTextW(buf.hMemDC, text, -1, &tr, DT_CENTER | DT_WORDBREAK);
                }
            }
        } else if (p->CtlType == ODT_MENU) {
            buf.g->Clear(Color(255, 255, 255));
            if (p->itemID == 0) {
                Pen pen(Color(220, 220, 220), 1); buf.g->DrawLine(&pen, S(10), h / 2, w - S(10), h / 2);
            } else {
                if (p->itemState & ODS_SELECTED) {
                    Rect r(S(4), S(2), w - S(8), h - S(4));
                    GraphicsPath path; GetRoundedRectPath(&path, r, S(8));
                    SolidBrush b(Color(230, 243, 255)); buf.g->FillPath(&b, &path);
                }
                bool chk = (p->itemState & ODS_CHECKED);
                DrawProIcon(*buf.g, p->itemID, S(12), (h - S(16)) / 2, S(16), chk ? Color(0, 120, 215) : Color(150, 150, 150), chk);

                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, 0); SelectObject(buf.hMemDC, hFontMenu);
                RECT tr = {S(40), 0, w, h}; DrawTextW(buf.hMemDC, (LPCWSTR)p->itemData, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id >= 100 && id <= 103) { 
            SetRot((id - 100) * 90);
            InvalidateRect(h, NULL, FALSE); }
        else if (id == 105) { SetRot(-1); InvalidateRect(h, NULL, FALSE); }
        else if (id == ID_BTN_SETTINGS || id == ID_BTN_BACK) { ToggleViewMode(h); }
        
        else if (id == ID_CHK_TRAY) {
            bCloseToTray = !bCloseToTray;
            SaveSettings(); 
            InvalidateRect((HWND)l, NULL, FALSE);
        }
        else if (id == ID_CHK_AUTOSTART) {
            bAutoStart = !bAutoStart;
            UpdateAutoStartRegistry(bAutoStart);
            SaveSettings();
            InvalidateRect((HWND)l, NULL, FALSE);
        }
        else if (id == ID_CHK_TRAYMODE) {
            bTrayToggleLP = !bTrayToggleLP;
            SetWindowTextW(hSetControls[8], bTrayToggleLP ? L"Tray Click: Landscape \u2194 Portrait" : L"Tray Click: Rotate Clockwise \u27F3");
            SaveSettings();
            InvalidateRect((HWND)l, NULL, FALSE);
        }
        else if (id >= ID_SC_NEXT && id <= ID_SC_APP) {
            int idx = id - 4000;
            bShortcutsState[idx] = !bShortcutsState[idx];
            ManageShortcut(idx, bShortcutsState[idx]);
            InvalidateRect((HWND)l, NULL, FALSE);
        }

        else if (id == ID_TRAY_RESTORE) { ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); Shell_NotifyIconW(NIM_DELETE, &nid); }
        else if (id == ID_TRAY_EXIT) { Shell_NotifyIconW(NIM_DELETE, &nid); DestroyWindow(h); }
        return 0;
    }

    case WM_CLOSE: {
        if (bCloseToTray && !bUpdateMode) { 
            ShowWindow(h, SW_HIDE); 
            Shell_NotifyIconW(NIM_ADD, &nid); 
            return 0; 
        }
        break;
    }

    case WM_TRAYICON: {
        if (l == WM_LBUTTONUP) { 
            if (bTrayToggleLP) {
                int target = (currentScreenRot % 2 == 0) ? 90 : 0;
                SetRot(target);
            } else {
                SetRot(-1); 
            }
            InvalidateRect(h, NULL, FALSE); 
        }
        else if (l == WM_RBUTTONUP) {
            POINT p; GetCursorPos(&p); HMENU hMenu = CreatePopupMenu();
            
            AppendMenuW(hMenu, MF_OWNERDRAW, 105, (LPCWSTR)L"Rotate Clockwise (Next)");
            AppendMenuW(hMenu, MF_SEPARATOR | MF_OWNERDRAW, 0, NULL);
            AppendMenuW(hMenu, MF_OWNERDRAW, 100, (LPCWSTR)L"Landscape");
            AppendMenuW(hMenu, MF_OWNERDRAW, 101, (LPCWSTR)L"Portrait");
            AppendMenuW(hMenu, MF_OWNERDRAW, 102, (LPCWSTR)L"Flipped Landscape");
            AppendMenuW(hMenu, MF_OWNERDRAW, 103, (LPCWSTR)L"Flipped Portrait");
            AppendMenuW(hMenu, MF_SEPARATOR | MF_OWNERDRAW, 0, NULL);
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_TRAY_RESTORE, (LPCWSTR)L"Restore Window");
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_TRAY_EXIT, (LPCWSTR)L"Exit");
            
            if (currentScreenRot >= 0 && currentScreenRot <= 3) CheckMenuItem(hMenu, 100 + currentScreenRot, MF_CHECKED);

            SetForegroundWindow(h); TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, h, NULL); DestroyMenu(hMenu);
        }
        else if (l == WM_LBUTTONDBLCLK) { ShowWindow(h, SW_SHOW); SetForegroundWindow(h); Shell_NotifyIconW(NIM_DELETE, &nid); }
        return 0;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DeleteObject(hFontBold); DeleteObject(hFontNormal); DeleteObject(hFontHeader);
        DeleteObject(hFontMenu);
        PostQuitMessage(0);
        return 0;
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HBRUSH bg = CreateSolidBrush(0xF0F0F0);
        FillRect(dc, &ps.rcPaint, bg);
        DeleteObject(bg);

        if (bSettingsMode) {
            HPEN hPen = CreatePen(PS_SOLID, S(2), RGB(180, 180, 180));
            SelectObject(dc, hPen);
            MoveToEx(dc, S(20), S(145), NULL);
            LineTo(dc, S(WIN_W) - S(32), S(145));
            DeleteObject(hPen);
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

bool IsArg(LPWSTR arg, const wchar_t* check) {
    while (*arg && *check) {
        if (MyToLower(*arg) != MyToLower(*check)) return false;
        arg++; check++;
    }
    return *arg == 0 && *check == 0;
}

extern "C" int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR c, int s) {
    CoInitialize(NULL);
    InitSettingsPath();

    if (GetFileAttributesW(iniPath) == INVALID_FILE_ATTRIBUTES) {
        SaveSettings();
    }

    LoadSettings();

    wchar_t safePath[MAX_PATH];
    if (GetStableExePath(safePath)) {
        wchar_t currentPath[MAX_PATH];
        GetModuleFileNameW(NULL, currentPath, MAX_PATH);
        if (lstrcmpiW(currentPath, safePath) != 0) {
            if (GetFileAttributesW(safePath) != INVALID_FILE_ATTRIBUTES) {
                 bUpdateMode = true;
            }
        }
    }
    
    if (bAutoStart) UpdateAutoStartRegistry(true);

    LPWSTR cmd = GetCommandLineW();
    bool q = false;
    while (*cmd) { if (*cmd == L'"') q = !q; else if (*cmd == L' ' && !q) { cmd++; break; } cmd++; }
    while (*cmd == L' ') cmd++;

    bool bSilentStart = false;
    if (*cmd != 0) {
        if (IsArg(cmd, L"-tray")) {
            bSilentStart = true;
        } 
        else {
            if (IsArg(cmd, L"next") || IsArg(cmd, L"rotate")) {
                SetRot(-1);
                GdiplusShutdown(gdiplusToken);
                ExitProcess(0);
            }

            int a = 0;
            bool valid = true;
            LPWSTR t = cmd;
            while (*t) {
                if (*t < L'0' || *t > L'9') { valid = false; break; }
                a = a * 10 + (*t - L'0');
                t++;
            }

            if (valid && (a == 0 || a == 90 || a == 180 || a == 270)) {
                SetRot(a);
                GdiplusShutdown(gdiplusToken);
                ExitProcess(0);
            } 
            
            wchar_t p[MAX_PATH];
            GetModuleFileNameW(NULL, p, MAX_PATH);
            wchar_t* n = p;
            for (wchar_t* t = p; *t; t++) if (*t == L'\\' || *t == L'/') n = t + 1;
            
            wchar_t msg[1024];
            wsprintfW(msg, 
                L"Usage:\n  .\\%s [angle] OR [next]\n\n"
                L"Examples:\n"
                L"  .\\%s next   (Rotate Clockwise)\n"
                L"  .\\%s 0      (Landscape)\n"
                L"  .\\%s 90     (Portrait)\n"
                L"  .\\%s 180    (Flipped Landscape)\n"
                L"  .\\%s 270    (Flipped Portrait)", 
                n, n, n, n, n, n);

            wchar_t title[256];
            wsprintfW(title, L" Error or Info? : %s", AppTitle);
            MessageBoxW(NULL, msg, title, MB_OK | MB_ICONINFORMATION);
            GdiplusShutdown(gdiplusToken);
            ExitProcess(0);
        }
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"ArKT_QuickRotate_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(AppClass, NULL);
        if (hExisting) {
            if (bUpdateMode) {
                SendMessageW(hExisting, WM_COMMAND, ID_TRAY_EXIT, 0);
                for(int i=0; i<20; i++) {
                    if (!FindWindowW(AppClass, NULL)) break;
                    Sleep(50);
                }
            } else {
                if (!bSilentStart) {
                    PostMessageW(hExisting, WM_COMMAND, ID_TRAY_RESTORE, 0);
                    SetForegroundWindow(hExisting);
                }
                CoUninitialize(); 
                ExitProcess(0); 
            }
        }
    }

    if (!bSilentStart && !bUpdateMode && bAutoStart) {
        wchar_t dummy[MAX_PATH];
        EnsureInstalled(dummy, false);
    }

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    int wSmall = GetSystemMetrics(SM_CXSMICON);
    int hSmall = GetSystemMetrics(SM_CYSMICON);
    int wBig   = GetSystemMetrics(SM_CXICON);
    int hBig   = GetSystemMetrics(SM_CYICON);

    hIconSm  = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, wSmall, hSmall, 0);
    hIconBig = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, wBig, hBig, 0);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = AppClass;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon = hIconBig;
    wc.hIconSm = hIconSm;
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    HDC screen = GetDC(NULL);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);

    hMainWnd = CreateWindowExW(WS_EX_TOPMOST, AppClass, AppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (sw - S(WIN_W)) / 2, (sh - S(WIN_H)) / 2, S(WIN_W), S(WIN_H), NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hMainWnd) {
        ReleaseMutex(hMutex);
        GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        ExitProcess(1);
    }

    SendMessageW(hMainWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessageW(hMainWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    
    if (bSilentStart) {
        Shell_NotifyIconW(NIM_ADD, &nid);
    } else {
        ShowWindow(hMainWnd, SW_SHOW);
    }
    
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    
    if (bUpdateMode) {
        wchar_t current[MAX_PATH]; GetModuleFileNameW(NULL, current, MAX_PATH);
        CopyFileW(current, safePath, FALSE); 
        if (bCloseToTray) {
            ShellExecuteW(NULL, L"open", safePath, L"-tray", NULL, SW_SHOW);
        }
    }

    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    CoUninitialize(); 
    ExitProcess(0);
    return 0;
}