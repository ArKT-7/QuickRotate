/*
  Quick Rotate v6.1 by ArKT | Modern Display Orientation Tool/Utility for Windows
  Copyright (c) 2026 ArKT-7 (https://github.com/ArKT-7/QuickRotate)
  Build: windres QuickRotate.rc -O coff -o QuickRotate_res.o; g++ QuickRotate.cpp QuickRotate_res.o -o QuickRotate.exe -static -nostartfiles -e _WinMain@16 -Os -s -fno-exceptions -fno-rtti -fno-stack-protector -fomit-frame-pointer "-Wl,--gc-sections" -lgdi32 -luser32 -lgdiplus -lshlwapi -lshell32 -lole32 -luuid -ladvapi32 -mwindows
*/

#define _WIN32_WINNT 0x0605
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include "version.h"
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef ODS_NOFOCUSRECT
#define ODS_NOFOCUSRECT 0x0200
#endif


const CLSID CLSID_ShellLink = {0x00021401, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
const IID IID_IShellLink    = {0x000214F9, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
const IID IID_IPersistFile  = {0x0000010b, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
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
#define ID_BTN_UPDATE     111
#define ID_BTN_DOWNLOAD   301

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

const wchar_t* AppTitle = L"Quick Rotate v6.1";
const wchar_t* AppClass = L"ArKT_QuickRotate";

HFONT hFontBold = NULL;
HFONT hFontNormal = NULL;
HFONT hFontHeader = NULL;
HFONT hFontTitle = NULL;

HICON hIconSm = NULL;
HICON hIconBig = NULL;

WNDPROC oldBtnProc;
HWND hHover = NULL;
HWND hMainWnd = NULL;
bool g_bShowFocus = false;

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
HWND hSetControls[12];

bool bUpdatePageMode = false;
HBRUSH g_hBrBkgnd = NULL;
HWND hLblStatus = NULL;
HWND hLblCurVer = NULL;
HWND hLblNewVer = NULL;
HWND hBtnDownload = NULL;
HWND hProgress = NULL;
wchar_t g_downloadUrl[512] = {0};
HMODULE g_hUrlMon = NULL, g_hWinInet = NULL;
typedef HRESULT (WINAPI *tUD)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPVOID);
typedef HRESULT (WINAPI *tOS)(LPUNKNOWN, LPCWSTR, IStream**, DWORD, LPVOID);
typedef BOOL    (WINAPI *tDC)(LPCWSTR);
tUD g_pDownload = NULL;
tOS g_pOpenStream = NULL;
tDC g_pDelCache = NULL;
int g_currentMonNum = 1;

const wchar_t* UPDATE_CHECK_URL = L"https://raw.githubusercontent.com/ArKT-7/QuickRotate/main/version.h";
const wchar_t* CURRENT_VER = VERSION_W;
void UpdateLayout(HWND h);
void PerformUpdateCheck(HWND h);
void PerformDownload(HWND h);

struct Threadupdt {
    HWND hWnd;
};

DWORD WINAPI CheckUpdateThread(LPVOID lpParam) {
    Threadupdt* x = (Threadupdt*)lpParam;
    if (x) {
        PerformUpdateCheck(x->hWnd);
        HeapFree(GetProcessHeap(), 0, x);
    }
    return 0;
}

DWORD WINAPI DownloadThread(LPVOID lpParam) {
    Threadupdt* x = (Threadupdt*)lpParam;
    if (x) {
        PerformDownload(x->hWnd);
        HeapFree(GetProcessHeap(), 0, x);
    }
    return 0;
}

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
            wnsprintfW(cmd, MAX_PATH + 20, L"\"%s\" -tray", exePath);
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
    wnsprintfW(outPath, MAX_PATH, L"%s\\%s.lnk", szDesktop, name);
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

struct MonData {
    HMONITOR hTarget;
    int foundIndex;
    int counter;
};

BOOL CALLBACK MonEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam) {
    MonData* p = (MonData*)lParam;
    p->counter++;
    if (hMon == p->hTarget) {
        p->foundIndex = p->counter;
    }
    return TRUE;
}

int GetLogicalMonitorIndex(HMONITOR hTarget) {
    MonData data = { hTarget, 0, 0 };
    EnumDisplayMonitors(NULL, NULL, MonEnumProc, (LPARAM)&data);
    return (data.foundIndex > 0) ? data.foundIndex : 1;
}

void GetCurrentDeviceName(wchar_t* deviceName) {
    deviceName[0] = 0;
    HMONITOR hMon = NULL;

    if (hMainWnd && IsWindowVisible(hMainWnd)) {
        hMon = MonitorFromWindow(hMainWnd, MONITOR_DEFAULTTONEAREST);
    } else {
        POINT pt;
        GetCursorPos(&pt);
        hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    }

    if (hMon) {
        MONITORINFOEXW mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, (LPMONITORINFO)&mi)) {
            lstrcpyW(deviceName, mi.szDevice);
        }
    }
}

bool IsNativePortrait(const wchar_t* device) {
    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(device && device[0] ? device : NULL, ENUM_CURRENT_SETTINGS, &dm)) {
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
    wchar_t dev[32];
    GetCurrentDeviceName(dev);
    const wchar_t* pDev = dev[0] ? dev : NULL;

    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(pDev, ENUM_CURRENT_SETTINGS, &dm)) {
        if (IsNativePortrait(pDev)) {
            currentScreenRot = (dm.dmDisplayOrientation + 1) % 4;
        } else {
            currentScreenRot = dm.dmDisplayOrientation;
        }
    }
}

void SetRot(int angle) {
    wchar_t dev[32];
    GetCurrentDeviceName(dev);
    const wchar_t* pDev = dev[0] ? dev : NULL;

    DEVMODEW dm = {0};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(pDev, ENUM_CURRENT_SETTINGS, &dm)) {
        if (angle == -1) {
            int current = dm.dmDisplayOrientation;
            int next = (current + 3) % 4; 
            angle = next * 90;
        }
        else {
            if (IsNativePortrait(pDev)) {
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
        ChangeDisplaySettingsExW(pDev, &dm, NULL, 0, NULL);
        if (IsNativePortrait(pDev)) currentScreenRot = (neu + 1) % 4;
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
    int id = GetDlgCtrlID(h);
    if (id == ID_CHK_TRAYMODE && m == WM_MOUSEMOVE) InvalidateRect(h, NULL, FALSE);
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

void ToggleUpdateView(HWND h, bool show) {
    bUpdatePageMode = show;
    UpdateLayout(h);

    int showSC = show ? SW_HIDE : SW_SHOW;
    for (int i = 3; i <= 7; i++) ShowWindow(hSetControls[i], showSC);
    ShowWindow(hSetControls[9], showSC);

    int showUpd = show ? SW_SHOW : SW_HIDE;
    ShowWindow(hLblStatus, showUpd);
    ShowWindow(hLblCurVer, showUpd);
    ShowWindow(hLblNewVer, showUpd);
    ShowWindow(hProgress, showUpd);
    
    if (!show) ShowWindow(hBtnDownload, SW_HIDE);
    if (show) {
        Threadupdt* params = (Threadupdt*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Threadupdt));
        params->hWnd = h;
        CreateThread(NULL, 0, CheckUpdateThread, params, 0, NULL);
    }
    
    InvalidateRect(h, NULL, TRUE);
}

void ToggleViewMode(HWND h) {
    if (bUpdatePageMode) {
        ToggleUpdateView(h, false);
        return;
    }

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
    for (int i=0; i<12; i++) ShowWindow(hSetControls[i], showSet);

    SetWindowPos(hSetControls[10], NULL, S(BTN_X), S(426), S(BTN_W), S(25), SWP_NOZORDER);
    SendMessageW(hSetControls[10], WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    InvalidateRect(h, NULL, TRUE);

    if (bSettingsMode) {
        SetFocus(hSetControls[0]); 
    } else {
        SetFocus(hBtnRot[0]); 
    }
}

HFONT MakeFont(int size, int weight) { 
    return CreateFontW(S(size),0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,0,FF_SWISS,L"Segoe UI"); 
}

void RecreateFonts() {
    if (hFontTitle) DeleteObject(hFontTitle);
    if (hFontBold) DeleteObject(hFontBold);
    if (hFontNormal) DeleteObject(hFontNormal);
    if (hFontHeader) DeleteObject(hFontHeader);
    
    hFontBold   = MakeFont(21, FW_BOLD);
    hFontNormal = MakeFont(17, FW_NORMAL);
    hFontHeader = MakeFont(19, FW_BOLD);
    hFontTitle  = MakeFont(27, FW_BOLD);
}

void UpdateLayout(HWND h) {
    RecreateFonts();

    auto mv = [](HWND w, int y, int h, int wd = BTN_W, int x = BTN_X) { 
        SetWindowPos(w, NULL, S(x), S(y), S(wd), S(h), SWP_NOZORDER); 
    };

    for (int i = 0; i < 5; i++) mv(hBtnRot[i], 20 + (i * 75), BTN_H);
    mv(hBtnSettings, 390, BTN_SH);
    int half = (BTN_W - 10) / 2;
    mv(hSetControls[0], 390, BTN_SH, half);
    mv(hSetControls[11], 390, BTN_SH, half, BTN_X + half + 10);

    mv(hSetControls[1], 20, 30);
    mv(hSetControls[2], 60, 30);
    mv(hSetControls[8], 100, 30);
    mv(hSetControls[9], 160, 30);

    for (int i = 0; i < 5; i++) mv(hSetControls[3+i], 198 + (i * 38), 30);
    mv(hSetControls[10], 426, 25);

    if (bUpdatePageMode) {
        mv(hLblStatus, 160, 35);
        mv(hLblCurVer, 200, 30);
        mv(hLblNewVer, 230, 30);
        mv(hProgress, 275, 40);
        mv(hBtnDownload, 320, 60);
    }
    
    InvalidateRect(h, NULL, TRUE);
}

void DrawProIcon(Graphics& g, int id, int x, int y, int s, Color c, bool isFilled) {
    Pen pen(c, S(2));
    SolidBrush brush(c);
    GraphicsPath p;

    if (id >= 100 && id <= 103) {
        int ss = s * 0.67, pad = (s - ss) / 2;
        Rect r = (id % 2 == 0) ? Rect(x, y + pad, s, ss) : Rect(x + pad, y, ss, s);
        int gw = S(1), L = r.X, T = r.Y, R = r.GetRight(), B = r.GetBottom();
        int cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;

        if (id == 100) {
            p.AddLine(R, cy + gw, R, B); p.AddLine(R, B, L, B); 
            p.AddLine(L, B, L, T); p.AddLine(L, T, R, T); p.AddLine(R, T, R, cy - gw);
        } else if (id == 102) {
            p.AddLine(L, cy - gw, L, T); p.AddLine(L, T, R, T); 
            p.AddLine(R, T, R, B); p.AddLine(R, B, L, B); p.AddLine(L, B, L, cy + gw);
        } else if (id == 103) {
            p.AddLine(cx + gw, T, R, T); p.AddLine(R, T, R, B); 
            p.AddLine(R, B, L, B); p.AddLine(L, B, L, T); p.AddLine(L, T, cx - gw, T);
        } else {
            p.AddLine(cx + gw, B, R, B); p.AddLine(R, B, R, T); 
            p.AddLine(R, T, L, T); p.AddLine(L, T, L, B); p.AddLine(L, B, cx - gw, B);
        }
        if (isFilled) g.FillPath(&brush, &p); else g.DrawPath(&pen, &p);
    }
    else if (id == 105) {
        int k = S(1); 
        Rect r(x + k, y + k, s - 2*k, s - 2*k);
        pen.SetEndCap(LineCapArrowAnchor);
        g.DrawArc(&pen, r, 0, 290);
    }
    else if (id == ID_TRAY_RESTORE) { 
        Rect r(x + S(1), y + S(-1), s - S(3), s - S(-1));
        g.DrawRectangle(&pen, r);
        g.DrawLine(&pen, x + S(1), y + S(5), x + s - S(2), y + S(5));
    }
    else if (id == ID_TRAY_EXIT) {
        int p = S(2); 
        g.DrawLine(&pen, x + p, y + p, x + s - p, y + s - p);
        g.DrawLine(&pen, x + s - p, y + p, x + p, y + s - p);
    }
}

void GetVal(char* src, const char* key, wchar_t* out) {
    char* p = StrStrA(src, key);
    if (p && (p = StrChrA(p, '\"'))) {
        char* e = StrChrA(++p, '\"');
        if (e) {
            *e = 0;
            MultiByteToWideChar(CP_UTF8, 0, p, -1, out, 512);
            *e = '\"';
        }
    }
}

int ParseVerNum(const wchar_t*& p) {
    int v = 0;
    while (*p >= L'0' && *p <= L'9') { v = v * 10 + (*p - L'0'); p++; }
    if (*p == L'.') p++;
    return v;
}

int CompareVersion(const wchar_t* v1, const wchar_t* v2) {
    const wchar_t *p1 = v1, *p2 = v2;
    while (*p1 || *p2) {
        int n1 = ParseVerNum(p1);
        int n2 = ParseVerNum(p2);
        if (n1 != n2) return n1 - n2;
    }
    return 0;
}

void PerformUpdateCheck(HWND h) {
    if (!g_pDownload) { SetWindowTextW(hLblStatus, L"Error: Missing DLL"); return; }

    ShowWindow(hBtnDownload, SW_HIDE);
    SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
    SetWindowTextW(hLblStatus, L"Checking...");
    SetWindowTextW(hLblCurVer, L"");
    SetWindowTextW(hLblNewVer, L"");
    SetWindowTextW(hProgress, L"Please wait...");

    if (g_pDelCache) g_pDelCache(UPDATE_CHECK_URL); 

    wchar_t tmp[MAX_PATH], path[MAX_PATH], url[512];
    GetTempPathW(MAX_PATH, tmp);
    wnsprintfW(path, MAX_PATH, L"%sQR_v.h", tmp);
    wnsprintfW(url, 512, L"%s?t=%lu", UPDATE_CHECK_URL, GetTickCount());

    HRESULT hr = E_FAIL;
    for (int i = 0; i < 2; i++) {
        hr = g_pDownload(NULL, url, path, 0, NULL);
        if (SUCCEEDED(hr)) break;
        Sleep(100);
    }

    if (SUCCEEDED(hr)) {
        HANDLE hF = CreateFileW(path, GENERIC_READ, 1, NULL, 3, 0, NULL);
        if (hF != INVALID_HANDLE_VALUE) {
            char buf[4096] = {0}; DWORD br;
            ReadFile(hF, buf, 4095, &br, NULL);
            CloseHandle(hF); DeleteFileW(path);
            wchar_t ver[32] = {0}, rbS[8] = {0};
            GetVal(buf, "VERSION_W", ver);
            GetVal(buf, "DOWNLOAD_URL", g_downloadUrl);
            GetVal(buf, "BUILD", rbS);
            int remB = (int)wcstol(rbS, NULL, 10);
            int locB = atoi(BUILD);
            SendMessageW(hLblCurVer, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            wchar_t cur[64]; wnsprintfW(cur, 64, L"Current: %s", VERSION_W);
            SetWindowTextW(hLblCurVer, cur);
            int verDiff = CompareVersion(ver, VERSION_W);
            bool isNewer = (verDiff > 0) || (verDiff == 0 && remB > locB);

            if (ver[0] == 0 || g_downloadUrl[0] == 0) {
                SetWindowTextW(hLblStatus, L"Update Error");
                SetWindowTextW(hProgress, L"Invalid server data.");
            } 
            else if (isNewer) {
                SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
                SetWindowTextW(hLblStatus, L"Update Available!");
                SendMessageW(hLblNewVer, WM_SETFONT, (WPARAM)hFontBold, TRUE);
                wchar_t neu[64]; 
                if (verDiff == 0 && remB > locB) wnsprintfW(neu, 64, L"New: %s (Rev %d)", ver, remB);
                else wnsprintfW(neu, 64, L"New: %s", ver);
                SetWindowTextW(hLblNewVer, neu);
                SendMessageW(hProgress, WM_SETFONT, (WPARAM)hFontBold, TRUE);
                SetWindowTextW(hProgress, L"Ready to Download");
                ShowWindow(hBtnDownload, SW_SHOW);
                EnableWindow(hBtnDownload, TRUE);
            } else {
                SetWindowTextW(hLblStatus, L"You are up to date");
                SetWindowTextW(hLblNewVer, L""); SetWindowTextW(hProgress, L"");
            }
        } else {
            SetWindowTextW(hLblStatus, L"Check Failed");
            SetWindowTextW(hProgress, L"Could not read file.");
        }
    } else {
        SetWindowTextW(hLblStatus, L"Connection Error");
        SetWindowTextW(hProgress, L"Click 'Check Update' to retry");
    }
}

void PerformDownload(HWND h) {
    if (!g_pOpenStream) return;

    EnableWindow(hBtnDownload, FALSE);
    InvalidateRect(hBtnDownload, NULL, FALSE);
    SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
    SetWindowTextW(hLblStatus, L"Starting Download...");
    SendMessageW(hProgress, WM_SETFONT, (WPARAM)hFontBold, TRUE);
    SetWindowTextW(hProgress, L"Initializing...");

    IStream* pStream = NULL;
    if (SUCCEEDED(g_pOpenStream(NULL, g_downloadUrl, &pStream, 0, NULL))) {
        wchar_t tmp[MAX_PATH], newE[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        wnsprintfW(newE, MAX_PATH, L"%sQR_Upd.tmp", tmp);
        
        HANDLE hOut = CreateFileW(newE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        
        STATSTG stat; pStream->Stat(&stat, STATFLAG_NONAME);
        DWORD total = stat.cbSize.LowPart;
        DWORD totalRead = 0, read; char buffer[4096];
        int lastPct = -1;

        while (true) {
            pStream->Read(buffer, sizeof(buffer), &read);
            if (read == 0) break;
            WriteFile(hOut, buffer, read, &read, NULL);
            totalRead += read;

            if (total > 0) {
                int pct = (totalRead * 100) / total;
                if (pct != lastPct) {
                    lastPct = pct;
                    wchar_t progText[128];
                    wnsprintfW(progText, 128, L"Downloading: %d KB / %d KB (%d%%)",
                        totalRead / 1024, total / 1024, pct);
                    SetWindowTextW(hProgress, progText);
                }
            }
        }
        CloseHandle(hOut); pStream->Release();

        SetWindowTextW(hLblStatus, L"Installing Update...");
        SetWindowTextW(hProgress, L"Restarting app...");
        Sleep(800);

        wchar_t cur[MAX_PATH], old[MAX_PATH]; GetModuleFileNameW(NULL, cur, MAX_PATH);
        lstrcpyW(old, cur); PathRemoveFileSpecW(old); PathAppendW(old, L"QuickRotate.old");
        DeleteFileW(old);
        
        if (MoveFileW(cur, old) && MoveFileW(newE, cur)) {
            ShellExecuteW(NULL, L"open", cur, NULL, NULL, SW_SHOW); ExitProcess(0);
        } else {
            MoveFileW(old, cur);
            SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontHeader, TRUE);
            SetWindowTextW(hLblStatus, L"Installation Failed");
            SetWindowTextW(hProgress, L"Could not replace file.");
            EnableWindow(hBtnDownload, TRUE);
        }
    } else {
        SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontHeader, TRUE);
        SetWindowTextW(hLblStatus, L"Download Failed");
        EnableWindow(hBtnDownload, TRUE);
    }
}

void MoveToMonitorCenter(HWND h, HMONITOR hMon) {
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfoW(hMon, &mi)) return;
    SetWindowPos(h, NULL, mi.rcWork.left, mi.rcWork.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    int w = S(WIN_W);
    int height = S(WIN_H);
    int monW = mi.rcWork.right - mi.rcWork.left;
    int monH = mi.rcWork.bottom - mi.rcWork.top;
    int x = mi.rcWork.left + (monW - w) / 2;
    int y = mi.rcWork.top + (monH - height) / 2;
    SetWindowPos(h, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE);
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
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
        g_currentMonNum = GetLogicalMonitorIndex(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST));

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

        hBtnSettings = CreateMyButton(h, L"\u2699 Settings", ID_BTN_SETTINGS, BTN_X, 390, BTN_W, BTN_SH, WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP);

        int halfW = (BTN_W - 10) / 2;
        hSetControls[0] = CreateMyButton(h, L"\u2B05 Back", ID_BTN_BACK, BTN_X, 390, halfW, BTN_SH, BS_PUSHBUTTON | WS_TABSTOP);
        hSetControls[11] = CreateMyButton(h, L"Check Update", ID_BTN_UPDATE, BTN_X + halfW + 10, 390, halfW, BTN_SH, WS_TABSTOP);
        struct TogData { int idx; LPCWSTR txt; int id; int y; };
        LPCWSTR trayText = bTrayToggleLP ? L"Tray Click: Landscape \u2194 Portrait" : L"Tray Click: Cycle Rotation (Next \u27F3)";
        
        TogData togs[] = {
            {1, L"Minimize to Tray on Close", ID_CHK_TRAY, 20},
            {2, L"Start with Windows", ID_CHK_AUTOSTART, 60},
            {8, trayText, ID_CHK_TRAYMODE, 100},
            {9, L"Shortcut: Quick Rotate App", ID_SC_APP, 160}
        };

        for (int i = 0; i < 4; i++) {
            hSetControls[togs[i].idx] = CreateMyButton(h, togs[i].txt, togs[i].id, 
                BTN_X, togs[i].y, BTN_W, 30, WS_TABSTOP);
        }

        LPCWSTR scTxt[] = {
            L"Shortcut: Rotate Clockwise",
            L"Shortcut: Landscape",
            L"Shortcut: Portrait",
            L"Shortcut: Flipped Landscape",
            L"Shortcut: Flipped Portrait"
        };
        int scIds[] = { ID_SC_NEXT, ID_SC_LANDSCAPE, ID_SC_PORTRAIT, ID_SC_FLIPPED, ID_SC_FLIPPORT };
        
        for (int i = 0; i < 5; i++) {
            hSetControls[3+i] = CreateMyButton(h, scTxt[i], scIds[i], BTN_X, 198 + (i * 38), BTN_W, 30, WS_TABSTOP);
        }

        hSetControls[10] = NULL;
        
        hLblStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        hLblCurVer = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SendMessageW(hLblCurVer, WM_SETFONT, (WPARAM)hFontHeader, TRUE);

        hLblNewVer = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SendMessageW(hLblNewVer, WM_SETFONT, (WPARAM)hFontHeader, TRUE);

        hProgress = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SendMessageW(hProgress, WM_SETFONT, (WPARAM)hFontHeader, TRUE);

        hBtnDownload = CreateMyButton(h, L"Download && Install", ID_BTN_DOWNLOAD, 0, 0, 0, 0, BS_PUSHBUTTON);

        nid.cbSize = sizeof(NOTIFYICONDATAW); nid.hWnd = h; nid.uID = 1001; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIconSm;
        
        wnsprintfW(nid.szTip, 128, L"%s", AppTitle);
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

    case WM_MOVE: {
        static HMONITOR hLastMon = NULL;
        HMONITOR hNewMon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
        if (hNewMon != hLastMon) {
            hLastMon = hNewMon;
            UpdateCurrentRotation();
            g_currentMonNum = GetLogicalMonitorIndex(hNewMon);
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }

    case WM_DISPLAYCHANGE: {
        UpdateCurrentRotation();
        if (IsWindowVisible(h)) {
            HMONITOR hMon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
            MoveToMonitorCenter(h, hMon);
            g_currentMonNum = GetLogicalMonitorIndex(hMon);
        }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)w; SetBkColor(hdcStatic, 0xF0F0F0); SetBkMode(hdcStatic, TRANSPARENT); 
        return (INT_PTR)g_hBrBkgnd;
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
        Graphics* g = buf.g;

        if (p->CtlType == ODT_BUTTON) {
            bool pressed = (p->itemState & ODS_SELECTED);
            bool hovered = (p->hwndItem == hHover);
            bool focused = (p->itemState & ODS_FOCUS);
            int btnId = p->CtlID;

            if (btnId == ID_CHK_TRAYMODE) {
                g->Clear(Color(240, 240, 240)); 
                wchar_t buf_t[64]; GetWindowTextW(p->hwndItem, buf_t, 64);
                SetBkMode(buf.hMemDC, 1); SetTextColor(buf.hMemDC, 0); SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(5), 0, w - S(50), h}; DrawTextW(buf.hMemDC, buf_t, -1, &tr, 36); 
                int bW = S(44), bH = S(24); Rect rB(w - bW - S(2), (h - bH) / 2, bW, bH);
                bool hv = (hHover == p->hwndItem);
                Color cB = pressed ? Color(0, 60, 120) : (hv ? Color(0, 140, 235) : Color(0, 120, 215));
                GraphicsPath ph; GetRoundedRectPath(&ph, rB, S(24));
                SolidBrush br(cB); g->FillPath(&br, &ph);
                Pen pn(Color(255, 255, 255), S(2));
                pn.SetStartCap(LineCapRound); pn.SetEndCap(LineCapRound);
                int cx = rB.X + bW/2, cy = rB.Y + bH/2, f = S(3);
                g->DrawLine(&pn, cx-f, cy-f, cx+f-S(1), cy); g->DrawLine(&pn, cx+f-S(1), cy, cx-f, cy+f);

                if (focused && g_bShowFocus) {
                    Pen fp(Color(150, 150, 150), 1); fp.SetDashStyle(DashStyleDot);
                    g->DrawRectangle(&fp, rB);
                }
            } else if (btnId >= 200 && btnId != ID_BTN_BACK && btnId != ID_TRAY_RESTORE && btnId != ID_TRAY_EXIT && btnId != ID_BTN_DOWNLOAD) {
                bool isChecked = false;
                if (btnId == ID_CHK_TRAY) isChecked = bCloseToTray;
                else if (btnId == ID_CHK_AUTOSTART) isChecked = bAutoStart;
                else if (btnId >= 4000) isChecked = bShortcutsState[btnId - 4000];

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, 0); SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(5), 0, w - S(55), h}; DrawTextW(buf.hMemDC, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                int tH = S(22), tW = S(44), tX = w - tW - S(2), tY = (h - tH) / 2;
                GraphicsPath path; path.AddArc(tX, tY, tH, tH, 90, 180); path.AddArc(tX + tW - tH, tY, tH, tH, 270, 180); path.CloseFigure();
                Color tc = isChecked ? (hovered ? Color(0, 140, 235) : Color(0, 120, 215)) : (hovered ? Color(195, 195, 195) : Color(180, 180, 180));
                SolidBrush tb(tc); g->FillPath(&tb, &path);
                SolidBrush wb(Color(255, 255, 255));
                g->FillEllipse(&wb, isChecked ? (tX + tW - S(16) - S(3)) : (tX + S(3)), tY + (tH - S(16)) / 2, S(16), S(16));

                if (focused && g_bShowFocus) {
                    Pen focusPen(Color(100, 100, 100), 1);
                    focusPen.SetDashStyle(DashStyleDot);
                    g->DrawRectangle(&focusPen, S(2), S(2), w - S(4), h - S(4));
                }

            } else {
                Color bg, txt;
                bool active = (btnId - 100 == currentScreenRot) && (btnId < 200);
                bool disabled = (p->itemState & ODS_DISABLED);

                if (btnId == ID_BTN_SETTINGS || btnId == ID_BTN_BACK || btnId == ID_BTN_UPDATE || btnId == ID_BTN_DOWNLOAD) {
                    if (disabled) { bg = Color(200, 200, 200); txt = Color(160, 160, 160); }
                    else if (pressed) { bg = Color(60, 60, 60); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(120, 120, 120); txt = Color(255, 255, 255); }
                    else { bg = Color(80, 80, 80); txt = Color(255, 255, 255); }
                } else {
                    if (active && !hovered) { bg = Color(255, 255, 255); txt = Color(0, 120, 215); }
                    else if (pressed) { bg = Color(0, 80, 160); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(135, 206, 250); txt = Color(0, 60, 140); }
                    else { bg = Color(0, 120, 215); txt = Color(255, 255, 255); }
                }
                if (btnId == ID_BTN_DOWNLOAD && !disabled) {
                    if (pressed) { bg = Color(0, 80, 160); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(0, 100, 200); txt = Color(255, 255, 255); }
                    else { bg = Color(0, 120, 215); txt = Color(255, 255, 255); }
                }

                GraphicsPath path; Rect r(S(2), S(2), w - S(4), h - S(4));
                GetRoundedRectPath(&path, r, S(CORNER_RADIUS) * 2);
                SolidBrush b(bg); g->FillPath(&b, &path);

                if (btnId >= 100 && btnId <= 105) {
                    DrawProIcon(*g, btnId, S(15), (h - S(20)) / 2, S(20), txt, active);
                }
                
                if (active) {
                    Pen p(hovered ? Color(255, 255, 255) : Color(0, 120, 215), S(3));
                    p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound);
                    int tx = w - S(40), ty = h / 2;
                    g->DrawLine(&p, tx, ty, tx + S(5), ty + S(5)); g->DrawLine(&p, tx + S(5), ty + S(5), tx + S(14), ty - S(6));
                }

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, txt.ToCOLORREF());
                if (btnId == ID_BTN_DOWNLOAD) SelectObject(buf.hMemDC, hFontTitle);
                else SelectObject(buf.hMemDC, hFontBold);
                RECT tr = {0, 0, w, h};
                if (btnId >= 200 || btnId == ID_BTN_SETTINGS || btnId == ID_BTN_DOWNLOAD || btnId == ID_BTN_UPDATE) {
                    DrawTextW(buf.hMemDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                else {
                    RECT cr = tr; DrawTextW(buf.hMemDC, text, -1, &cr, DT_CALCRECT | DT_CENTER | DT_WORDBREAK);
                    tr.top += (h - (cr.bottom - cr.top)) / 2; DrawTextW(buf.hMemDC, text, -1, &tr, DT_CENTER | DT_WORDBREAK);
                }

                if (focused && g_bShowFocus) {
                    Pen focusPen(pressed ? Color(200, 200, 200) : Color(255, 255, 255), 1);
                    focusPen.SetDashStyle(DashStyleDot);
                    GraphicsPath focusPath; 
                    Rect fr(S(5), S(5), w - S(10), h - S(10));
                    GetRoundedRectPath(&focusPath, fr, S(CORNER_RADIUS) * 2 - S(4));
                    g->DrawPath(&focusPen, &focusPath);
                }
            }
        } else if (p->CtlType == ODT_MENU) {
            g->Clear(Color(255, 255, 255));
            if (p->itemID == 0) {
                Pen pen(Color(220, 220, 220), 1); g->DrawLine(&pen, S(10), h / 2, w - S(10), h / 2);
            } else {
                if (p->itemState & ODS_SELECTED) {
                    Rect r(S(4), S(2), w - S(8), h - S(4));
                    GraphicsPath path; GetRoundedRectPath(&path, r, S(8));
                    SolidBrush b(Color(230, 243, 255)); g->FillPath(&b, &path);
                }
                bool chk = (p->itemState & ODS_CHECKED);
                DrawProIcon(*g, p->itemID, S(12), (h - S(16)) / 2, S(16), chk ? Color(0, 120, 215) : Color(150, 150, 150), chk);

                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, 0); SelectObject(buf.hMemDC, hFontHeader);
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
        else if (id == ID_BTN_SETTINGS || id == ID_BTN_BACK) {
            if (id == ID_BTN_BACK && bUpdatePageMode) ToggleUpdateView(h, false);
            else ToggleViewMode(h);
        }
        else if (id == ID_BTN_UPDATE) { ToggleUpdateView(h, true); }
        else if (id == ID_BTN_DOWNLOAD) { 
            Threadupdt* params = (Threadupdt*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Threadupdt));
            params->hWnd = h;
            CreateThread(NULL, 0, DownloadThread, params, 0, NULL);
        }
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
            SetWindowTextW(hSetControls[8], bTrayToggleLP ? L"Tray Click: Landscape \u2194 Portrait" : L"Tray Click: Cycle Rotation (Next \u27F3)");
            SaveSettings();
            InvalidateRect((HWND)l, NULL, FALSE);
        }
        else if (id >= ID_SC_NEXT && id <= ID_SC_APP) {
            int idx = id - 4000;
            bShortcutsState[idx] = !bShortcutsState[idx];
            ManageShortcut(idx, bShortcutsState[idx]);
            InvalidateRect((HWND)l, NULL, FALSE);
        }

        else if (id == ID_TRAY_RESTORE) { 
            POINT pt; GetCursorPos(&pt);
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MoveToMonitorCenter(h, hMon);
            ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
        }
        else if (id == ID_TRAY_EXIT) { Shell_NotifyIconW(NIM_DELETE, &nid); DestroyWindow(h); }
        return 0;
    }

    case WM_CLOSE: {
        if (bCloseToTray && !bUpdateMode) { 
            ShowWindow(h, SW_HIDE); 
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
        else if (l == WM_LBUTTONDBLCLK) { 
            POINT pt; GetCursorPos(&pt);
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MoveToMonitorCenter(h, hMon);
            ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
        }
        return 0;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DeleteObject(hFontBold); DeleteObject(hFontNormal); DeleteObject(hFontHeader);
        DeleteObject(g_hBrBkgnd);
        PostQuitMessage(0);
        return 0;
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HBRUSH bg = CreateSolidBrush(0xF0F0F0);
        FillRect(dc, &ps.rcPaint, bg);
        DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, hFontHeader);
        SetTextColor(dc, RGB(160, 160, 160));

        if (bSettingsMode) {
            HPEN hPen = CreatePen(PS_SOLID, S(2), RGB(200, 200, 200));
            SelectObject(dc, hPen);
            MoveToEx(dc, S(20), S(145), NULL);
            LineTo(dc, S(WIN_W) - S(32), S(145));
            DeleteObject(hPen);
            wchar_t verText[64];
            wnsprintfW(verText, 64, L"Quick Rotate %s by ArKT", CURRENT_VER);
            RECT tr = {S(BTN_X), S(430), S(BTN_X) + S(BTN_W), S(480)};
            DrawTextW(dc, verText, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);
        } 
        else if (!bUpdatePageMode) {
            wchar_t statusText[64];
            wnsprintfW(statusText, 64, L"Active Monitor: %d", g_currentMonNum);
            RECT tr = {0, S(430), S(WIN_W), S(480)}; 
            DrawTextW(dc, statusText, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);
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

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    g_hUrlMon = LoadLibraryW(L"urlmon.dll");
    if (g_hUrlMon) {
        g_pDownload = (tUD)GetProcAddress(g_hUrlMon, "URLDownloadToFileW");
        g_pOpenStream = (tOS)GetProcAddress(g_hUrlMon, "URLOpenBlockingStreamW");
    }
    g_hWinInet = LoadLibraryW(L"wininet.dll");
    if (g_hWinInet) g_pDelCache = (tDC)GetProcAddress(g_hWinInet, "DeleteUrlCacheEntryW");
    g_hBrBkgnd = CreateSolidBrush(0xF0F0F0);
    InitSettingsPath();

    if (GetFileAttributesW(iniPath) == INVALID_FILE_ATTRIBUTES) {
        SaveSettings();
    }

    LoadSettings();

    wchar_t exePath[MAX_PATH], oldPath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    lstrcpyW(oldPath, exePath);
    PathRemoveFileSpecW(oldPath);
    PathAppendW(oldPath, L"QuickRotate.old");
    DeleteFileW(oldPath);

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
                CoUninitialize();
                ExitProcess(0);
            }

            wchar_t* endPtr;
            int a = (int)wcstol(cmd, &endPtr, 10);
            bool isValidNum = (cmd != endPtr);

            if (isValidNum && (a == 0 || a == 90 || a == 180 || a == 270)) {
                SetRot(a);
                GdiplusShutdown(gdiplusToken);
                CoUninitialize();
                ExitProcess(0);
            } 
            
            wchar_t p[MAX_PATH];
            GetModuleFileNameW(NULL, p, MAX_PATH);
            wchar_t* n = p;
            for (wchar_t* t = p; *t; t++) if (*t == L'\\' || *t == L'/') n = t + 1;
            
            wchar_t msg[1024];
            wnsprintfW(msg, 1024,
                L"Usage:\n  .\\%s [angle] OR [next]\n\n"
                L"Examples:\n"
                L"  .\\%s next   (Rotate Clockwise)\n"
                L"  .\\%s 0      (Landscape)\n"
                L"  .\\%s 90     (Portrait)\n"
                L"  .\\%s 180    (Flipped Landscape)\n"
                L"  .\\%s 270    (Flipped Portrait)", 
                n, n, n, n, n, n);

            wchar_t title[256];
            wnsprintfW(title, 256, L" Error or Info? : %s", AppTitle);
            MessageBoxW(NULL, msg, title, MB_OK | MB_ICONINFORMATION);
            GdiplusShutdown(gdiplusToken);
            CoUninitialize();
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
                GdiplusShutdown(gdiplusToken);
                CoUninitialize(); 
                ExitProcess(0); 
            }
        }
    }

    if (!bSilentStart && !bUpdateMode && bAutoStart) {
        wchar_t dummy[MAX_PATH];
        EnsureInstalled(dummy, false);
    }

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
    wc.hbrBackground = g_hBrBkgnd;
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
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    if (!bSilentStart) {
        POINT pt; GetCursorPos(&pt);
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MoveToMonitorCenter(hMainWnd, hMon);
        ShowWindow(hMainWnd, SW_SHOW);
    }
    
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
            g_bShowFocus = true;
        } else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN) {
            g_bShowFocus = false;
        }

        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            msg.wParam = VK_SPACE; 
        } else if (msg.message == WM_KEYUP && msg.wParam == VK_RETURN) {
            msg.wParam = VK_SPACE;
        }

        if (!IsDialogMessage(hMainWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    
    if (bUpdateMode) {
        wchar_t current[MAX_PATH]; GetModuleFileNameW(NULL, current, MAX_PATH);
        CopyFileW(current, safePath, FALSE); 
        if (bCloseToTray) {
            ShellExecuteW(NULL, L"open", safePath, L"-tray", NULL, SW_SHOW);
        }
    }

    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    if (g_hUrlMon) FreeLibrary(g_hUrlMon);
    if (g_hWinInet) FreeLibrary(g_hWinInet);
    CoUninitialize(); 
    ExitProcess(0);
    return 0;
}