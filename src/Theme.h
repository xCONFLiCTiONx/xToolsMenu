#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>

inline bool IsDarkMode()
{
    DWORD darkValue = 0;
    DWORD dataSize = sizeof(darkValue);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &darkValue, &dataSize) == ERROR_SUCCESS)
    {
        return darkValue == 0;
    }
    return false; // Default to Light Mode
}

inline std::wstring GetThemeSubDir()
{
    return IsDarkMode() ? L"Dark" : L"Light";
}
