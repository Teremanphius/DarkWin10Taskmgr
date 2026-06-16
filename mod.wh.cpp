// ==WindhawkMod==
// @id           taskmgr-dark-mode-native
// @name         Task Manager Native Dark Mode Fix
// @description  Forces Task Manager into a native dark theme with perfect white chevrons, white tab text, dark title bars, dark top menu bars, and pitch black performance graphs.
// @version      1.9.13
// @author       Me
// @include      taskmgr.exe
// @compilerOptions -luxtheme -lgdi32 -luser32 -lcomctl32 -ldwmapi -lmsimg32
// ==/WindhawkMod==

#include <windows.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdlib.h>

typedef void (WINAPI *pfnSetPreferredAppMode)(int mode);
typedef BOOL (WINAPI *pfnAllowDarkModeForWindow)(HWND hwnd, BOOL bAllow);
typedef void (WINAPI *pfnFlushMenuThemes)();

enum AppMode { Default, AllowDark, ForceDark, ForceLight, Max };

pfnSetPreferredAppMode SetPreferredAppMode = NULL;
pfnAllowDarkModeForWindow AllowDarkModeForWindow = NULL;
pfnFlushMenuThemes FlushMenuThemes = NULL;

struct ThemeTracking {
    HTHEME hTheme;
    WCHAR szClass[64];
};
// Expanded to 1024 to prevent the bottleneck that left menus white
ThemeTracking g_ThemeLog[1024] = {0};
int g_ThemeLogCount = 0;

void RegisterTheme(HTHEME hTheme, LPCWSTR szClass) {
    if (!hTheme || !szClass || g_ThemeLogCount >= 1024) return;
    for (int i = 0; i < g_ThemeLogCount; i++) {
        if (g_ThemeLog[i].hTheme == hTheme) return;
    }
    g_ThemeLog[g_ThemeLogCount].hTheme = hTheme;
    wcsncpy(g_ThemeLog[g_ThemeLogCount].szClass, szClass, 63);
    g_ThemeLogCount++;
}

LPCWSTR GetThemeClassName(HTHEME hTheme) {
    for (int i = 0; i < g_ThemeLogCount; i++) {
        if (g_ThemeLog[i].hTheme == hTheme) return g_ThemeLog[i].szClass;
    }
    return L"";
}

struct DCMapping {
    HDC hMemoryDC;
    HWND hWnd;
};
DCMapping g_DCMaps[128] = {0};
int g_DCMapCount = 0;

void RegisterDC(HDC hMemoryDC, HWND hWnd) {
    if (!hMemoryDC || g_DCMapCount >= 128) return;
    for (int i = 0; i < g_DCMapCount; i++) {
        if (g_DCMaps[i].hMemoryDC == hMemoryDC) {
            g_DCMaps[i].hWnd = hWnd;
            return;
        }
    }
    g_DCMaps[g_DCMapCount].hMemoryDC = hMemoryDC;
    g_DCMaps[g_DCMapCount].hWnd = hWnd;
    g_DCMapCount++;
}

void UnregisterDC(HDC hMemoryDC) {
    for (int i = 0; i < g_DCMapCount; i++) {
        if (g_DCMaps[i].hMemoryDC == hMemoryDC) {
            g_DCMaps[i] = g_DCMaps[g_DCMapCount - 1];
            g_DCMapCount--;
            return;
        }
    }
}

HWND GetWindowFromDCExtended(HDC hdc) {
    if (!hdc) return NULL;
    HWND hwnd = WindowFromDC(hdc);
    if (hwnd) return hwnd;
    for (int i = 0; i < g_DCMapCount; i++) {
        if (g_DCMaps[i].hMemoryDC == hdc) {
            return g_DCMaps[i].hWnd;
        }
    }
    return NULL;
}

using GetSysColor_t = COLORREF (WINAPI *)(int nIndex);
GetSysColor_t real_GetSysColor;

using GetSysColorBrush_t = HBRUSH (WINAPI *)(int nIndex);
GetSysColorBrush_t real_GetSysColorBrush;

using SetTextColor_t = COLORREF (WINAPI *)(HDC hdc, COLORREF color);
SetTextColor_t real_SetTextColor;

using SetBkColor_t = COLORREF (WINAPI *)(HDC hdc, COLORREF color);
SetBkColor_t real_SetBkColor;

using CreateSolidBrush_t = HBRUSH (WINAPI *)(COLORREF color);
CreateSolidBrush_t real_CreateSolidBrush;

using GetStockObject_t = HGDIOBJ (WINAPI *)(int fnObject);
GetStockObject_t real_GetStockObject;

using PatBlt_t = BOOL (WINAPI *)(HDC hdc, int x, int y, int w, int h, DWORD dwRop);
PatBlt_t real_PatBlt;

using BitBlt_t = BOOL (WINAPI *)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
BitBlt_t real_BitBlt;

using StretchBlt_t = BOOL (WINAPI *)(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);
StretchBlt_t real_StretchBlt;

using OpenThemeData_t = HTHEME (WINAPI *)(HWND hwnd, LPCWSTR pszClassList);
OpenThemeData_t real_OpenThemeData;

using DrawThemeBackground_t = HRESULT (WINAPI *)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT *pRect, const RECT *pClipRect);
DrawThemeBackground_t real_DrawThemeBackground;

using CreateWindowExW_t = HWND (WINAPI *)(DWORD dwExStyle, LPCWSTR lpszClassName, LPCWSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
CreateWindowExW_t real_CreateWindowExW;

using SetDCBrushColor_t = COLORREF (WINAPI *)(HDC hdc, COLORREF color);
SetDCBrushColor_t real_SetDCBrushColor;

using SetDCPenColor_t = COLORREF (WINAPI *)(HDC hdc, COLORREF color);
SetDCPenColor_t real_SetDCPenColor;

using CreatePen_t = HPEN (WINAPI *)(int iStyle, int cWidth, COLORREF color);
CreatePen_t real_CreatePen;

using CreatePenIndirect_t = HPEN (WINAPI *)(const LOGPEN *plp);
CreatePenIndirect_t real_CreatePenIndirect;

using DwmSetWindowAttribute_t = HRESULT (WINAPI *)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
DwmSetWindowAttribute_t real_DwmSetWindowAttribute;

using SetWindowTheme_t = HRESULT (WINAPI *)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
SetWindowTheme_t real_SetWindowTheme;

using GradientFill_t = BOOL (WINAPI *)(HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode);
GradientFill_t real_GradientFill;

using FillRect_t = int (WINAPI *)(HDC hdc, const RECT *lprc, HBRUSH hbr);
FillRect_t real_FillRect;

using ExtTextOutW_t = BOOL (WINAPI *)(HDC hdc, int x, int y, UINT fuOptions, const RECT *lprect, LPCWSTR lpString, UINT cchString, CONST INT *lpDx);
ExtTextOutW_t real_ExtTextOutW;

using FillRgn_t = int (WINAPI *)(HDC hdc, HRGN hrgn, HBRUSH hbr);
FillRgn_t real_FillRgn;

using FrameRgn_t = int (WINAPI *)(HDC hdc, HRGN hrgn, HBRUSH hbr, int w, int h);
FrameRgn_t real_FrameRgn;

using CreateCompatibleDC_t = HDC (WINAPI *)(HDC hdc);
CreateCompatibleDC_t real_CreateCompatibleDC;

using DeleteDC_t = BOOL (WINAPI *)(HDC hdc);
DeleteDC_t real_DeleteDC;

typedef BOOL (WINAPI* pfnExtTextOutW)(HDC hdc, int x, int y, UINT options, const RECT *lprc, LPCWSTR lpString, UINT c, const int *lpDx);
typedef int (WINAPI* pfnDrawTextW)(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format);
typedef int (WINAPI* pfnDrawTextExW)(HDC hdc, LPWSTR lpchText, int cchText, LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp);

pfnDrawTextW real_DrawTextW = DrawTextW;
pfnDrawTextExW real_DrawTextExW = DrawTextExW;

// Track which windows have been subclassed to prevent double-subclassing
struct SubclassInfo {
    HWND hwnd;
    bool isSubclassed;
};
SubclassInfo g_SubclassList[256] = {0};
int g_SubclassCount = 0;

bool IsWindowSubclassed(HWND hwnd) {
    for (int i = 0; i < g_SubclassCount; i++) {
        if (g_SubclassList[i].hwnd == hwnd) {
            return g_SubclassList[i].isSubclassed;
        }
    }
    return false;
}

void MarkWindowSubclassed(HWND hwnd) {
    // Check if already tracked
    for (int i = 0; i < g_SubclassCount; i++) {
        if (g_SubclassList[i].hwnd == hwnd) {
            g_SubclassList[i].isSubclassed = true;
            return;
        }
    }
    // Add new entry if space available
    if (g_SubclassCount < 256) {
        g_SubclassList[g_SubclassCount].hwnd = hwnd;
        g_SubclassList[g_SubclassCount].isSubclassed = true;
        g_SubclassCount++;
    }
}

COLORREF GetTargetBgColor(HDC hdc) {
    if (hdc) {
        HWND hwnd = GetWindowFromDCExtended(hdc);
        if (hwnd) {
            WCHAR szClass[64] = {0};
            GetClassNameW(hwnd, szClass, 64);
            if (wcscmp(szClass, L"NativeHWNDHost") == 0) {
                return RGB(0, 0, 0); 
            }
        }
    }
    return RGB(15, 15, 15); 
}

COLORREF ConvertTaskMgrColor(COLORREF color, HDC hdc) {
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);

    COLORREF targetBg = GetTargetBgColor(hdc);

    if (r < 45 && g < 45 && b < 45) {
        return color;
    }

    if (r > 230 && g > 230 && b > 230 && abs((int)r - g) < 12 && abs((int)g - b) < 12) {
        return targetBg;
    }

    int deficit = (255 - r) + (255 - g) + (255 - b);

    if (deficit < 65) {
        return targetBg;
    }

    float t = (float)(deficit - 65) / (500 - 65);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    int minGrey = 30;
    int maxGrey = 110;
    int greyVal = minGrey + (int)(t * (maxGrey - minGrey));

    return RGB(greyVal, greyVal, greyVal);
}

BOOL IsSectionHeaderFill(HDC hdc, const RECT *lprc, COLORREF brushColor) {
    BYTE r = GetRValue(brushColor);
    BYTE g = GetGValue(brushColor);
    BYTE b = GetBValue(brushColor);
    
    BOOL bIsNeutralGrey = (abs((int)r - (int)g) < 12 && abs((int)g - (int)b) < 12 && r > 180);
    if (!bIsNeutralGrey) return FALSE;
    
    RECT rcClient;
    HWND hwnd = GetWindowFromDCExtended(hdc);
    if (!hwnd || !GetClientRect(hwnd, &rcClient)) return FALSE;
    
    int fillWidth = lprc->right - lprc->left;
    int fillHeight = lprc->bottom - lprc->top;
    int clientWidth = rcClient.right - rcClient.left;
    
    return (fillWidth > (clientWidth * 9 / 10)) && (fillHeight >= 18 && fillHeight <= 35);
}

#define THEME_SUBCLASS_ID 1337
LRESULT CALLBACK TaskMgrSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            COLORREF bgCol = GetTargetBgColor(hdc);
            if (real_SetBkColor) real_SetBkColor(hdc, bgCol);
            if (real_SetTextColor) real_SetTextColor(hdc, RGB(255, 255, 255));
            
            // FIX: Use cached brushes to prevent GDI object leak
            static HBRUSH hDarkBrush = NULL;
            static HBRUSH hBlackBrush = NULL;
            if (!hDarkBrush && real_CreateSolidBrush) hDarkBrush = real_CreateSolidBrush(RGB(15, 15, 15));
            if (!hBlackBrush && real_CreateSolidBrush) hBlackBrush = real_CreateSolidBrush(RGB(0, 0, 0));
            
            WCHAR szClass[64] = {0};
            GetClassNameW(hwnd, szClass, 64);
            if (wcscmp(szClass, L"NativeHWNDHost") == 0 && hBlackBrush) {
                return (LRESULT)hBlackBrush;
            }
            return (LRESULT)(hDarkBrush ? hDarkBrush : GetStockObject(DKGRAY_BRUSH));
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            // FIX: Use cached brush instead of creating/deleting per erase
            static HBRUSH hEraseBrush = NULL;
            if (!hEraseBrush && real_CreateSolidBrush) hEraseBrush = real_CreateSolidBrush(GetTargetBgColor(hdc));
            if (hEraseBrush && real_FillRect) {
                real_FillRect(hdc, &rc, hEraseBrush);
            }
            return 1;
        }
        case WM_NCACTIVATE:
        case WM_NCPAINT:
        case WM_WINDOWPOSCHANGED: {
            BOOL isTopLevel = !(GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD);
            if (isTopLevel && real_DwmSetWindowAttribute) {
                BOOL bDark = TRUE;
                real_DwmSetWindowAttribute(hwnd, 19, &bDark, sizeof(BOOL)); 
                real_DwmSetWindowAttribute(hwnd, 20, &bDark, sizeof(BOOL)); 
                
                COLORREF titleBarColor = RGB(15, 15, 15);
                COLORREF textColor = RGB(255, 255, 255);
                real_DwmSetWindowAttribute(hwnd, 35, &titleBarColor, sizeof(COLORREF));
                real_DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(COLORREF));
            }
            break;
        }
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void ApplyImmersiveDarkMode(HWND hwnd) {
    if (!hwnd) return;
    
    // Prevent double-subclassing
    if (IsWindowSubclassed(hwnd)) {
        return;
    }
    
    WCHAR szClass[64] = {0};
    GetClassNameW(hwnd, szClass, 64);
    
    // CRITICAL FIX: Skip standard Windows dialogs to prevent crashes in "Run new task"
    // These classes are used by common dialogs like Open/Save/Run
    if (wcscmp(szClass, L"#32770") == 0 || // Dialog box
        wcscmp(szClass, L"Edit") == 0 ||
        wcscmp(szClass, L"Button") == 0 ||
        wcscmp(szClass, L"ComboBox") == 0 ||
        wcscmp(szClass, L"ListBox") == 0) {
        return; 
    }

    BOOL isTopLevel = !(GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD);

    if (AllowDarkModeForWindow) AllowDarkModeForWindow(hwnd, TRUE);
    
    // Prevent overriding the primary window frame layout with wrong explorer metrics
    if (wcscmp(szClass, L"TaskManagerWindow") == 0 || isTopLevel) {
        if (real_SetWindowTheme) real_SetWindowTheme(hwnd, L"DarkMode", NULL);
    } else {
        if (real_SetWindowTheme) real_SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
    }

    if (isTopLevel && real_DwmSetWindowAttribute) {
        BOOL bDark = TRUE;
        real_DwmSetWindowAttribute(hwnd, 19, &bDark, sizeof(BOOL)); 
        real_DwmSetWindowAttribute(hwnd, 20, &bDark, sizeof(BOOL)); 
        
        COLORREF titleBarColor = RGB(15, 15, 15);
        COLORREF textColor = RGB(255, 255, 255);
        real_DwmSetWindowAttribute(hwnd, 35, &titleBarColor, sizeof(COLORREF));
        real_DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(COLORREF));
    }
    
    SetWindowSubclass(hwnd, TaskMgrSubclassProc, THEME_SUBCLASS_ID, 0);
    MarkWindowSubclassed(hwnd);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

HDC WINAPI CreateCompatibleDC_Hook(HDC hdc) {
    HDC hNewDC = real_CreateCompatibleDC ? real_CreateCompatibleDC(hdc) : NULL;
    if (hNewDC && hdc) {
        HWND hwnd = WindowFromDC(hdc);
        if (hwnd) {
            RegisterDC(hNewDC, hwnd);
        }
    }
    return hNewDC;
}

BOOL WINAPI DeleteDC_Hook(HDC hdc) {
    UnregisterDC(hdc);
    return real_DeleteDC ? real_DeleteDC(hdc) : FALSE;
}

COLORREF WINAPI GetSysColor_Hook(int nIndex) {
    switch (nIndex) {
        case COLOR_WINDOW:      case COLOR_BACKGROUND: return RGB(15, 15, 15);
        case COLOR_WINDOWTEXT:  case COLOR_BTNTEXT:    case COLOR_CAPTIONTEXT:
        case COLOR_MENUTEXT:    case COLOR_INFOTEXT:   return RGB(255, 255, 255);
        case COLOR_BTNFACE:                            return RGB(25, 25, 25);
        case COLOR_MENUBAR:     case COLOR_MENU:       return RGB(15, 15, 15);
        case COLOR_ACTIVEBORDER:                       return RGB(35, 35, 35);
    }
    return real_GetSysColor ? real_GetSysColor(nIndex) : GetSysColor(nIndex);
}

HBRUSH WINAPI GetSysColorBrush_Hook(int nIndex) {
    switch (nIndex) {
        case COLOR_WINDOW:     case COLOR_BACKGROUND: { static HBRUSH b = NULL; if(!b && real_CreateSolidBrush) b = real_CreateSolidBrush(RGB(15, 15, 15)); return b; }
        case COLOR_BTNFACE:                           { static HBRUSH b = NULL; if(!b && real_CreateSolidBrush) b = real_CreateSolidBrush(RGB(25, 25, 25)); return b; }
        case COLOR_MENU:       case COLOR_MENUBAR:    { static HBRUSH b = NULL; if(!b && real_CreateSolidBrush) b = real_CreateSolidBrush(RGB(15, 15, 15)); return b; }
    }
    return real_GetSysColorBrush ? real_GetSysColorBrush(nIndex) : GetSysColorBrush(nIndex);
}

COLORREF WINAPI SetTextColor_Hook(HDC hdc, COLORREF color) {
    if (!real_SetTextColor) return color;
    
    HWND hwnd = GetWindowFromDCExtended(hdc);
    if (hwnd) {
        WCHAR szClass[64] = {0};
        GetClassNameW(hwnd, szClass, 64);
        
        if (wcscmp(szClass, L"NativeHWNDHost") == 0) {
            return real_SetTextColor(hdc, color);
        }

        // FIX: Target the main window frame where the classic menu bar lives
        if (wcscmp(szClass, L"TaskManagerWindow") == 0) {
            BYTE r = GetRValue(color);
            BYTE g = GetGValue(color);
            BYTE b = GetBValue(color);
            // If the system tries to draw dark text (File, Options, View), force it to stay crisp black
            if (r < 150 && g < 150 && b < 150) {
                return real_SetTextColor(hdc, RGB(0, 0, 0));
            }
        }
    }

    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);
    
    if (r < 150 && g < 150 && b < 150) {
        return real_SetTextColor(hdc, RGB(255, 255, 255));
    }
    if (b > r && b > g && b > 100) {
        return real_SetTextColor(hdc, RGB(255, 255, 255));
    }
    return real_SetTextColor(hdc, color);
}

COLORREF WINAPI SetBkColor_Hook(HDC hdc, COLORREF color) {
    if (!real_SetBkColor) return color;
    return real_SetBkColor(hdc, ConvertTaskMgrColor(color, hdc));
}

HBRUSH WINAPI CreateSolidBrush_Hook(COLORREF color) {
    if (!real_CreateSolidBrush) return CreateSolidBrush(color);
    return real_CreateSolidBrush(ConvertTaskMgrColor(color, NULL));
}

COLORREF WINAPI SetDCBrushColor_Hook(HDC hdc, COLORREF color) {
    if (!real_SetDCBrushColor) return color;
    return real_SetDCBrushColor(hdc, ConvertTaskMgrColor(color, hdc));
}

COLORREF WINAPI SetDCPenColor_Hook(HDC hdc, COLORREF color) {
    if (!real_SetDCPenColor) return color;
    
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);
    if (r > 200 && g > 200 && b > 200) {
        return real_SetDCPenColor(hdc, RGB(45, 45, 45)); 
    }
    return real_SetDCPenColor(hdc, color);
}

HPEN WINAPI CreatePen_Hook(int iStyle, int cWidth, COLORREF color) {
    if (!real_CreatePen) return CreatePen(iStyle, cWidth, color);
    
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);
    if (r > 200 && g > 200 && b > 200) {
        return real_CreatePen(iStyle, cWidth, RGB(45, 45, 45));
    }
    return real_CreatePen(iStyle, cWidth, color);
}

HPEN WINAPI CreatePenIndirect_Hook(const LOGPEN *plp) {
    if (!real_CreatePenIndirect) return CreatePenIndirect(plp);
    
    if (plp) {
        BYTE r = GetRValue(plp->lopnColor);
        BYTE g = GetGValue(plp->lopnColor);
        BYTE b = GetBValue(plp->lopnColor);
        if (r > 200 && g > 200 && b > 200) {
            LOGPEN lp = *plp;
            lp.lopnColor = RGB(45, 45, 45);
            return real_CreatePenIndirect(&lp);
        }
    }
    return real_CreatePenIndirect(plp);
}

HGDIOBJ WINAPI GetStockObject_Hook(int fnObject) {
    if (fnObject == WHITE_BRUSH) {
        static HBRUSH darkBrush = NULL;
        if (!darkBrush && real_CreateSolidBrush) darkBrush = real_CreateSolidBrush(RGB(15, 15, 15));
        return darkBrush ? darkBrush : real_GetStockObject(fnObject);
    }
    if (fnObject == LTGRAY_BRUSH) {
        static HBRUSH darkBrush = NULL;
        if (!darkBrush && real_CreateSolidBrush) darkBrush = real_CreateSolidBrush(RGB(25, 25, 25));
        return darkBrush ? darkBrush : real_GetStockObject(fnObject);
    }
    return real_GetStockObject ? real_GetStockObject(fnObject) : GetStockObject(fnObject);
}

BOOL WINAPI PatBlt_Hook(HDC hdc, int x, int y, int w, int h, DWORD dwRop) {
    if (!real_PatBlt || !real_CreateSolidBrush || !real_FillRect) {
        return PatBlt(hdc, x, y, w, h, dwRop);
    }
    
    if (dwRop == WHITENESS || dwRop == BLACKNESS) {
        RECT r = { x, y, x + w, y + h };
        HBRUSH hDark = real_CreateSolidBrush(GetTargetBgColor(hdc));
        if (hDark) {
            real_FillRect(hdc, &r, hDark);
            DeleteObject(hDark);
            return TRUE;
        }
    }
    return real_PatBlt(hdc, x, y, w, h, dwRop);
}

BOOL WINAPI BitBlt_Hook(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) {
    if (!real_BitBlt || !real_CreateSolidBrush || !real_FillRect) {
        return BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
    }
    
    if (rop == WHITENESS || rop == BLACKNESS) {
        RECT r = { x, y, x + cx, y + cy };
        HBRUSH hDark = real_CreateSolidBrush(GetTargetBgColor(hdc));
        if (hDark) {
            real_FillRect(hdc, &r, hDark);
            DeleteObject(hDark);
            return TRUE;
        }
    }
    return real_BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
}

BOOL WINAPI StretchBlt_Hook(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop) {
    if (!real_StretchBlt || !real_CreateSolidBrush || !real_FillRect) {
        return StretchBlt(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop);
    }
    
    if (rop == WHITENESS || rop == BLACKNESS) {
        RECT r = { xDest, yDest, xDest + wDest, yDest + hDest };
        HBRUSH hDark = real_CreateSolidBrush(GetTargetBgColor(hdcDest));
        if (hDark) {
            real_FillRect(hdcDest, &r, hDark);
            DeleteObject(hDark);
            return TRUE;
        }
    }
    return real_StretchBlt(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop);
}

HTHEME WINAPI OpenThemeData_Hook(HWND hwnd, LPCWSTR pszClassList) {
    HTHEME hTheme = real_OpenThemeData ? real_OpenThemeData(hwnd, pszClassList) : OpenThemeData(hwnd, pszClassList);
    if (hTheme && pszClassList) RegisterTheme(hTheme, pszClassList);
    return hTheme;
}

HRESULT WINAPI DrawThemeBackground_Hook(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT *pRect, const RECT *pClipRect) {
    if (!real_DrawThemeBackground) {
        return DrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
    }
    
    LPCWSTR cls = hTheme ? GetThemeClassName(hTheme) : NULL;
    
    HWND hwnd = WindowFromDC(hdc);
    WCHAR szClass[64] = {0};
    if (hwnd) {
        GetClassNameW(hwnd, szClass, 64);
        
        // FIX 1: Force the topmost title bar / caption frame to turn black
        // Use the hooked version to avoid recursion issues
        if (wcscmp(szClass, L"TaskManagerWindow") == 0 && real_DwmSetWindowAttribute) {
            BOOL useDarkMode = TRUE;
            real_DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
            real_DwmSetWindowAttribute(hwnd, 19, &useDarkMode, sizeof(useDarkMode)); // Fallback for older Win10 builds
        }
    }

    // Check if this is a standard menu class string
    BOOL isMenu = (cls && (wcscmp(cls, L"Menu") == 0 || wcsstr(cls, L"Menu") != NULL));
    
    if (wcscmp(szClass, L"TaskManagerWindow") == 0 && (iPartId == 7 || iPartId == 8)) {
        isMenu = TRUE;
    }
    
    if (isMenu) {
        if (iPartId == 7) { // MENU_BARBACKGROUND
            if (real_CreateSolidBrush && real_FillRect) {
                HBRUSH hBrush = real_CreateSolidBrush(RGB(0, 0, 0)); // Pure Black
                if (hBrush) {
                    real_FillRect(hdc, pRect, hBrush);
                    DeleteObject(hBrush);
                }
            }
            return S_OK;
        }
        if (iPartId == 8) { // MENU_BARITEM
            COLORREF bg = RGB(0, 0, 0); // Pure Black when idle
            if (iStateId == 2 || iStateId == 3) { // MBI_HOT or MBI_PUSHED
                bg = RGB(55, 55, 55); 
            }
            if (real_CreateSolidBrush && real_FillRect) {
                HBRUSH hBrush = real_CreateSolidBrush(bg);
                if (hBrush) {
                    real_FillRect(hdc, pRect, hBrush);
                    DeleteObject(hBrush);
                }
            }
            return S_OK;
        }
        if (iPartId == 9) { // MENU_POPUPBACKGROUND
            if (real_CreateSolidBrush && real_FillRect) {
                HBRUSH hBrush = real_CreateSolidBrush(RGB(25, 25, 25));
                if (hBrush) {
                    real_FillRect(hdc, pRect, hBrush);
                    DeleteObject(hBrush);
                }
            }
            return S_OK;
        }
        if (iPartId == 14) { // MENU_POPUPITEM
            COLORREF bg = RGB(25, 25, 25);
            if (iStateId == 2) { // MPI_HOT
                bg = RGB(50, 50, 50);
            }
            if (real_CreateSolidBrush && real_FillRect) {
                HBRUSH hBrush = real_CreateSolidBrush(bg);
                if (hBrush) {
                    real_FillRect(hdc, pRect, hBrush);
                    DeleteObject(hBrush);
                }
            }
            return S_OK;
        }
    }

    if (cls && wcscmp(cls, L"Button") == 0) {
        if (iPartId == 1) { // BP_PUSHBUTTON
            COLORREF btnBg = RGB(45, 45, 45);
            COLORREF btnBorder = RGB(80, 80, 80);
            
            if (iStateId == 2) { // PBS_HOT
                btnBg = RGB(65, 65, 65);
                btnBorder = RGB(110, 110, 110);
            } else if (iStateId == 3) { // PBS_PRESSED
                btnBg = RGB(25, 25, 25);
                btnBorder = RGB(60, 60, 60);
            } else if (iStateId == 4) { // PBS_DISABLED
                btnBg = RGB(30, 30, 30);
                btnBorder = RGB(45, 45, 45);
            } else if (iStateId == 5) { // PBS_DEFAULTED
                btnBg = RGB(45, 45, 45);
                btnBorder = RGB(0, 120, 215); 
            }
            
            if (real_CreateSolidBrush && real_FillRect) {
                HBRUSH hBrush = real_CreateSolidBrush(btnBg);
                if (hBrush) {
                    real_FillRect(hdc, pRect, hBrush);
                    DeleteObject(hBrush);
                }
            }
            
            if (real_CreatePen && real_GetStockObject) {
                HPEN hPen = real_CreatePen(PS_SOLID, 1, btnBorder);
                if (hPen) {
                    HGDIOBJ oldPen = SelectObject(hdc, hPen);
                    HGDIOBJ oldBrush = SelectObject(hdc, real_GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, pRect->left, pRect->top, pRect->right, pRect->bottom);
                    SelectObject(hdc, oldBrush);
                    SelectObject(hdc, oldPen);
                    DeleteObject(hPen);
                }
            }
            return S_OK;
        }
    }

    if (cls && wcscmp(cls, L"TreeView") == 0) {
        if (real_CreateSolidBrush && real_FillRect) {
            HBRUSH hThemeBrush = real_CreateSolidBrush(RGB(15, 15, 15));
            if (hThemeBrush) {
                real_FillRect(hdc, pRect, hThemeBrush);
                DeleteObject(hThemeBrush);
            }
        }
        return S_OK;
    }

    if (cls && wcscmp(cls, L"Tab") == 0) {
        if (real_CreateSolidBrush && real_FillRect) {
            HBRUSH hThemeBrush = real_CreateSolidBrush(RGB(20, 20, 20));
            if (hThemeBrush) {
                real_FillRect(hdc, pRect, hThemeBrush);
                DeleteObject(hThemeBrush);
            }
        }
        return S_OK;
    }
    
    if (cls && wcscmp(cls, L"Header") == 0) {
        if (real_CreateSolidBrush && real_FillRect) {
            HBRUSH hThemeBrush = real_CreateSolidBrush(RGB(30, 30, 30));
            if (hThemeBrush) {
                real_FillRect(hdc, pRect, hThemeBrush);
                DeleteObject(hThemeBrush);
            }
        }
        
        if (real_CreatePen) {
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            if (hPen) {
                HGDIOBJ oldPen = SelectObject(hdc, hPen);
                MoveToEx(hdc, pRect->right - 1, pRect->top, NULL);
                LineTo(hdc, pRect->right - 1, pRect->bottom);
                SelectObject(hdc, oldPen);
                DeleteObject(hPen);
            }
        }
        return S_OK;
    }
    
    return real_DrawThemeBackground(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
}

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpszClassName, LPCWSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    HWND hwnd = real_CreateWindowExW ? real_CreateWindowExW(dwExStyle, lpszClassName, lpszWindowName, dwStyle, x, y, nWidth, nHeight, hwndParent, hMenu, hInstance, lpParam) : 
                                     CreateWindowExW(dwExStyle, lpszClassName, lpszWindowName, dwStyle, x, y, nWidth, nHeight, hwndParent, hMenu, hInstance, lpParam);
    
    if (hwnd) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId()) {
            WCHAR szClass[64] = {0};
            GetClassNameW(hwnd, szClass, 64);
            
            // Skip standard dialog classes entirely - let Windows handle them
            if (wcscmp(szClass, L"#32770") != 0 && 
                wcscmp(szClass, L"Edit") != 0 &&
                wcscmp(szClass, L"Button") != 0 &&
                wcscmp(szClass, L"ComboBox") != 0 &&
                wcscmp(szClass, L"ListBox") != 0 &&
                wcscmp(szClass, L"Static") != 0) {
                
                // Only apply DWM/theme attributes here, NOT subclassing
                if (AllowDarkModeForWindow) AllowDarkModeForWindow(hwnd, TRUE);
                
                BOOL isTopLevel = !(dwStyle & WS_CHILD);
                if (wcscmp(szClass, L"TaskManagerWindow") == 0 || isTopLevel) {
                    if (real_SetWindowTheme) real_SetWindowTheme(hwnd, L"DarkMode", NULL);
                } else {
                    if (real_SetWindowTheme) real_SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
                }
                
                if (isTopLevel && real_DwmSetWindowAttribute) {
                    BOOL bDark = TRUE;
                    real_DwmSetWindowAttribute(hwnd, 19, &bDark, sizeof(BOOL));
                    real_DwmSetWindowAttribute(hwnd, 20, &bDark, sizeof(BOOL));
                }
            }
        }
    }
    return hwnd;
}

BOOL CALLBACK EnumChildWindowsProc(HWND child, LPARAM lParam) {
    ApplyImmersiveDarkMode(child);
    return TRUE;
}

BOOL CALLBACK SafeSubclassEnumProc(HWND hwnd, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && !IsWindowSubclassed(hwnd)) {
        WCHAR szClass[64] = {0};
        GetClassNameW(hwnd, szClass, 64);
        
        // Only subclass top-level Task Manager windows, never dialogs or standard controls
        BOOL isTopLevel = !(GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD);
        if (isTopLevel && (wcscmp(szClass, L"TaskManagerWindow") == 0 || 
                           wcscmp(szClass, L"NativeHWNDHost") == 0)) {
            SetWindowSubclass(hwnd, TaskMgrSubclassProc, THEME_SUBCLASS_ID, 0);
            MarkWindowSubclassed(hwnd);
        }
    }
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) {
        // Defer subclassing to avoid race conditions during window creation
        PostMessage(hwnd, WM_NULL, 0, 0);
        EnumChildWindows(hwnd, SafeSubclassEnumProc, 0);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    }
    return TRUE;
}

HRESULT WINAPI DwmSetWindowAttribute_Hook(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute) {
    if (!real_DwmSetWindowAttribute) {
        return DwmSetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
    }
    
    if (dwAttribute == 19 || dwAttribute == 20) {
        BOOL bDark = TRUE;
        return real_DwmSetWindowAttribute(hwnd, dwAttribute, &bDark, sizeof(BOOL));
    }
    if (dwAttribute == 35 || dwAttribute == 36) {
        COLORREF col = (dwAttribute == 35) ? RGB(15, 15, 15) : RGB(255, 255, 255);
        return real_DwmSetWindowAttribute(hwnd, dwAttribute, &col, sizeof(COLORREF));
    }
    return real_DwmSetWindowAttribute(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

HRESULT WINAPI SetWindowTheme_Hook(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList) {
    if (!real_SetWindowTheme) {
        return SetWindowTheme(hwnd, pszSubAppName, pszSubIdList);
    }
    
    WCHAR szClass[64] = {0};
    GetClassNameW(hwnd, szClass, 64);
    if (wcscmp(szClass, L"TaskManagerWindow") == 0) {
        return real_SetWindowTheme(hwnd, L"DarkMode", NULL);
    }
    return real_SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
}

BOOL WINAPI GradientFill_Hook(HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode) {
    if (!real_GradientFill) {
        return GradientFill(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
    }
    
    for (ULONG i = 0; i < nVertex; i++) {
        COLORREF color = RGB(pVertex[i].Red >> 8, pVertex[i].Green >> 8, pVertex[i].Blue >> 8);
        COLORREF newColor = ConvertTaskMgrColor(color, hdc);
        if (newColor != color) {
            pVertex[i].Red = GetRValue(newColor) * 256;
            pVertex[i].Green = GetGValue(newColor) * 256;
            pVertex[i].Blue = GetBValue(newColor) * 256;
        }
    }
    return real_GradientFill(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

int WINAPI FillRect_Hook(HDC hdc, const RECT *lprc, HBRUSH hbr) {
    if (!real_FillRect) {
        return FillRect(hdc, lprc, hbr);
    }
    
    HWND hwnd = GetWindowFromDCExtended(hdc);
    if (hwnd) {
        WCHAR szClass[64] = {0};
        GetClassNameW(hwnd, szClass, 64);
        
        // TARGET: The tab bar control
        if (wcscmp(szClass, L"SysTabControl32") == 0) {
            if (real_CreateSolidBrush) {
                HBRUSH hBlackBrush = real_CreateSolidBrush(RGB(0, 0, 0));
                if (hBlackBrush) {
                    int result = real_FillRect(hdc, lprc, hBlackBrush);
                    DeleteObject(hBlackBrush);
                    return result;
                }
            }
        }
    }
    return real_FillRect(hdc, lprc, hbr);
}

BOOL WINAPI ExtTextOutW_Hook(HDC hdc, int x, int y, UINT fuOptions, const RECT *lprect, LPCWSTR lpString, UINT cchString, CONST INT *lpDx) {
    if (!real_ExtTextOutW) {
        return ExtTextOutW(hdc, x, y, fuOptions, lprect, lpString, cchString, lpDx);
    }
    
    if (lpString && cchString > 0) {
        HWND hwnd = GetWindowFromDCExtended(hdc);
        if (hwnd) {
            WCHAR szClass[64] = {0};
            GetClassNameW(hwnd, szClass, 64);
            // Ensure we are only targeting text drawn directly on the main Task Manager window frame
            if (wcscmp(szClass, L"TaskManagerWindow") == 0) {
                if ((cchString == 4 && wcsncmp(lpString, L"File", 4) == 0) ||
                    (cchString == 5 && wcsncmp(lpString, L"&File", 5) == 0) ||
                    (cchString == 7 && wcsncmp(lpString, L"Options", 7) == 0) ||
                    (cchString == 8 && wcsncmp(lpString, L"&Options", 8) == 0) ||
                    (cchString == 4 && wcsncmp(lpString, L"View", 4) == 0) ||
                    (cchString == 5 && wcsncmp(lpString, L"&View", 5) == 0)) {
                    
                    // Force the text color to black so it is readable on the white menu bar
                    if (real_SetTextColor) {
                        real_SetTextColor(hdc, RGB(0, 0, 0));
                    }
                }
            }
        }
    }

    if (fuOptions & ETO_OPAQUE && lprect) {
        COLORREF bkColor = GetBkColor(hdc);
        COLORREF newBkColor = ConvertTaskMgrColor(bkColor, hdc);
        if (newBkColor != bkColor && real_SetBkColor) {
            real_SetBkColor(hdc, newBkColor);
        }
    }
    return real_ExtTextOutW(hdc, x, y, fuOptions, lprect, lpString, cchString, lpDx);
}

int WINAPI FillRgn_Hook(HDC hdc, HRGN hrgn, HBRUSH hbr) {
    if (!real_FillRgn) {
        return FillRgn(hdc, hrgn, hbr);
    }
    
    if (hbr && real_CreateSolidBrush) {
        LOGBRUSH lb;
        if (GetObject(hbr, sizeof(LOGBRUSH), &lb)) {
            COLORREF newColor = ConvertTaskMgrColor(lb.lbColor, hdc);
            if (newColor != lb.lbColor) {
                HBRUSH hNewBrush = real_CreateSolidBrush(newColor);
                if (hNewBrush) {
                    int result = real_FillRgn(hdc, hrgn, hNewBrush);
                    DeleteObject(hNewBrush);
                    return result;
                }
            }
        }
    }
    return real_FillRgn(hdc, hrgn, hbr);
}

int WINAPI FrameRgn_Hook(HDC hdc, HRGN hrgn, HBRUSH hbr, int w, int h) {
    if (!real_FrameRgn) {
        return FrameRgn(hdc, hrgn, hbr, w, h);
    }
    
    if (hbr && real_CreateSolidBrush) {
        LOGBRUSH lb;
        if (GetObject(hbr, sizeof(LOGBRUSH), &lb)) {
            BYTE r = GetRValue(lb.lbColor);
            BYTE g = GetGValue(lb.lbColor);
            BYTE b = GetBValue(lb.lbColor);
            if (r > 200 && g > 200 && b > 200) {
                HBRUSH hDarkBrush = real_CreateSolidBrush(RGB(45, 45, 45));
                if (hDarkBrush) {
                    int result = real_FrameRgn(hdc, hrgn, hDarkBrush, w, h);
                    DeleteObject(hDarkBrush);
                    return result;
                }
            }
        }
    }
    return real_FrameRgn(hdc, hrgn, hbr, w, h);
}

void InitDarkMode() {
    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (hUxTheme) {
        SetPreferredAppMode = (pfnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
        if (SetPreferredAppMode) SetPreferredAppMode(ForceDark);

        AllowDarkModeForWindow = (pfnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
        
        FlushMenuThemes = (pfnFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));
        if (FlushMenuThemes) FlushMenuThemes();
    }
}

BOOL Wh_ModInit() {
    InitDarkMode();

    Wh_SetFunctionHook((void*)GetSysColor, (void*)GetSysColor_Hook, (void**)&real_GetSysColor);
    Wh_SetFunctionHook((void*)GetSysColorBrush, (void*)GetSysColorBrush_Hook, (void**)&real_GetSysColorBrush);
    Wh_SetFunctionHook((void*)SetTextColor, (void*)SetTextColor_Hook, (void**)&real_SetTextColor);
    Wh_SetFunctionHook((void*)SetBkColor, (void*)SetBkColor_Hook, (void**)&real_SetBkColor);
    Wh_SetFunctionHook((void*)CreateSolidBrush, (void*)CreateSolidBrush_Hook, (void**)&real_CreateSolidBrush);
    Wh_SetFunctionHook((void*)GetStockObject, (void*)GetStockObject_Hook, (void**)&real_GetStockObject);
    Wh_SetFunctionHook((void*)PatBlt, (void*)PatBlt_Hook, (void**)&real_PatBlt);
    Wh_SetFunctionHook((void*)BitBlt, (void*)BitBlt_Hook, (void**)&real_BitBlt);
    Wh_SetFunctionHook((void*)StretchBlt, (void*)StretchBlt_Hook, (void**)&real_StretchBlt);
    Wh_SetFunctionHook((void*)OpenThemeData, (void*)OpenThemeData_Hook, (void**)&real_OpenThemeData);
    Wh_SetFunctionHook((void*)DrawThemeBackground, (void*)DrawThemeBackground_Hook, (void**)&real_DrawThemeBackground);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&real_CreateWindowExW);

    Wh_SetFunctionHook((void*)SetDCBrushColor, (void*)SetDCBrushColor_Hook, (void**)&real_SetDCBrushColor);
    Wh_SetFunctionHook((void*)SetDCPenColor, (void*)SetDCPenColor_Hook, (void**)&real_SetDCPenColor);
    Wh_SetFunctionHook((void*)CreatePen, (void*)CreatePen_Hook, (void**)&real_CreatePen);
    Wh_SetFunctionHook((void*)CreatePenIndirect, (void*)CreatePenIndirect_Hook, (void**)&real_CreatePenIndirect);
    
    Wh_SetFunctionHook((void*)CreateCompatibleDC, (void*)CreateCompatibleDC_Hook, (void**)&real_CreateCompatibleDC);
    Wh_SetFunctionHook((void*)DeleteDC, (void*)DeleteDC_Hook, (void**)&real_DeleteDC);

    HMODULE hDwmApi = GetModuleHandleW(L"dwmapi.dll");
    if (!hDwmApi) hDwmApi = LoadLibraryW(L"dwmapi.dll");
    if (hDwmApi) {
        Wh_SetFunctionHook((void*)GetProcAddress(hDwmApi, "DwmSetWindowAttribute"), (void*)DwmSetWindowAttribute_Hook, (void**)&real_DwmSetWindowAttribute);
    }

    HMODULE hUxThemeInit = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxThemeInit) hUxThemeInit = LoadLibraryW(L"uxtheme.dll");
    if (hUxThemeInit) {
        Wh_SetFunctionHook((void*)GetProcAddress(hUxThemeInit, "SetWindowTheme"), (void*)SetWindowTheme_Hook, (void**)&real_SetWindowTheme);
    }

    HMODULE hMsImg32 = GetModuleHandleW(L"msimg32.dll");
    if (!hMsImg32) hMsImg32 = LoadLibraryW(L"msimg32.dll");
    
    FARPROC pGradientFill = NULL;
    if (hMsImg32) pGradientFill = GetProcAddress(hMsImg32, "GradientFill");
    if (!pGradientFill) {
        HMODULE hGdi32 = GetModuleHandleW(L"gdi32.dll");
        if (hGdi32) pGradientFill = GetProcAddress(hGdi32, "GradientFill");
    }
    
    if (pGradientFill) {
        Wh_SetFunctionHook((void*)pGradientFill, (void*)GradientFill_Hook, (void**)&real_GradientFill);
    }

    Wh_SetFunctionHook((void*)FillRect, (void*)FillRect_Hook, (void**)&real_FillRect);
    Wh_SetFunctionHook((void*)ExtTextOutW, (void*)ExtTextOutW_Hook, (void**)&real_ExtTextOutW);
    Wh_SetFunctionHook((void*)FillRgn, (void*)FillRgn_Hook, (void**)&real_FillRgn);
    Wh_SetFunctionHook((void*)FrameRgn, (void*)FrameRgn_Hook, (void**)&real_FrameRgn);

    EnumWindows(EnumWindowsProc, 0);
    return TRUE;
}
