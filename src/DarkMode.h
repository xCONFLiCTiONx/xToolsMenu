#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

enum class AppMode
{
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};

class DarkModeManager
{
public:
    static void Initialize();
    static bool IsDarkMode();
    static void ApplyTheme(HWND hwnd);
    static void ApplyToControls(HWND hwnd);
    static void FixTabControl(HWND hTab);
    static void OnSettingChange(HWND hwnd, LPARAM lParam);
    static void Cleanup();

    static HBRUSH GetBackgroundBrush();
    static COLORREF GetTextColor();
    static COLORREF GetBackgroundColor();
    static std::wstring GetThemeSubDir();

private:
    static void UpdateThemeInternal(HWND hwnd, bool dark);
};

// Global helper for simple usage in dialog procs
void ApplyNativeDarkMode(HWND hwnd);
