#include "DarkMode.h"
#include <vector>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// Undocumented UxTheme APIs for Windows 10/11 Dark Mode
typedef bool (WINAPI* pAllowDarkModeForApp)(bool allow);
typedef bool (WINAPI* pAllowDarkModeForWindow)(HWND hwnd, bool allow);
typedef void (WINAPI* pRefreshImmersiveColorPolicyState)();
typedef bool (WINAPI* pIsDarkModeAllowedForWindow)(HWND hwnd);
typedef void (WINAPI* pSetPreferredAppMode)(AppMode mode);
typedef bool (WINAPI* pIsDarkModeAllowedForApp)();
typedef void (WINAPI* pFlushMenuThemes)();

static pAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
static pAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
static pRefreshImmersiveColorPolicyState _RefreshImmersiveColorPolicyState = nullptr;
static pSetPreferredAppMode _SetPreferredAppMode = nullptr;
static pFlushMenuThemes _FlushMenuThemes = nullptr;

static HBRUSH g_hbrBackground = NULL;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void DarkModeManager::Initialize()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (hUxTheme)
    {
        _AllowDarkModeForApp = (pAllowDarkModeForApp)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(132));
        _AllowDarkModeForWindow = (pAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
        _RefreshImmersiveColorPolicyState = (pRefreshImmersiveColorPolicyState)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104));
        _SetPreferredAppMode = (pSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
        _FlushMenuThemes = (pFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));
    }

    if (_AllowDarkModeForApp) _AllowDarkModeForApp(true);
    if (_SetPreferredAppMode) _SetPreferredAppMode(AppMode::AllowDark);
    if (_RefreshImmersiveColorPolicyState) _RefreshImmersiveColorPolicyState();
}

bool DarkModeManager::IsDarkMode()
{
    DWORD darkValue = 0;
    DWORD dataSize = sizeof(darkValue);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &darkValue, &dataSize) == ERROR_SUCCESS)
    {
        return darkValue == 0;
    }
    return false;
}

void DarkModeManager::ApplyTheme(HWND hwnd)
{
    bool dark = IsDarkMode();

    // Set title bar dark mode
    BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    // Allow dark mode for the window
    if (_AllowDarkModeForWindow) _AllowDarkModeForWindow(hwnd, dark);

    // Apply "DarkMode_Explorer" theme to the window itself
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : nullptr, nullptr);

    // Apply to all child controls recursively
    ApplyToControls(hwnd);

    if (_RefreshImmersiveColorPolicyState) _RefreshImmersiveColorPolicyState();
    if (_FlushMenuThemes) _FlushMenuThemes();
}

void DarkModeManager::ApplyToControls(HWND hwnd)
{
    bool dark = IsDarkMode();

    EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp) -> BOOL {
        bool dark = (bool)lp;
        WCHAR className[256];
        GetClassNameW(hChild, className, 256);

        // Apply dark theme to standard controls
        if (_wcsicmp(className, L"BUTTON") == 0 ||
            _wcsicmp(className, L"EDIT") == 0 ||
            _wcsicmp(className, L"STATIC") == 0 ||
            _wcsicmp(className, WC_COMBOBOXW) == 0 ||
            _wcsicmp(className, WC_LISTVIEWW) == 0 ||
            _wcsicmp(className, WC_TREEVIEWW) == 0 ||
            _wcsicmp(className, WC_TABCONTROLW) == 0)
        {
            SetWindowTheme(hChild, dark ? L"DarkMode_Explorer" : nullptr, nullptr);

            if (_wcsicmp(className, L"BUTTON") == 0 || _wcsicmp(className, WC_COMBOBOXW) == 0)
            {
                InvalidateRect(hChild, NULL, TRUE);
            }
        }
        return TRUE;
    }, (LPARAM)dark);
}

LRESULT CALLBACK TabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        if (DarkModeManager::IsDarkMode())
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, DarkModeManager::GetBackgroundBrush());
            return TRUE;
        }
        break;
    }
    case WM_PAINT:
    {
        if (DarkModeManager::IsDarkMode())
        {
            // First let the control draw itself
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Then we can try to "darken" the tabs or the border
            // Standard tab control doesn't provide easy ways to theme the tabs themselves
            // without owner-draw. But the manifest + "Explorer" theme helps.
            return res;
        }
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, TabSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void DarkModeManager::FixTabControl(HWND hTab)
{
    if (hTab)
    {
        SetWindowSubclass(hTab, TabSubclassProc, 0, 0);
    }
}

void DarkModeManager::OnSettingChange(HWND hwnd, LPARAM lParam)
{
    if (lParam && _wcsicmp((const wchar_t*)lParam, L"ImmersiveColorSet") == 0)
    {
        Cleanup(); // Release old brush
        ApplyTheme(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void DarkModeManager::Cleanup()
{
    if (g_hbrBackground)
    {
        DeleteObject(g_hbrBackground);
        g_hbrBackground = NULL;
    }
}

void ApplyNativeDarkMode(HWND hwnd)
{
    DarkModeManager::Initialize();
    DarkModeManager::ApplyTheme(hwnd);
}

HBRUSH DarkModeManager::GetBackgroundBrush()
{
    if (!g_hbrBackground)
    {
        g_hbrBackground = CreateSolidBrush(GetBackgroundColor());
    }
    return g_hbrBackground;
}

COLORREF DarkModeManager::GetTextColor()
{
    return IsDarkMode() ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

COLORREF DarkModeManager::GetBackgroundColor()
{
    return IsDarkMode() ? RGB(32, 32, 32) : GetSysColor(COLOR_BTNFACE);
}

std::wstring DarkModeManager::GetThemeSubDir()
{
    return IsDarkMode() ? L"Dark" : L"Light";
}
