#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include "resource.h"
#include "Theme.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

HWND g_hTab = nullptr;
HFONT g_hFont = nullptr;
HBRUSH g_hbrBackground = nullptr;

const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";

struct SettingItem {
    std::wstring label;
    std::wstring regValue;
    HWND hWnd = nullptr;
};

std::vector<SettingItem> g_fileSettings = {
    { L"Enable Attributes", L"Files_Attributes" },
    { L"Enable Edit With", L"Files_EditWith" },
    { L"Enable Copy Name", L"Files_CopyName" },
    { L"Enable Copy Path", L"Files_CopyPath" },
    { L"Enable Take Ownership", L"Files_TakeOwnership" }
};

std::vector<SettingItem> g_dirSettings = {
    { L"Enable Attributes", L"Directory_Attributes" },
    { L"Enable Terminal", L"Directory_Terminal" },
    { L"Enable Terminal (admin)", L"Directory_TerminalAdmin" },
    { L"Enable System Folders", L"Directory_SystemFolders" },
    { L"Enable Paste to File", L"Directory_PasteToFile" },
    { L"Enable Copy Name", L"Directory_CopyName" },
    { L"Enable Copy Path", L"Directory_CopyPath" },
    { L"Enable Take Ownership", L"Directory_TakeOwnership" }
};

std::vector<SettingItem> g_bgSettings = {
    { L"Enable Terminal", L"Background_Terminal" },
    { L"Enable Terminal (admin)", L"Background_TerminalAdmin" },
    { L"Enable System Folders", L"Background_SystemFolders" },
    { L"Enable Paste to File", L"Background_PasteToFile" }
};

bool GetSetting(const wchar_t* name) {
    DWORD value = 1, size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, REG_PATH, name, RRF_RT_REG_DWORD, NULL, &value, &size);
    return value != 0;
}

void SetSetting(const wchar_t* name, bool enabled) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : 0;
        RegSetValueExW(hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void UpdateTabVisibility() {
    int sel = TabCtrl_GetCurSel(g_hTab);
    auto ToggleGroup = [&](std::vector<SettingItem>& group, bool show) {
        for (auto& item : group) {
            ShowWindow(item.hWnd, show ? SW_SHOW : SW_HIDE);
        }
    };

    ToggleGroup(g_fileSettings, sel == 0);
    ToggleGroup(g_dirSettings, sel == 1);
    ToggleGroup(g_bgSettings, sel == 2);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, DARK_TEXT);
        SetBkColor(hdc, DARK_BACKGROUND);
        return (LRESULT)g_hbrBackground;
    }
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        g_hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 10, 10, 380, 340, hwnd, NULL, hInst, NULL);
        SendMessage(g_hTab, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        TCITEMW tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"Files";
        TabCtrl_InsertItem(g_hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"Directory";
        TabCtrl_InsertItem(g_hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"Directory Background";
        TabCtrl_InsertItem(g_hTab, 2, &tie);

        auto CreateCheckboxes = [&](std::vector<SettingItem>& group) {
            int y = 50;
            for (auto& item : group) {
                item.hWnd = CreateWindowW(L"BUTTON", item.label.c_str(), WS_CHILD | BS_AUTOCHECKBOX, 30, y, 300, 25, hwnd, NULL, hInst, NULL);
                SendMessage(item.hWnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SendMessage(item.hWnd, BM_SETCHECK, GetSetting(item.regValue.c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
                y += 30;
            }
        };

        CreateCheckboxes(g_fileSettings);
        CreateCheckboxes(g_dirSettings);
        CreateCheckboxes(g_bgSettings);

        UpdateTabVisibility();
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == TCN_SELCHANGE) {
            UpdateTabVisibility();
        }
        return 0;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == BN_CLICKED) {
            HWND hCtrl = (HWND)lParam;
            bool checked = SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED;

            auto UpdateGroup = [&](std::vector<SettingItem>& group) {
                for (auto& item : group) {
                    if (item.hWnd == hCtrl) {
                        SetSetting(item.regValue.c_str(), checked);
                        break;
                    }
                }
            };
            UpdateGroup(g_fileSettings);
            UpdateGroup(g_dirSettings);
            UpdateGroup(g_bgSettings);
        }
        return 0;
    }
    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.Settings");

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    g_hbrBackground = CreateSolidBrush(DARK_BACKGROUND);

    const wchar_t CLASS_NAME[] = L"SettingsDialogClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = g_hbrBackground;
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"xToolsMenu Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 400, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(g_hbrBackground);
    return 0;
}
