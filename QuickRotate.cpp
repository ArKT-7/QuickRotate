/*
  Quick Rotate v6.2 by ArKT | Modern Display Orientation Tool/Utility for Windows
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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef WS_EX_COMPOSITED
#define WS_EX_COMPOSITED 0x02000000L
#endif

const CLSID CLSID_ShellLink = {0x00021401, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
const IID IID_IShellLink    = {0x000214F9, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
const IID IID_IPersistFile  = {0x0000010b, 0, 0, {0xC0,0,0,0,0,0,0,0x46}};
int g_dpi = 96;

int S(int value) {
    if (g_dpi <= 0) g_dpi = 96;
    return MulDiv(value, g_dpi, 96);
}

static inline void SetCtrlFont(HWND h, HFONT f) {
    SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}

static inline void ForceRedraw(HWND h, bool children = false) {
    UINT flags = RDW_INVALIDATE | RDW_UPDATENOW;
    if (children) flags |= RDW_ALLCHILDREN;
    RedrawWindow(h, NULL, NULL, flags);
}

using namespace Gdiplus;

#define WIN_W  338
#define WIN_H  480
#define BTN_W  290
#define BTN_H  60
#define BTN_SH 40
#define BTN_X  22
#define CORNER_RADIUS 10

#define SETTINGS_Y (20 + (5 * (BTN_H + 15)) - 5)
#define STATUS_Y   (SETTINGS_Y + BTN_SH)

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
#define ID_CHK_THEME      204

#define ID_SC_NEXT        4000
#define ID_SC_LANDSCAPE   4001
#define ID_SC_PORTRAIT    4002
#define ID_SC_FLIPPED     4003
#define ID_SC_FLIPPORT    4004
#define ID_SC_APP         4005

#define APP_NAME_STR L"Quick Rotate"

const wchar_t* AppName  = APP_NAME_STR;
const wchar_t* AppTitle = L"Quick Rotate v6.2";
const wchar_t* AppClass = L"ArKT_QuickRotate";
const wchar_t* SC_NAMES[] = { L"Rotate Screen Clockwise", L"Set Landscape", L"Set Portrait", L"Set Flipped Landscape", L"Set Flipped Portrait", APP_NAME_STR };

HFONT hFontBold = NULL;
HFONT hFontNormal = NULL;
HFONT hFontHeader = NULL;
HFONT hFontTitle = NULL;

HICON hIconSm = NULL;
HICON hIconBig = NULL;

WNDPROC oldBtnProc = NULL;
WNDPROC oldstaticProc = NULL;
HWND hHover = NULL;
HWND hMainWnd = NULL;
bool g_bShowFocus = false;
bool b_IsDarktheme = false;

ULONG_PTR gdiplusToken;
int currentScreenRot = -1;
NOTIFYICONDATAW nid = {0};
bool bCloseToTray = true;
bool bAutoStart = false;
bool bSettingsMode = false;
bool bUpdateMode = false;
int bTrayToggleLP = 1;
int bThemeMode = 0;
wchar_t g_appDir[MAX_PATH];
wchar_t g_exePath[MAX_PATH];
wchar_t g_currentPath[MAX_PATH];
wchar_t g_startMenuPath[MAX_PATH];
wchar_t g_desktopPath[MAX_PATH];
bool bShortcutsState[6] = {0};

HWND hBtnRot[5];
HWND hBtnSettings;
HWND hSetControls[13];

bool bUpdatePageMode = false;
HBRUSH g_hBrBkgnd = NULL;
HWND hLblStatus = NULL;
HWND hLblCurVer = NULL;
HWND hLblNewVer = NULL;
HWND hBtnDownload = NULL;
HWND hProgress = NULL;
wchar_t g_downloadUrl[512] = {0};
HMODULE g_hUrlMon = NULL, g_hWinInet = NULL, g_hDwm = NULL;
typedef HRESULT (WINAPI *tUD)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPVOID);
typedef HRESULT (WINAPI *tOS)(LPUNKNOWN, LPCWSTR, IStream**, DWORD, LPVOID);
typedef BOOL    (WINAPI *tDC)(LPCWSTR);
typedef HRESULT (WINAPI *tDWM)(HWND, DWORD, LPCVOID, DWORD);
tDWM g_pfnDwmSetAttr = NULL;
tUD g_pDownload = NULL;
tOS g_pOpenStream = NULL;
tDC g_pDelCache = NULL;
int g_currentMonNum = 1;

static inline void CleanExit(int code = 0) {
    if (g_hDwm) FreeLibrary(g_hDwm);
    if (g_hUrlMon) FreeLibrary(g_hUrlMon);
    if (g_hWinInet) FreeLibrary(g_hWinInet);
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    ExitProcess(code);
}

void RefreshTheme(HWND h) {
    DWORD lightModeVal = 1;
    
    if (bThemeMode == 0) {
        DWORD valSize = sizeof(lightModeVal);
        HKEY hKey;
    
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&lightModeVal, &valSize);
            RegCloseKey(hKey);
        }
    } else if (bThemeMode == 1) {
        lightModeVal = 1;
    } else if (bThemeMode == 2) {
        lightModeVal = 0;
    }
    
    b_IsDarktheme = (lightModeVal == 0);

    if (g_hBrBkgnd) DeleteObject(g_hBrBkgnd);
    g_hBrBkgnd = CreateSolidBrush(b_IsDarktheme ? RGB(32, 32, 32) : RGB(240, 240, 240));

    if (h) {
        if (g_pfnDwmSetAttr) {
            BOOL dark = b_IsDarktheme;
            g_pfnDwmSetAttr(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        }
        SetClassLongPtr(h, GCLP_HBRBACKGROUND, (LONG_PTR)g_hBrBkgnd);
        InvalidateRect(h, NULL, TRUE);
    }
}

const wchar_t* UPDATE_CHECK_URL = L"https://raw.githubusercontent.com/ArKT-7/QuickRotate/main/version.h";
const wchar_t* CURRENT_VER = VERSION_W;
const wchar_t* UNINSTALL_REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ArKT_QuickRotate";
const wchar_t* APPPATH_REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\QuickRotate.exe";
const wchar_t* SETTINGS_REG_PATH = L"Software\\ArKT-7\\QuickRotate";
void UpdateLayout(HWND h);
void PerformUpdateCheck(HWND h);
void PerformDownload(HWND h);
void UpdateAutoStartRegistry(bool enable);
void GetLinkPath(wchar_t* outPath, const wchar_t* name);

DWORD WINAPI CheckUpdateThread(LPVOID lpParam) {
    CoInitialize(NULL);
    PerformUpdateCheck((HWND)lpParam);
    CoUninitialize();
    return 0;
}

DWORD WINAPI DownloadThread(LPVOID lpParam) {
    CoInitialize(NULL);
    PerformDownload((HWND)lpParam);
    CoUninitialize();
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
        g->Clear(b_IsDarktheme ? Color(32, 32, 32) : Color(240, 240, 240));
    }

    ~AutoMemDC() {
        BitBlt(hDC, x, y, w, h, hMemDC, 0, 0, SRCCOPY);
        delete g;
        SelectObject(hMemDC, hOldBM);
        DeleteObject(hBM);
        DeleteDC(hMemDC);
    }
};

LRESULT CALLBACK StaticProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) {
        HDC dc = (HDC)w;
        RECT rc;
        GetClientRect(h, &rc);
        FillRect(dc, &rc, g_hBrBkgnd);
        return 1;
    }
    return CallWindowProc(oldstaticProc, h, m, w, l);
}

LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) {
        HDC dc = (HDC)w;
        RECT rc;
        GetClientRect(h, &rc);
        FillRect(dc, &rc, g_hBrBkgnd);
        return 1;
    }

    int id = GetDlgCtrlID(h);
    if ((id == ID_CHK_TRAYMODE || id == ID_CHK_THEME) && m == WM_MOUSEMOVE) {
        ForceRedraw(h);
    }
    if (m == WM_MOUSEMOVE) {
        if (hHover != h) {
            hHover = h;
            ForceRedraw(h); 
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
        }
    }
    else if (m == WM_MOUSELEAVE) {
        hHover = NULL;
        ForceRedraw(h);
    }
    return CallWindowProc(oldBtnProc, h, m, w, l);
}

HWND CreateMyButton(HWND parent, LPCWSTR text, int id, int x, int y, int w, int h, DWORD extraStyle = 0) {
    HWND btn = CreateWindowW(L"BUTTON", text, WS_CHILD | BS_OWNERDRAW | extraStyle, 
        S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(btn, GWLP_WNDPROC, (LONG_PTR)BtnProc);
    if (!oldBtnProc) oldBtnProc = prev;
    return btn;
}

bool GetAppDataPath(wchar_t* outPath, const wchar_t* appendFile) {
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, outPath))) return false;
    PathAppendW(outPath, AppClass);
    CreateDirectoryW(outPath, NULL);
    if (appendFile) PathAppendW(outPath, appendFile);
    return true;
}

void InitPaths() {
    GetModuleFileNameW(NULL, g_currentPath, MAX_PATH);
    GetAppDataPath(g_appDir, NULL);
    GetAppDataPath(g_exePath, L"QuickRotate.exe");
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, g_startMenuPath);
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, g_desktopPath);
}

void RegSetStr(HKEY hKey, LPCWSTR name, LPCWSTR val) {
    RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)val, (lstrlenW(val) + 1) * sizeof(wchar_t));
}

void RegisterUninstaller() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;

    wchar_t uninstallCmd[MAX_PATH + 32];
    wnsprintfW(uninstallCmd, MAX_PATH + 32, L"\"%s\" -nuke", g_exePath);
    
    wchar_t quietUninstallCmd[MAX_PATH + 32];
    wnsprintfW(quietUninstallCmd, MAX_PATH + 32, L"\"%s\" -silentnuke", g_exePath);

    RegSetStr(hKey, L"DisplayName", AppName);
    RegSetStr(hKey, L"DisplayVersion", CURRENT_VER);
    RegSetStr(hKey, L"Publisher", L"ArKT-7");
    RegSetStr(hKey, L"DisplayIcon", g_exePath);
    RegSetStr(hKey, L"InstallLocation", g_appDir);
    RegSetStr(hKey, L"UninstallString", uninstallCmd);
    RegSetStr(hKey, L"QuietUninstallString", quietUninstallCmd);
    RegSetStr(hKey, L"URLInfoAbout", L"https://github.com/ArKT-7/QuickRotate");

    HKEY hAppPathKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, APPPATH_REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hAppPathKey, NULL) == ERROR_SUCCESS) {
        RegSetStr(hAppPathKey, L"", g_exePath);
        RegSetStr(hAppPathKey, L"Path", g_appDir);
        RegCloseKey(hAppPathKey);
    }

    DWORD one = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(g_exePath, GetFileExInfoStandard, &fad)) {
        ULONGLONG bytes = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        DWORD sizeKB = (DWORD)(bytes / 1024);
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&sizeKB, sizeof(sizeKB));
    }
    RegCloseKey(hKey);
}

bool GetInstalledRegVersion(wchar_t* outVer) {
    HKEY hCheck;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, UNINSTALL_REG_PATH, 0, KEY_READ, &hCheck) == ERROR_SUCCESS) {
        DWORD size = 64 * sizeof(wchar_t);
        if (RegQueryValueExW(hCheck, L"DisplayVersion", NULL, NULL, (LPBYTE)outVer, &size) == ERROR_SUCCESS) {
            RegCloseKey(hCheck);
            return true;
        }
        RegCloseKey(hCheck);
    }
    return false;
}

bool EnsureInstalled(bool forceUpdate) {
    if (g_exePath[0] != 0) {
        if (forceUpdate || GetFileAttributesW(g_exePath) == INVALID_FILE_ATTRIBUTES) {
            if (lstrcmpiW(g_currentPath, g_exePath) != 0) {
                CopyFileW(g_currentPath, g_exePath, FALSE);
            }
        }

        HKEY hCheck;
        bool bNeedsReg = true;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, UNINSTALL_REG_PATH, 0, KEY_READ, &hCheck) == ERROR_SUCCESS) {
            wchar_t regVer[64] = {0};
            DWORD size = sizeof(regVer);
            if (RegQueryValueExW(hCheck, L"DisplayVersion", NULL, NULL, (LPBYTE)regVer, &size) == ERROR_SUCCESS) {
                if (lstrcmpW(regVer, CURRENT_VER) == 0) {
                    bNeedsReg = false;
                }
            }
            RegCloseKey(hCheck);
        }

        if (bNeedsReg) {
            RegisterUninstaller();
        }
        
        return true;
    }
    return false;
}

void KillExistingInstance() {
    HWND hExisting = FindWindowW(AppClass, NULL);
    if (hExisting) {
        SendMessageW(hExisting, WM_COMMAND, ID_TRAY_EXIT, 0);
        for(int i = 0; i < 20; i++) {
            if (!FindWindowW(AppClass, NULL)) break;
            Sleep(50);
        }
    }
}

bool RunUninstall(bool silent = false) {
    if (!silent) {
        if (MessageBoxW(NULL, L"Are you sure you want to uninstall Quick Rotate and remove all settings?", AppTitle, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) {
            return false;
        }
    }

    KillExistingInstance();
    UpdateAutoStartRegistry(false);

    for (int i = 0; i < 6; i++) {
        wchar_t path[MAX_PATH];
        GetLinkPath(path, SC_NAMES[i]);
        DeleteFileW(path);
    }

    if (g_startMenuPath[0] != 0) {
        wchar_t szLinkPath[MAX_PATH];
        wnsprintfW(szLinkPath, MAX_PATH, L"%s\\Quick Rotate.lnk", g_startMenuPath);
        DeleteFileW(szLinkPath);
    }

    RegDeleteKeyW(HKEY_CURRENT_USER, APPPATH_REG_PATH);
    RegDeleteKeyW(HKEY_CURRENT_USER, SETTINGS_REG_PATH);
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_REG_PATH);

    if (!silent) {
        MessageBoxW(NULL, L"Quick Rotate was successfully removed from your computer.", AppTitle, MB_OK | MB_ICONINFORMATION);
    }

    wchar_t cmdLine[MAX_PATH * 3 + 128];
    wnsprintfW(cmdLine, MAX_PATH * 3 + 128,
        L"/c cd \\ & ping 127.0.0.1 -n 3 > nul & del /f /q \"%s\" & ping 127.0.0.1 -n 2 > nul & rmdir /s /q \"%s\"",
        g_exePath, g_appDir);

    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = cmdLine;
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei) && sei.hProcess) CloseHandle(sei.hProcess);
    return true;
}

void UpdateAutoStartRegistry(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            EnsureInstalled(true); 
            wchar_t cmd[MAX_PATH + 20];
            wnsprintfW(cmd, MAX_PATH + 20, L"\"%s\" -tray", g_exePath);
            RegSetValueExW(hKey, AppClass, 0, REG_SZ, (LPBYTE)cmd, (lstrlenW(cmd) + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, AppClass);
        }
        RegCloseKey(hKey);
    }
}

void GetLinkPath(wchar_t* outPath, const wchar_t* name) {
    wnsprintfW(outPath, MAX_PATH, L"%s\\%s.lnk", g_desktopPath, name);
}

HRESULT CreateLink(LPCWSTR lpszArgs, LPCWSTR lpszDesc, LPCWSTR lpszSuffix) {
    HRESULT hres;
    IShellLink* psl;
    EnsureInstalled(false);
    wchar_t szLinkPath[MAX_PATH];
    GetLinkPath(szLinkPath, lpszSuffix);

    if (GetFileAttributesW(szLinkPath) != INVALID_FILE_ATTRIBUTES) return HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;
        psl->SetPath(g_exePath);
        psl->SetArguments(lpszArgs);
        psl->SetDescription(lpszDesc);
        psl->SetIconLocation(g_exePath, 0);
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hres)) { hres = ppf->Save(szLinkPath, TRUE); ppf->Release(); }
        psl->Release();
    }
    return hres;
}

void EnsureStartMenuShortcut() {
    if (g_startMenuPath[0] == 0) return;
    wchar_t szLinkPath[MAX_PATH];
    wnsprintfW(szLinkPath, MAX_PATH, L"%s\\Quick Rotate.lnk", g_startMenuPath);

    if (GetFileAttributesW(szLinkPath) != INVALID_FILE_ATTRIBUTES) return;

    HRESULT hres;
    IShellLink* psl;
    EnsureInstalled(false);

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;
        psl->SetPath(g_exePath);
        psl->SetDescription(AppName);
        psl->SetIconLocation(g_exePath, 0);
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hres)) { 
            ppf->Save(szLinkPath, TRUE); 
            ppf->Release(); 
        }
        psl->Release();
    }
}

void ManageShortcut(int index, bool create) {
    LPCWSTR args[]  = {L"next", L"0", L"90", L"180", L"270", L""};
    
    if (create) {
        CreateLink(args[index], SC_NAMES[index], SC_NAMES[index]);
    } else {
        wchar_t path[MAX_PATH];
        GetLinkPath(path, SC_NAMES[index]);
        DeleteFileW(path);
    }
}

void LoadSettings() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD); DWORD val;
        if (RegQueryValueExW(hKey, L"CloseToTray", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) bCloseToTray = (val != 0);
        if (RegQueryValueExW(hKey, L"AutoStart", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) bAutoStart = (val != 0);
        if (RegQueryValueExW(hKey, L"TrayToggleLP", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) bTrayToggleLP = val;
        if (RegQueryValueExW(hKey, L"ThemeMode", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) bThemeMode = val;
        RegCloseKey(hKey);
    }
}

void SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_REG_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = bCloseToTray ? 1 : 0;
        RegSetValueExW(hKey, L"CloseToTray", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = bAutoStart ? 1 : 0;
        RegSetValueExW(hKey, L"AutoStart", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = bTrayToggleLP;
        RegSetValueExW(hKey, L"TrayToggleLP", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = bThemeMode;
        RegSetValueExW(hKey, L"ThemeMode", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
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
    HDWP hdwp = BeginDeferWindowPos(25);

    auto mv = [&hdwp](HWND w, int y, int h_val, int wd = BTN_W, int x = BTN_X) { 
        if (w) {
            hdwp = DeferWindowPos(hdwp, w, NULL, S(x), S(y), S(wd), S(h_val), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS); 
        }
    };

    for (int i = 0; i < 5; i++) mv(hBtnRot[i], 20 + (i * (BTN_H + 15)), BTN_H);
    mv(hBtnSettings, SETTINGS_Y, BTN_SH);
    int half = (BTN_W - 10) / 2;
    mv(hSetControls[0], SETTINGS_Y, BTN_SH, half);
    mv(hSetControls[11], SETTINGS_Y, BTN_SH, half, BTN_X + half + 10);

    mv(hSetControls[1], 10, 30);
    mv(hSetControls[2], 46, 30);
    mv(hSetControls[12], 82, 30);
    mv(hSetControls[8], 118, 30);
    mv(hSetControls[9], 172, 30);

    for (int i = 0; i < 5; i++) mv(hSetControls[3+i], 208 + (i * 36), 30);
    
    if (hSetControls[10]) mv(hSetControls[10], 426, 25);

    if (bUpdatePageMode) {
        mv(hLblStatus, 160, 35);
        mv(hLblCurVer, 200, 30);
        mv(hLblNewVer, 230, 30);
        mv(hProgress, 275, 40);
        mv(hBtnDownload, 320, 60);
    }
    
    EndDeferWindowPos(hdwp);
}

void ToggleUpdateView(HWND h, bool show) {
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);

    bUpdatePageMode = show;
    UpdateLayout(h);

    HDWP hdwp = BeginDeferWindowPos(15);
    UINT flagSC = show ? SWP_HIDEWINDOW : SWP_SHOWWINDOW;
    UINT flagUpd = show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;

    for (int i = 3; i <= 7; i++) {
        if (hSetControls[i]) hdwp = DeferWindowPos(hdwp, hSetControls[i], NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagSC);
    }
    if (hSetControls[9]) hdwp = DeferWindowPos(hdwp, hSetControls[9], NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagSC);

    if (hLblStatus) hdwp = DeferWindowPos(hdwp, hLblStatus, NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagUpd);
    if (hLblCurVer) hdwp = DeferWindowPos(hdwp, hLblCurVer, NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagUpd);
    if (hLblNewVer) hdwp = DeferWindowPos(hdwp, hLblNewVer, NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagUpd);
    if (hProgress)  hdwp = DeferWindowPos(hdwp, hProgress, NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|flagUpd);

    if (!show && hBtnDownload) {
        hdwp = DeferWindowPos(hdwp, hBtnDownload, NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
    }
    EndDeferWindowPos(hdwp);

    if (show) {
        HANDLE hThread = CreateThread(NULL, 0, CheckUpdateThread, (LPVOID)h, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    ForceRedraw(h, true);
}

void ToggleViewMode(HWND h) {
    if (bUpdatePageMode) {
        ToggleUpdateView(h, false);
        return;
    }

    SendMessageW(h, WM_SETREDRAW, FALSE, 0);

    bSettingsMode = !bSettingsMode;

    if (bSettingsMode) {
        for(int i=0; i<6; i++) {
            wchar_t path[MAX_PATH];
            GetLinkPath(path, SC_NAMES[i]);
            bShortcutsState[i] = (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
        }
    }

    HDWP hdwp = BeginDeferWindowPos(20);
    UINT flagMain = bSettingsMode ? SWP_HIDEWINDOW : SWP_SHOWWINDOW;
    UINT flagSet = bSettingsMode ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;

    for (int i=0; i<5; i++) {
        if (hBtnRot[i]) hdwp = DeferWindowPos(hdwp, hBtnRot[i], NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | flagMain);
    }
    if (hBtnSettings) hdwp = DeferWindowPos(hdwp, hBtnSettings, NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | flagMain);

    for (int i=0; i<13; i++) {
        if (hSetControls[i]) {
            hdwp = DeferWindowPos(hdwp, hSetControls[i], NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | flagSet);
        }
    }
    EndDeferWindowPos(hdwp);

    if (hSetControls[10]) SendMessageW(hSetControls[10], WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    if (bSettingsMode) {
        SetFocus(hSetControls[0]); 
    } else {
        SetFocus(hBtnRot[0]); 
    }
    
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    ForceRedraw(h, true);
}

void DrawProIcon(Graphics& g, int id, int x, int y, int s, Color c, bool isFilled) {
    Pen pen(c, S(2));
    SolidBrush brush(c);
    GraphicsPath p;

    if (id >= ID_BTN_LANDSCAPE && id <= ID_BTN_FLIPPORT) {
        int ss = s * 0.67, pad = (s - ss) / 2;
        Rect r = (id % 2 == 0) ? Rect(x, y + pad, s, ss) : Rect(x + pad, y, ss, s);
        int gw = S(1), L = r.X, T = r.Y, R = r.GetRight(), B = r.GetBottom();
        int cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;

        if (id == ID_BTN_LANDSCAPE) {
            p.AddLine(R, cy + gw, R, B); p.AddLine(R, B, L, B); 
            p.AddLine(L, B, L, T); p.AddLine(L, T, R, T); p.AddLine(R, T, R, cy - gw);
        } else if (id == ID_BTN_FLIPPED) {
            p.AddLine(L, cy - gw, L, T); p.AddLine(L, T, R, T); 
            p.AddLine(R, T, R, B); p.AddLine(R, B, L, B); p.AddLine(L, B, L, cy + gw);
        } else if (id == ID_BTN_FLIPPORT) {
            p.AddLine(cx + gw, T, R, T); p.AddLine(R, T, R, B); 
            p.AddLine(R, B, L, B); p.AddLine(L, B, L, T); p.AddLine(L, T, cx - gw, T);
        } else {
            p.AddLine(cx + gw, B, R, B); p.AddLine(R, B, R, T); 
            p.AddLine(R, T, L, T); p.AddLine(L, T, L, B); p.AddLine(L, B, cx - gw, B);
        }
        if (isFilled) g.FillPath(&brush, &p); else g.DrawPath(&pen, &p);
    }
    else if (id == ID_BTN_NEXT) {
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
    if (!g_hUrlMon) {
        g_hUrlMon = LoadLibraryW(L"urlmon.dll");
        if (g_hUrlMon) {
            g_pDownload = (tUD)GetProcAddress(g_hUrlMon, "URLDownloadToFileW");
            g_pOpenStream = (tOS)GetProcAddress(g_hUrlMon, "URLOpenBlockingStreamW");
        }
    }
    if (!g_hWinInet) {
        g_hWinInet = LoadLibraryW(L"wininet.dll");
        if (g_hWinInet) g_pDelCache = (tDC)GetProcAddress(g_hWinInet, "DeleteUrlCacheEntryW");
    }

    if (!g_pDownload) { SetWindowTextW(hLblStatus, L"Error: Missing DLL"); return; }
    if (!IsWindow(hLblStatus)) return;
    ShowWindow(hBtnDownload, SW_HIDE);
    SetCtrlFont(hLblStatus, hFontTitle);
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
        if (!IsWindow(hLblStatus)) return;
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
            SetCtrlFont(hLblCurVer, hFontBold);
            wchar_t cur[64]; wnsprintfW(cur, 64, L"Current: %s", VERSION_W);
            SetWindowTextW(hLblCurVer, cur);
            int verDiff = CompareVersion(ver, VERSION_W);
            bool isNewer = (verDiff > 0) || (verDiff == 0 && remB > locB);

            if (ver[0] == 0 || g_downloadUrl[0] == 0) {
                SetWindowTextW(hLblStatus, L"Update Error");
                SetWindowTextW(hProgress, L"Invalid server data.");
            } 
            else if (isNewer) {
                SetCtrlFont(hLblStatus, hFontTitle);
                SetWindowTextW(hLblStatus, L"Update Available!");
                SetCtrlFont(hLblNewVer, hFontBold);
                wchar_t neu[64]; 
                if (verDiff == 0 && remB > locB) wnsprintfW(neu, 64, L"New: %s (Rev %d)", ver, remB);
                else wnsprintfW(neu, 64, L"New: %s", ver);
                SetWindowTextW(hLblNewVer, neu);
                SetCtrlFont(hProgress, hFontBold);
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
    if (!IsWindow(hLblStatus)) return;

    EnableWindow(hBtnDownload, FALSE);
    EnableWindow(hSetControls[0], FALSE);
    EnableWindow(hSetControls[11], FALSE);
    InvalidateRect(hBtnDownload, NULL, FALSE);
    InvalidateRect(hSetControls[0], NULL, FALSE);
    InvalidateRect(hSetControls[11], NULL, FALSE);
    SetCtrlFont(hLblStatus, hFontTitle);
    SetWindowTextW(hLblStatus, L"Starting Download...");
    SetCtrlFont(hProgress, hFontBold);
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

        if (!IsWindow(hLblStatus)) return;

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
            EnableWindow(hSetControls[0], TRUE);
            EnableWindow(hSetControls[11], TRUE);
        }
    } else {
        SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)hFontHeader, TRUE);
        SetWindowTextW(hLblStatus, L"Download Failed");
        EnableWindow(hBtnDownload, TRUE);
        EnableWindow(hSetControls[0], TRUE);
        EnableWindow(hSetControls[11], TRUE);
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
    case WM_ERASEBKGND: {
        HDC dc = (HDC)w;
        RECT rc;
        GetClientRect(h, &rc);
        FillRect(dc, &rc, g_hBrBkgnd);
        return 1;
    }
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
        int ids[] = { ID_BTN_LANDSCAPE, ID_BTN_PORTRAIT, ID_BTN_NEXT, ID_BTN_FLIPPED, ID_BTN_FLIPPORT };

        for (int i = 0; i < 5; i++) {
            hBtnRot[i] = CreateMyButton(h, txt[i], ids[i], BTN_X, 20 + (i * (BTN_H + 15)), BTN_W, BTN_H, WS_VISIBLE | WS_TABSTOP);
        }

        hBtnSettings = CreateMyButton(h, L"\u2699 Settings", ID_BTN_SETTINGS, BTN_X, SETTINGS_Y, BTN_W, BTN_SH, WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP);

        int halfW = (BTN_W - 10) / 2;
        hSetControls[0] = CreateMyButton(h, L"\u2B05 Back", ID_BTN_BACK, BTN_X, SETTINGS_Y, halfW, BTN_SH, BS_PUSHBUTTON | WS_TABSTOP);
        hSetControls[11] = CreateMyButton(h, L"Check Update", ID_BTN_UPDATE, BTN_X + halfW + 10, SETTINGS_Y, halfW, BTN_SH, WS_TABSTOP);
        struct TogData { int idx; LPCWSTR txt; int id; int y; };
        LPCWSTR trayText;
        if (bTrayToggleLP == 1) trayText = L"Tray Click: Landscape \u2194 Portrait";
        else if (bTrayToggleLP == 2) trayText = L"Tray Click: (F) Landscape \u2194 Portrait";
        else trayText = L"Tray Click: Cycle Rotation (Next \u27F3)";
        
        const wchar_t* themeText;
        if (bThemeMode == 0) themeText = L"Theme: System Default";
        else if (bThemeMode == 1) themeText = L"Theme: Light Mode";
        else themeText = L"Theme: Dark Mode";
        
        TogData togs[] = {
            {1, L"Minimize to Tray on Close", ID_CHK_TRAY, 10},
            {2, L"Start with Windows", ID_CHK_AUTOSTART, 46},
            {12, themeText, ID_CHK_THEME, 82},
            {8, trayText, ID_CHK_TRAYMODE, 118},
            {9, L"Shortcut: Quick Rotate App", ID_SC_APP, 172}
        };

        for (int i = 0; i < 5; i++) {
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
            hSetControls[3+i] = CreateMyButton(h, scTxt[i], scIds[i], BTN_X, 208 + (i * 36), BTN_W, 30, WS_TABSTOP);
        }

        hSetControls[10] = NULL;
        
        hLblStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        oldstaticProc = (WNDPROC)SetWindowLongPtr(hLblStatus, GWLP_WNDPROC, (LONG_PTR)StaticProc);
        SetCtrlFont(hLblStatus, hFontTitle);

        hLblCurVer = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SetWindowLongPtr(hLblCurVer, GWLP_WNDPROC, (LONG_PTR)StaticProc);
        SetCtrlFont(hLblCurVer, hFontHeader);

        hLblNewVer = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SetWindowLongPtr(hLblNewVer, GWLP_WNDPROC, (LONG_PTR)StaticProc);
        SetCtrlFont(hLblNewVer, hFontHeader);

        hProgress = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 0, 0, h, NULL, NULL, NULL);
        SetWindowLongPtr(hProgress, GWLP_WNDPROC, (LONG_PTR)StaticProc);
        SetCtrlFont(hProgress, hFontHeader);

        hBtnDownload = CreateMyButton(h, L"Download && Install", ID_BTN_DOWNLOAD, 0, 0, 0, 0, BS_PUSHBUTTON);

        nid.cbSize = sizeof(NOTIFYICONDATAW); nid.hWnd = h; nid.uID = 1001; nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIconSm;
        
        lstrcpynW(nid.szTip, AppTitle, 128);
        return 0;
    }

    case WM_DPICHANGED: {
        g_dpi = HIWORD(w);
        RECT* const prcNewWindow = (RECT*)l;
        SetWindowPos(h, NULL, prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        RecreateFonts();
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
            ForceRedraw(h);
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
        ForceRedraw(h);
        return 0;
    }

    case WM_SETTINGCHANGE: {
        if (l != 0 && lstrcmpiW((LPCWSTR)l, L"ImmersiveColorSet") == 0) {
            if (bThemeMode == 0) {
                RefreshTheme(h);
                ForceRedraw(h, true);
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)w;
        SetBkColor(hdcStatic, b_IsDarktheme ? RGB(32, 32, 32) : RGB(240, 240, 240)); 
        SetTextColor(hdcStatic, b_IsDarktheme ? RGB(220, 220, 220) : RGB(0, 0, 0));
        SetBkMode(hdcStatic, TRANSPARENT); 
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

            if (btnId == ID_CHK_TRAYMODE || btnId == ID_CHK_THEME) {
                g->Clear(b_IsDarktheme ? Color(32, 32, 32) : Color(240, 240, 240)); 
                wchar_t buf_t[64]; GetWindowTextW(p->hwndItem, buf_t, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); 
                SetTextColor(buf.hMemDC, b_IsDarktheme ? RGB(220, 220, 220) : RGB(0, 0, 0)); 
                SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(5), 0, w - S(50), h}; DrawTextW(buf.hMemDC, buf_t, -1, &tr, 36); 
                int bW = S(44), bH = S(24); Rect rB(w - bW - S(2), (h - bH) / 2, bW, bH);
                bool hv = (hHover == p->hwndItem);
                Color cB;
                if (pressed) cB = b_IsDarktheme ? Color(0, 80, 150) : Color(0, 60, 120);
                else if (hv) cB = b_IsDarktheme ? Color(0, 120, 215) : Color(0, 140, 235);
                else cB = b_IsDarktheme ? Color(0, 100, 180) : Color(0, 120, 215);
                GraphicsPath ph; GetRoundedRectPath(&ph, rB, S(24));
                SolidBrush br(cB); g->FillPath(&br, &ph);
                Pen pn(b_IsDarktheme ? Color(220, 220, 220) : Color(255, 255, 255), S(2));
                pn.SetStartCap(LineCapRound); pn.SetEndCap(LineCapRound);
                int cx = rB.X + bW/2, cy = rB.Y + bH/2, f = S(3);
                g->DrawLine(&pn, cx-f, cy-f, cx+f-S(1), cy); g->DrawLine(&pn, cx+f-S(1), cy, cx-f, cy+f);

                if (focused && g_bShowFocus) {
                    Pen fp(Color(150, 150, 150), 1); fp.SetDashStyle(DashStyleDot);
                    g->DrawRectangle(&fp, rB);
                }
            } else if (btnId >= ID_BTN_BACK && btnId != ID_BTN_BACK && btnId != ID_TRAY_RESTORE && btnId != ID_TRAY_EXIT && btnId != ID_BTN_DOWNLOAD) {
                bool isChecked = false;
                if (btnId == ID_CHK_TRAY) isChecked = bCloseToTray;
                else if (btnId == ID_CHK_AUTOSTART) isChecked = bAutoStart;
                else if (btnId >= ID_SC_NEXT) isChecked = bShortcutsState[btnId - ID_SC_NEXT];

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); 
                SetTextColor(buf.hMemDC, b_IsDarktheme ? RGB(220, 220, 220) : RGB(0, 0, 0)); 
                SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(5), 0, w - S(55), h}; DrawTextW(buf.hMemDC, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                int tH = S(22), tW = S(44), tX = w - tW - S(2), tY = (h - tH) / 2;
                GraphicsPath path; path.AddArc(tX, tY, tH, tH, 90, 180); path.AddArc(tX + tW - tH, tY, tH, tH, 270, 180); path.CloseFigure();
                Color tc;
                if (isChecked) {
                    tc = hovered ? (b_IsDarktheme ? Color(0, 120, 215) : Color(0, 140, 235)) : (b_IsDarktheme ? Color(0, 100, 180) : Color(0, 120, 215));
                } else {
                    tc = hovered ? (b_IsDarktheme ? Color(70, 70, 70) : Color(195, 195, 195)) : (b_IsDarktheme ? Color(50, 50, 50) : Color(180, 180, 180));
                }
                SolidBrush tb(tc); g->FillPath(&tb, &path);
                SolidBrush wb(b_IsDarktheme ? Color(220, 220, 220) : Color(255, 255, 255));
                g->FillEllipse(&wb, isChecked ? (tX + tW - S(16) - S(3)) : (tX + S(3)), tY + (tH - S(16)) / 2, S(16), S(16));

                if (focused && g_bShowFocus) {
                    Pen focusPen(Color(100, 100, 100), 1);
                    focusPen.SetDashStyle(DashStyleDot);
                    g->DrawRectangle(&focusPen, S(2), S(2), w - S(4), h - S(4));
                }

            } else {
                Color bg, txt;
                bool active = (btnId - ID_BTN_LANDSCAPE == currentScreenRot) && (btnId < ID_BTN_BACK);
                bool disabled = (p->itemState & ODS_DISABLED);

                if (btnId == ID_BTN_SETTINGS || btnId == ID_BTN_BACK || btnId == ID_BTN_UPDATE || btnId == ID_BTN_DOWNLOAD) {
                    if (disabled) { bg = b_IsDarktheme ? Color(40, 40, 40) : Color(200, 200, 200); txt = b_IsDarktheme ? Color(100, 100, 100) : Color(160, 160, 160); }
                    else if (pressed) { bg = b_IsDarktheme ? Color(35, 35, 35) : Color(60, 60, 60); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = b_IsDarktheme ? Color(60, 60, 60) : Color(120, 120, 120); txt = Color(255, 255, 255); }
                    else { bg = b_IsDarktheme ? Color(45, 45, 45) : Color(80, 80, 80); txt = b_IsDarktheme ? Color(230, 230, 230) : Color(255, 255, 255); }
                } else {
                    if (active) {
                        if (pressed) { bg = b_IsDarktheme ? Color(35, 35, 35) : Color(200, 220, 255); txt = b_IsDarktheme ? Color(60, 150, 235) : Color(0, 100, 200); }
                        else if (hovered) { bg = b_IsDarktheme ? Color(60, 60, 60) : Color(220, 235, 255); txt = b_IsDarktheme ? Color(100, 190, 255) : Color(0, 120, 215); }
                        else { bg = b_IsDarktheme ? Color(45, 45, 45) : Color(255, 255, 255); txt = b_IsDarktheme ? Color(80, 170, 255) : Color(0, 120, 215); }
                    } else {
                        if (pressed) { bg = b_IsDarktheme ? Color(0, 80, 150) : Color(0, 80, 160); txt = b_IsDarktheme ? Color(200, 200, 200) : Color(255, 255, 255); }
                        else if (hovered) { bg = Color(135, 206, 250); txt = Color(0, 60, 140); }
                        else { bg = b_IsDarktheme ? Color(0, 100, 180) : Color(0, 120, 215); txt = b_IsDarktheme ? Color(230, 230, 230) : Color(255, 255, 255); }
                    }
                }
                if (btnId == ID_BTN_DOWNLOAD && !disabled) {
                    if (pressed) { bg = b_IsDarktheme ? Color(0, 80, 150) : Color(0, 80, 160); txt = Color(255, 255, 255); }
                    else if (hovered) { bg = Color(135, 206, 250); txt = Color(0, 60, 140); }
                    else { bg = b_IsDarktheme ? Color(0, 100, 180) : Color(0, 120, 215); txt = b_IsDarktheme ? Color(230, 230, 230) : Color(255, 255, 255); }
                }

                GraphicsPath path; Rect r(S(2), S(2), w - S(4), h - S(4));
                GetRoundedRectPath(&path, r, S(CORNER_RADIUS) * 2);
                SolidBrush b(bg); g->FillPath(&b, &path);

                if (btnId >= ID_BTN_LANDSCAPE && btnId <= ID_BTN_NEXT) {
                    DrawProIcon(*g, btnId, S(15), (h - S(20)) / 2, S(20), txt, active);
                }
                
                if (active) {
                    Pen p(txt, S(3));
                    p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound);
                    int tx = w - S(40), ty = h / 2;
                    g->DrawLine(&p, tx, ty, tx + S(5), ty + S(5)); g->DrawLine(&p, tx + S(5), ty + S(5), tx + S(14), ty - S(6));
                }

                wchar_t text[64]; GetWindowTextW(p->hwndItem, text, 64);
                SetBkMode(buf.hMemDC, TRANSPARENT); SetTextColor(buf.hMemDC, txt.ToCOLORREF());
                if (btnId == ID_BTN_DOWNLOAD) SelectObject(buf.hMemDC, hFontTitle);
                else SelectObject(buf.hMemDC, hFontBold);
                RECT tr = {0, 0, w, h};
                if (btnId >= ID_BTN_BACK || btnId == ID_BTN_SETTINGS || btnId == ID_BTN_DOWNLOAD || btnId == ID_BTN_UPDATE) {
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
            g->Clear(b_IsDarktheme ? Color(36, 36, 36) : Color(255, 255, 255));
            if (p->itemID == 0) {
                Pen pen(b_IsDarktheme ? Color(70, 70, 70) : Color(220, 220, 220), 1); 
                g->DrawLine(&pen, S(10), h / 2, w - S(10), h / 2);
            } else {
                if (p->itemState & ODS_SELECTED) {
                    Rect r(S(4), S(2), w - S(8), h - S(4));
                    GraphicsPath path; GetRoundedRectPath(&path, r, S(8));
                    SolidBrush b(b_IsDarktheme ? Color(55, 55, 55) : Color(230, 243, 255)); g->FillPath(&b, &path);
                }
                bool chk = (p->itemState & ODS_CHECKED);
                Color iconCol = chk ? (b_IsDarktheme ? Color(80, 170, 255) : Color(0, 120, 215)) : (b_IsDarktheme ? Color(120, 120, 120) : Color(150, 150, 150));
                DrawProIcon(*g, p->itemID, S(12), (h - S(16)) / 2, S(16), iconCol, chk);

                SetBkMode(buf.hMemDC, TRANSPARENT); 
                SetTextColor(buf.hMemDC, b_IsDarktheme ? RGB(220, 220, 220) : RGB(0, 0, 0)); 
                SelectObject(buf.hMemDC, hFontHeader);
                RECT tr = {S(40), 0, w, h}; DrawTextW(buf.hMemDC, (LPCWSTR)p->itemData, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id >= ID_BTN_LANDSCAPE && id <= ID_BTN_FLIPPORT) { 
            SetRot((id - ID_BTN_LANDSCAPE) * 90);
            ForceRedraw(h, true); }
        else if (id == ID_BTN_NEXT) { SetRot(-1); ForceRedraw(h, true); }
        else if (id == ID_BTN_SETTINGS || id == ID_BTN_BACK) {
            if (id == ID_BTN_BACK && bUpdatePageMode) ToggleUpdateView(h, false);
            else ToggleViewMode(h);
        }
        else if (id == ID_BTN_UPDATE) { ToggleUpdateView(h, true); }
        else if (id == ID_BTN_DOWNLOAD) { 
            HANDLE hThread = CreateThread(NULL, 0, DownloadThread, (LPVOID)h, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }
        else if (id == ID_CHK_TRAY) {
            bCloseToTray = !bCloseToTray;
            SaveSettings(); 
            ForceRedraw((HWND)l);
        }
        else if (id == ID_CHK_AUTOSTART) {
            bAutoStart = !bAutoStart;
            UpdateAutoStartRegistry(bAutoStart);
            SaveSettings();
            ForceRedraw((HWND)l);
        }
        else if (id == ID_CHK_TRAYMODE) {
            if (bTrayToggleLP == 1) bTrayToggleLP = 2;
            else if (bTrayToggleLP == 2) bTrayToggleLP = 0;
            else bTrayToggleLP = 1;
            const wchar_t* modeText;
            if (bTrayToggleLP == 1) modeText = L"Tray Click: Landscape \u2194 Portrait";
            else if (bTrayToggleLP == 2) modeText = L"Tray Click: (F) Landscape \u2194 Portrait";
            else modeText = L"Tray Click: Cycle Rotation (Next \u27F3)";
            SetWindowTextW(hSetControls[8], modeText);
            SaveSettings();
            ForceRedraw((HWND)l);
        }
        else if (id == ID_CHK_THEME) {
            bThemeMode = (bThemeMode + 1) % 3;
            const wchar_t* modeText;
            if (bThemeMode == 0) modeText = L"Theme: System Default";
            else if (bThemeMode == 1) modeText = L"Theme: Light Mode";
            else modeText = L"Theme: Dark Mode";
            SetWindowTextW(hSetControls[12], modeText);
            SaveSettings();
            RefreshTheme(h);
            ForceRedraw(h, true);
        }
        else if (id >= ID_SC_NEXT && id <= ID_SC_APP) {
            int idx = id - ID_SC_NEXT;
            bShortcutsState[idx] = !bShortcutsState[idx];
            ManageShortcut(idx, bShortcutsState[idx]);
            ForceRedraw((HWND)l);
        }

        else if (id == ID_TRAY_RESTORE) { 
            POINT pt; GetCursorPos(&pt);
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MoveToMonitorCenter(h, hMon);
            ShowWindow(h, SW_SHOW);
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
            if (bTrayToggleLP == 1) {
                int target = (currentScreenRot % 2 == 0) ? 90 : 0;
                SetRot(target);
            } 
            else if (bTrayToggleLP == 2) {
                int target = (currentScreenRot % 2 == 0) ? 270 : 180;
                SetRot(target);
            }
            else {
                SetRot(-1); 
            }
            ForceRedraw(h, true); 
        }
        else if (l == WM_RBUTTONUP) {
            POINT p; GetCursorPos(&p); HMENU hMenu = CreatePopupMenu();
            
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_BTN_NEXT, (LPCWSTR)L"Rotate Clockwise (Next)");
            AppendMenuW(hMenu, MF_SEPARATOR | MF_OWNERDRAW, 0, NULL);
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_BTN_LANDSCAPE, (LPCWSTR)L"Landscape");
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_BTN_PORTRAIT, (LPCWSTR)L"Portrait");
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_BTN_FLIPPED, (LPCWSTR)L"Flipped Landscape");
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_BTN_FLIPPORT, (LPCWSTR)L"Flipped Portrait");
            AppendMenuW(hMenu, MF_SEPARATOR | MF_OWNERDRAW, 0, NULL);
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_TRAY_RESTORE, (LPCWSTR)L"Restore Window");
            AppendMenuW(hMenu, MF_OWNERDRAW, ID_TRAY_EXIT, (LPCWSTR)L"Exit");
            
            if (currentScreenRot >= 0 && currentScreenRot <= 3) CheckMenuItem(hMenu, ID_BTN_LANDSCAPE + currentScreenRot, MF_CHECKED);

            SetForegroundWindow(h); TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, h, NULL); DestroyMenu(hMenu);
        }
        else if (l == WM_LBUTTONDBLCLK) { 
            POINT pt; GetCursorPos(&pt);
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            MoveToMonitorCenter(h, hMon);
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        }
        return 0;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DestroyIcon(hIconSm);
        DestroyIcon(hIconBig);
        DeleteObject(hFontBold); DeleteObject(hFontNormal); DeleteObject(hFontHeader); DeleteObject(hFontTitle);
        DeleteObject(g_hBrBkgnd);
        PostQuitMessage(0);
        return 0;
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        FillRect(dc, &ps.rcPaint, g_hBrBkgnd); 
        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ hOldFont = SelectObject(dc, hFontHeader);
        SetTextColor(dc, b_IsDarktheme ? RGB(110, 110, 110) : RGB(160, 160, 160)); 

        if (bSettingsMode) {
            HPEN hPen = CreatePen(PS_SOLID, S(2), b_IsDarktheme ? RGB(70, 70, 70) : RGB(200, 200, 200)); 
            HGDIOBJ hOldPen = SelectObject(dc, hPen);
            MoveToEx(dc, S(20), S(160), NULL);
            LineTo(dc, S(WIN_W) - S(32), S(160));
            SelectObject(dc, hOldPen);
            DeleteObject(hPen);
            wchar_t verText[64];
            wnsprintfW(verText, 64, L"Quick Rotate %s by ArKT", CURRENT_VER);
            RECT tr = {S(BTN_X), S(STATUS_Y), S(BTN_X) + S(BTN_W), S(WIN_H)};
            DrawTextW(dc, verText, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);
        } 
        else if (!bUpdatePageMode) {
            wchar_t statusText[64];
            wnsprintfW(statusText, 64, L"Active Monitor: %d", g_currentMonNum);
            RECT tr = {0, S(STATUS_Y), S(WIN_W), S(WIN_H)}; 
            DrawTextW(dc, statusText, -1, &tr, DT_CENTER | DT_TOP | DT_SINGLELINE);
        }
        SelectObject(dc, hOldFont);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

extern "C" int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR c, int s) {
    CoInitialize(NULL);

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    g_hDwm = LoadLibraryW(L"dwmapi.dll");
    if (g_hDwm) g_pfnDwmSetAttr = (tDWM)GetProcAddress(g_hDwm, "DwmSetWindowAttribute");

    RefreshTheme(NULL);
    InitPaths();

    wchar_t oldIniPath[MAX_PATH];
    wnsprintfW(oldIniPath, MAX_PATH, L"%s\\ArKT_QuickRotate.ini", g_appDir);
    
    if (GetFileAttributesW(oldIniPath) != INVALID_FILE_ATTRIBUTES) {
        bCloseToTray = GetPrivateProfileIntW(L"Settings", L"CloseToTray", 1, oldIniPath);
        bAutoStart = GetPrivateProfileIntW(L"Settings", L"AutoStart", 0, oldIniPath);
        bTrayToggleLP = GetPrivateProfileIntW(L"Settings", L"TrayToggleLP", 1, oldIniPath);
        bThemeMode = GetPrivateProfileIntW(L"Settings", L"ThemeMode", 0, oldIniPath);
        
        SaveSettings();
        DeleteFileW(oldIniPath);
    } else {
        LoadSettings();
        SaveSettings(); 
    }

    wchar_t oldPath[MAX_PATH];
    lstrcpyW(oldPath, g_currentPath);
    PathRemoveFileSpecW(oldPath);
    PathAppendW(oldPath, L"QuickRotate.old");
    DeleteFileW(oldPath);
    
    if (bAutoStart) UpdateAutoStartRegistry(true);

    LPWSTR cmd = GetCommandLineW();
    bool q = false;
    while (*cmd) { if (*cmd == L'"') q = !q; else if (*cmd == L' ' && !q) { cmd++; break; } cmd++; }
    while (*cmd == L' ') cmd++;

    bool bSilentStart = false;
    if (*cmd != 0) {
        if (lstrcmpiW(cmd, L"-tray") == 0) {
            bSilentStart = true;
        } 
        else if (lstrcmpiW(cmd, L"-nuke") == 0) {
            if (RunUninstall(false)) CleanExit(0);
            else CleanExit(1602);
        }
        else if (lstrcmpiW(cmd, L"-silentnuke") == 0) {
            if (RunUninstall(true)) CleanExit(0);
            else CleanExit(1602);
        }
        else if (lstrcmpiW(cmd, L"-spawn") == 0) {
            wchar_t installedVer[32] = {0};
            if (GetInstalledRegVersion(installedVer) && CompareVersion(VERSION_W, installedVer) < 0) {
                CleanExit();
            }

            HANDLE hMutexInst = CreateMutexW(NULL, TRUE, L"ArKT_QuickRotate_Mutex");
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                KillExistingInstance();
            }

            EnsureInstalled(true);
            EnsureStartMenuShortcut();

            if (hMutexInst) {
                ReleaseMutex(hMutexInst);
                CloseHandle(hMutexInst); 
            }
            CleanExit();
        }
        else {
            if (lstrcmpiW(cmd, L"next") == 0 || lstrcmpiW(cmd, L"rotate") == 0) {
                SetRot(-1);
                CleanExit();
            }

            wchar_t* endPtr;
            int a = (int)wcstol(cmd, &endPtr, 10);
            bool isValidNum = (cmd != endPtr);

            if (isValidNum && (a == 0 || a == 90 || a == 180 || a == 270)) {
                SetRot(a);
                CleanExit();
            } 
            
            wchar_t* n = g_currentPath;
            for (wchar_t* t = g_currentPath; *t; t++) if (*t == L'\\' || *t == L'/') n = t + 1;
            
            wchar_t msg[1024];
            wnsprintfW(msg, 1024,
                L"Usage:\n  .\\%s [angle] OR [next]\n\n"
                L"Examples:\n"
                L"  .\\%s next \t(Rotate Clockwise)\n"
                L"  .\\%s 0 \t(Landscape)\n"
                L"  .\\%s 90 \t(Portrait)\n"
                L"  .\\%s 180 \t(Flipped Landscape)\n"
                L"  .\\%s 270 \t(Flipped Portrait)", 
                n, n, n, n, n, n);

            wchar_t title[256];
            wnsprintfW(title, 256, L" Error or Info? : %s", AppTitle);
            MessageBoxW(NULL, msg, title, MB_OK | MB_ICONINFORMATION);
            CleanExit();
        }
    }

    bool bTempRun = false;

    if (lstrcmpiW(g_currentPath, g_exePath) != 0) {
        if (GetFileAttributesW(g_exePath) != INVALID_FILE_ATTRIBUTES) {
            wchar_t installedVer[32] = {0};
            int verDiff = 1;
            if (GetInstalledRegVersion(installedVer)) {
                verDiff = CompareVersion(VERSION_W, installedVer);
            }

            if (verDiff < 0) {
                wchar_t msg[512];
                wnsprintfW(msg, 512, 
                    L"You are launching Quick Rotate v%s, but a newer version (v%s) is already installed on your PC.\n\n"
                    L"How would you like to proceed?\n\n"
                    L"[Cancel] or [X] \t-  To launch the newer version\n"
                    L"[OK] \t\t-  Run this older version temporarily", 
                    VERSION_W, installedVer);
                
                if (MessageBoxW(NULL, msg, AppTitle, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDCANCEL) {
                    KillExistingInstance();
                    ShellExecuteW(NULL, L"open", g_exePath, NULL, NULL, SW_SHOW);
                    CleanExit(0);
                } else {
                    bUpdateMode = false;
                    bTempRun = true;
                }
            } else {
                bUpdateMode = true;
            }
        } else {
            bUpdateMode = true;
        }
    } else {
        EnsureInstalled(false);
        EnsureStartMenuShortcut();
    }

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"ArKT_QuickRotate_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (bUpdateMode || bTempRun) {
            KillExistingInstance();
        } else {
            HWND hExisting = FindWindowW(AppClass, NULL);
            if (hExisting && !bSilentStart) {
                PostMessageW(hExisting, WM_COMMAND, ID_TRAY_RESTORE, 0);
                SetForegroundWindow(hExisting);
            }
            CloseHandle(hMutex);
            CleanExit();
        }
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

    hMainWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_COMPOSITED, AppClass, AppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (sw - S(WIN_W)) / 2, (sh - S(WIN_H)) / 2, S(WIN_W), S(WIN_H), NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hMainWnd) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex); 
        CleanExit(1);
    }
    RefreshTheme(hMainWnd);

    SendMessageW(hMainWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessageW(hMainWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    if (!bSilentStart) {
        POINT pt; GetCursorPos(&pt);
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MoveToMonitorCenter(hMainWnd, hMon);
        ShowWindow(hMainWnd, SW_SHOW);
        UpdateWindow(hMainWnd);
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
        CopyFileW(g_currentPath, g_exePath, FALSE); 
        
        EnsureInstalled(false);
        EnsureStartMenuShortcut();

        if (bCloseToTray) {
            ShellExecuteW(NULL, L"open", g_exePath, L"-tray", NULL, SW_SHOW);
        }
    }

    ReleaseMutex(hMutex); 
    CloseHandle(hMutex);
    CleanExit();
    return 0;
}