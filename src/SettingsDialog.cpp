#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <map>
#include "resource.h"
#include "DarkMode.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

HWND g_hTab = nullptr;
HFONT g_hFont = nullptr;

const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";
const wchar_t* REG_CUSTOM = L"Software\\xToolsMenu\\CustomCommands";

struct SettingItem {
    std::wstring label;
    std::wstring regValue;
    HWND hWnd = nullptr;
    bool isCustom = false;
};

std::vector<SettingItem> g_fileSettings = {
    { L"Attributes", L"Files_Attributes" },
    { L"Edit With", L"Files_EditWith" },
    { L"Copy Name", L"Files_CopyName" },
    { L"Copy Path", L"Files_CopyPath" },
    { L"Take Ownership", L"Files_TakeOwnership" }
};

std::vector<SettingItem> g_dirSettings = {
    { L"Attributes", L"Directory_Attributes" },
    { L"Terminal", L"Directory_Terminal" },
    { L"Terminal (admin)", L"Directory_TerminalAdmin" },
    { L"System Folders", L"Directory_SystemFolders" },
    { L"Paste to File", L"Directory_PasteToFile" },
    { L"Copy Name", L"Directory_CopyName" },
    { L"Copy Path", L"Directory_CopyPath" },
    { L"Take Ownership", L"Directory_TakeOwnership" }
};

std::vector<SettingItem> g_bgSettings = {
    { L"Attributes", L"Background_Attributes" },
    { L"Terminal", L"Background_Terminal" },
    { L"Terminal (admin)", L"Background_TerminalAdmin" },
    { L"System Folders", L"Background_SystemFolders" },
    { L"Paste to File", L"Background_PasteToFile" }
};

// Custom Tab Controls
HWND g_hViewport = nullptr;
HWND g_hPageContent = nullptr;
HWND g_hComboCustom = nullptr;
HWND g_hEditName = nullptr, g_hEditPath = nullptr, g_hEditArgs = nullptr, g_hEditIconLight = nullptr, g_hEditIconDark = nullptr;
HWND g_hBtnAdd = nullptr, g_hBtnEdit = nullptr, g_hBtnDel = nullptr, g_hBtnBrowse = nullptr, g_hBtnBrowseIconLight = nullptr, g_hBtnBrowseIconDark = nullptr;
HWND g_hStaticSelect = nullptr, g_hStaticName = nullptr, g_hStaticPath = nullptr, g_hStaticArgs = nullptr, g_hStaticIconLight = nullptr, g_hStaticIconDark = nullptr;
HWND g_hChkFile = nullptr, g_hChkDir = nullptr, g_hChkBG = nullptr, g_hChkAdmin = nullptr;
HWND g_hBtnBackup = nullptr, g_hBtnRestore = nullptr;

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

LRESULT CALLBACK ViewportProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_VSCROLL: {
        SCROLLINFO si = { sizeof(si), SIF_ALL };
        GetScrollInfo(hwnd, SB_VERT, &si);
        int oldPos = si.nPos;
        switch (LOWORD(wParam)) {
        case SB_TOP: si.nPos = si.nMin; break;
        case SB_BOTTOM: si.nPos = si.nMax; break;
        case SB_LINEUP: si.nPos -= 20; break;
        case SB_LINEDOWN: si.nPos += 20; break;
        case SB_PAGEUP: si.nPos -= si.nPage; break;
        case SB_PAGEDOWN: si.nPos += si.nPage; break;
        case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
        }
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);
        if (si.nPos != oldPos) {
            SetWindowPos(g_hPageContent, NULL, 0, -si.nPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
        SendMessage(hwnd, WM_VSCROLL, (short)HIWORD(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        return 0;
    case WM_COMMAND:
    case WM_NOTIFY:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        return SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
    case WM_ERASEBKGND:
        if (DarkModeManager::IsDarkMode()) {
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, DarkModeManager::GetBackgroundBrush());
            return TRUE;
        }
        break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PageContentProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        return SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
    case WM_ERASEBKGND:
        if (DarkModeManager::IsDarkMode()) {
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, DarkModeManager::GetBackgroundBrush());
            return TRUE;
        }
        break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void UpdateScroll(int totalHeight) {
    RECT rc; GetClientRect(g_hViewport, &rc);
    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0; si.nMax = totalHeight; si.nPage = rc.bottom;
    SetScrollInfo(g_hViewport, SB_VERT, &si, TRUE);

    // Show/Hide scrollbar based on content height
    ShowScrollBar(g_hViewport, SB_VERT, totalHeight > rc.bottom);

    int pos = GetScrollPos(g_hViewport, SB_VERT);
    if (pos > si.nMax - (int)si.nPage) {
        int newPos = max(0, si.nMax - (int)si.nPage);
        SetScrollPos(g_hViewport, SB_VERT, newPos, TRUE);
        SetWindowPos(g_hPageContent, NULL, 0, -newPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void LoadCustomCommands() {
    SendMessage(g_hComboCustom, CB_RESETCONTENT, 0, 0);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD subKeys;
        RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        for (DWORD i = 0; i < subKeys; i++) {
            WCHAR name[256];
            DWORD nameSize = 256;
            if (RegEnumKeyExW(hKey, i, name, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                SendMessage(g_hComboCustom, CB_ADDSTRING, 0, (LPARAM)name);
            }
        }
        RegCloseKey(hKey);
    }
    SendMessage(g_hComboCustom, CB_INSERTSTRING, 0, (LPARAM)L"-- New Command --");
    SendMessage(g_hComboCustom, CB_SETCURSEL, 0, 0);
}

void SelectCustomCommand() {
    int sel = (int)SendMessage(g_hComboCustom, CB_GETCURSEL, 0, 0);
    if (sel <= 0) { // -- New Command --
        SetWindowTextW(g_hEditName, L"");
        SetWindowTextW(g_hEditPath, L"");
        SetWindowTextW(g_hEditArgs, L"");
        SetWindowTextW(g_hEditIconLight, L"");
        SetWindowTextW(g_hEditIconDark, L"");
        SendMessage(g_hChkFile, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkDir, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkBG, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkAdmin, BM_SETCHECK, BST_UNCHECKED, 0);
        return;
    }

    WCHAR name[256];
    SendMessage(g_hComboCustom, CB_GETLBTEXT, sel, (LPARAM)name);
    SetWindowTextW(g_hEditName, name);

    HKEY hKey;
    std::wstring subPath = std::wstring(REG_CUSTOM) + L"\\" + name;
    WCHAR path[MAX_PATH] = { 0 }, args[32768] = { 0 }, iconPath[MAX_PATH] = { 0 }, iconPathLight[MAX_PATH] = { 0 }, iconPathDark[MAX_PATH] = { 0 };
    DWORD pSize = sizeof(path), aSize = sizeof(args), iSize = sizeof(iconPath), iLightSize = sizeof(iconPathLight), iDarkSize = sizeof(iconPathDark);
    DWORD f = 1, d = 1, b = 1, admin = 0, dwSize = sizeof(DWORD);

    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"Path", RRF_RT_REG_SZ, NULL, path, &pSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"Args", RRF_RT_REG_SZ, NULL, args, &aSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"IconPath", RRF_RT_REG_SZ, NULL, iconPath, &iSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"IconPath_Light", RRF_RT_REG_SZ, NULL, iconPathLight, &iLightSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"IconPath_Dark", RRF_RT_REG_SZ, NULL, iconPathDark, &iDarkSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowFile", RRF_RT_REG_DWORD, NULL, &f, &dwSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowDir", RRF_RT_REG_DWORD, NULL, &d, &dwSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowBG", RRF_RT_REG_DWORD, NULL, &b, &dwSize);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"RunAsAdmin", RRF_RT_REG_DWORD, NULL, &admin, &dwSize);

    if (wcslen(iconPathLight) == 0 && wcslen(iconPath) > 0) wcscpy_s(iconPathLight, iconPath);
    if (wcslen(iconPathDark) == 0 && wcslen(iconPath) > 0) wcscpy_s(iconPathDark, iconPath);

    SetWindowTextW(g_hEditPath, path);
    SetWindowTextW(g_hEditArgs, args);
    SetWindowTextW(g_hEditIconLight, iconPathLight);
    SetWindowTextW(g_hEditIconDark, iconPathDark);
    SendMessage(g_hChkFile, BM_SETCHECK, f ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_hChkDir, BM_SETCHECK, d ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_hChkBG, BM_SETCHECK, b ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_hChkAdmin, BM_SETCHECK, admin ? BST_CHECKED : BST_UNCHECKED, 0);
}

void UpdateTabVisibility() {
    int sel = TabCtrl_GetCurSel(g_hTab);

    // Reset scroll
    SetScrollPos(g_hViewport, SB_VERT, 0, TRUE);
    SetWindowPos(g_hPageContent, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    auto ToggleGroup = [&](std::vector<SettingItem>& group, bool show) {
        for (auto& item : group) ShowWindow(item.hWnd, show ? SW_SHOW : SW_HIDE);
    };
    // Destroy custom entries dynamically created on previous views to load updated states
    auto ClearCustomCheckboxes = [&](std::vector<SettingItem>& group) {
        auto it = group.begin();
        while (it != group.end()) {
            if (it->isCustom) {
                if (it->hWnd) DestroyWindow(it->hWnd);
                it = group.erase(it);
            } else {
                ++it;
            }
        }
    };

    int maxY = 0;
    if (sel < 3) {
        ClearCustomCheckboxes(g_fileSettings);
        ClearCustomCheckboxes(g_dirSettings);
        ClearCustomCheckboxes(g_bgSettings);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_hTab, GWLP_HINSTANCE);
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD subKeys;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            int yFile = 10 + (int)(g_fileSettings.size() * 30);
            int yDir = 10 + (int)(g_dirSettings.size() * 30);
            int yBg = 10 + (int)(g_bgSettings.size() * 30);

            for (DWORD i = 0; i < subKeys; i++) {
                WCHAR name[256];
                DWORD nameSize = 256;
                if (RegEnumKeyExW(hKey, i, name, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    std::wstring subPath = std::wstring(REG_CUSTOM) + L"\\" + name;
                    DWORD f = 1, d = 1, b = 1, dwSize = sizeof(DWORD);
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowFile", RRF_RT_REG_DWORD, NULL, &f, &dwSize);
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowDir", RRF_RT_REG_DWORD, NULL, &d, &dwSize);
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"ShowBG", RRF_RT_REG_DWORD, NULL, &b, &dwSize);

                    DWORD enabledFile = 1, enabledDir = 1, enabledBg = 1;
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"Enabled_Files", RRF_RT_REG_DWORD, NULL, &enabledFile, &dwSize);
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"Enabled_Directory", RRF_RT_REG_DWORD, NULL, &enabledDir, &dwSize);
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"Enabled_Background", RRF_RT_REG_DWORD, NULL, &enabledBg, &dwSize);

                    if (sel == 0 && f) {
                        SettingItem item = { name, std::wstring(name), nullptr, true };
                        item.hWnd = CreateWindowW(L"BUTTON", name, WS_CHILD | BS_AUTOCHECKBOX, 20, yFile, 300, 25, g_hPageContent, NULL, hInst, NULL);
                        SendMessage(item.hWnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWnd, BM_SETCHECK, enabledFile ? BST_CHECKED : BST_UNCHECKED, 0);
                        g_fileSettings.push_back(item);
                        yFile += 30;
                    }
                    if (sel == 1 && d) {
                        SettingItem item = { name, std::wstring(name), nullptr, true };
                        item.hWnd = CreateWindowW(L"BUTTON", name, WS_CHILD | BS_AUTOCHECKBOX, 20, yDir, 300, 25, g_hPageContent, NULL, hInst, NULL);
                        SendMessage(item.hWnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWnd, BM_SETCHECK, enabledDir ? BST_CHECKED : BST_UNCHECKED, 0);
                        g_dirSettings.push_back(item);
                        yDir += 30;
                    }
                    if (sel == 2 && b) {
                        SettingItem item = { name, std::wstring(name), nullptr, true };
                        item.hWnd = CreateWindowW(L"BUTTON", name, WS_CHILD | BS_AUTOCHECKBOX, 20, yBg, 300, 25, g_hPageContent, NULL, hInst, NULL);
                        SendMessage(item.hWnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWnd, BM_SETCHECK, enabledBg ? BST_CHECKED : BST_UNCHECKED, 0);
                        g_bgSettings.push_back(item);
                        yBg += 30;
                    }
                }
            }
            RegCloseKey(hKey);
            if (sel == 0) maxY = yFile;
            else if (sel == 1) maxY = yDir;
            else if (sel == 2) maxY = yBg;
        }
        DarkModeManager::ApplyToControls(g_hPageContent);
    } else {
        maxY = 340;
    }
    UpdateScroll(maxY);

    ToggleGroup(g_fileSettings, sel == 0);
    ToggleGroup(g_dirSettings, sel == 1);
    ToggleGroup(g_bgSettings, sel == 2);
    BOOL bCustom = (sel == 3);
    ShowWindow(g_hComboCustom, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditName, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditPath, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditArgs, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditIconLight, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditIconDark, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnAdd, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnEdit, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDel, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBrowse, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBrowseIconLight, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBrowseIconDark, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticSelect, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticName, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticPath, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticArgs, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticIconLight, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticIconDark, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkFile, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkDir, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkBG, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkAdmin, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBackup, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnRestore, bCustom ? SW_SHOW : SW_HIDE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        ApplyNativeDarkMode(hwnd);
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        g_hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED, 10, 10, 380, 410, hwnd, NULL, hInst, NULL);
        DarkModeManager::FixTabControl(g_hTab);
        SendMessage(g_hTab, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hViewport = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 12, 42, 376, 375, hwnd, NULL, hInst, NULL);
        SetWindowSubclass(g_hViewport, ViewportProc, 0, 0);
        if (DarkModeManager::IsDarkMode()) SetWindowTheme(g_hViewport, L"DarkMode_Explorer", NULL);

        g_hPageContent = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 376, 1000, g_hViewport, NULL, hInst, NULL);
        SetWindowSubclass(g_hPageContent, PageContentProc, 0, 0);

        TCITEMW tie = { TCIF_TEXT };
        tie.pszText = (LPWSTR)L"Files"; TabCtrl_InsertItem(g_hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"Directory"; TabCtrl_InsertItem(g_hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"Directory Background"; TabCtrl_InsertItem(g_hTab, 2, &tie);
        tie.pszText = (LPWSTR)L"Custom"; TabCtrl_InsertItem(g_hTab, 3, &tie);

        auto CreateCheckboxes = [&](std::vector<SettingItem>& group) {
            int y = 10;
            for (auto& item : group) {
                item.hWnd = CreateWindowW(L"BUTTON", item.label.c_str(), WS_CHILD | BS_AUTOCHECKBOX, 20, y, 300, 25, g_hPageContent, NULL, hInst, NULL);
                SendMessage(item.hWnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SendMessage(item.hWnd, BM_SETCHECK, GetSetting(item.regValue.c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
                y += 30;
            }
        };
        CreateCheckboxes(g_fileSettings);
        CreateCheckboxes(g_dirSettings);
        CreateCheckboxes(g_bgSettings);

        int y = 10;
        int labelX = 15, inputX = 90, fieldW = 250;

        g_hStaticSelect = CreateWindowW(L"STATIC", L"Select Entry:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hComboCustom = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, inputX, y, fieldW, 200, g_hPageContent, (HMENU)200, hInst, NULL);
        y += 32;
        g_hStaticName = CreateWindowW(L"STATIC", L"Name:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hEditName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, inputX, y, fieldW, 25, g_hPageContent, NULL, hInst, NULL);
        y += 28;
        g_hStaticPath = CreateWindowW(L"STATIC", L"Path:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, inputX, y, fieldW - 40, 25, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD, inputX + fieldW - 35, y, 35, 25, g_hPageContent, (HMENU)102, hInst, NULL);
        y += 28;
        g_hStaticArgs = CreateWindowW(L"STATIC", L"Args:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hEditArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, inputX, y, fieldW, 25, g_hPageContent, NULL, hInst, NULL);
        y += 28;
        g_hStaticIconLight = CreateWindowW(L"STATIC", L"Light Icon:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hEditIconLight = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, inputX, y, fieldW - 40, 25, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowseIconLight = CreateWindowW(L"BUTTON", L"...", WS_CHILD, inputX + fieldW - 35, y, 35, 25, g_hPageContent, (HMENU)104, hInst, NULL);
        y += 28;
        g_hStaticIconDark = CreateWindowW(L"STATIC", L"Dark Icon:", WS_CHILD, labelX, y + 3, 75, 20, g_hPageContent, NULL, hInst, NULL);
        g_hEditIconDark = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, inputX, y, fieldW - 40, 25, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowseIconDark = CreateWindowW(L"BUTTON", L"...", WS_CHILD, inputX + fieldW - 35, y, 35, 25, g_hPageContent, (HMENU)107, hInst, NULL);
        y += 28;
        g_hChkFile = CreateWindowW(L"BUTTON", L"File", WS_CHILD | BS_AUTOCHECKBOX, inputX, y, 55, 25, g_hPageContent, NULL, hInst, NULL);
        g_hChkDir = CreateWindowW(L"BUTTON", L"Directory", WS_CHILD | BS_AUTOCHECKBOX, inputX + 65, y, 85, 25, g_hPageContent, NULL, hInst, NULL);
        g_hChkBG = CreateWindowW(L"BUTTON", L"Background", WS_CHILD | BS_AUTOCHECKBOX, inputX + 155, y, 95, 25, g_hPageContent, NULL, hInst, NULL);
        y += 22;
        g_hChkAdmin = CreateWindowW(L"BUTTON", L"Run as administrator", WS_CHILD | BS_AUTOCHECKBOX, inputX, y, 200, 25, g_hPageContent, NULL, hInst, NULL);

        y += 35;
        int btnW = 100;
        g_hBtnAdd = CreateWindowW(L"BUTTON", L"Add", WS_CHILD, 25, y, btnW, 30, g_hPageContent, (HMENU)100, hInst, NULL);
        g_hBtnEdit = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD, 137, y, btnW, 30, g_hPageContent, (HMENU)103, hInst, NULL);
        g_hBtnDel = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD, 249, y, btnW, 30, g_hPageContent, (HMENU)101, hInst, NULL);

        y += 35;
        int longBtnW = 155;
        g_hBtnBackup = CreateWindowW(L"BUTTON", L"Backup Settings...", WS_CHILD, 25, y, longBtnW, 30, g_hPageContent, (HMENU)105, hInst, NULL);
        g_hBtnRestore = CreateWindowW(L"BUTTON", L"Restore Settings...", WS_CHILD, 194, y, longBtnW, 30, g_hPageContent, (HMENU)106, hInst, NULL);

        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            return TRUE;
        }, 0);
        LoadCustomCommands();
        UpdateTabVisibility();
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == 200 && HIWORD(wParam) == CBN_SELCHANGE) { SelectCustomCommand(); return 0; }
        if (HIWORD(wParam) == BN_CLICKED) {
            if (wmId == 100 || wmId == 103) { // Add or Edit
                WCHAR name[256], path[MAX_PATH], args[32768], iconPathLight[MAX_PATH], iconPathDark[MAX_PATH];
                GetWindowTextW(g_hEditName, name, 256);
                GetWindowTextW(g_hEditPath, path, MAX_PATH);
                GetWindowTextW(g_hEditArgs, args, 32768);
                GetWindowTextW(g_hEditIconLight, iconPathLight, MAX_PATH);
                GetWindowTextW(g_hEditIconDark, iconPathDark, MAX_PATH);
                DWORD f = (SendMessage(g_hChkFile, BM_GETCHECK, 0, 0) == BST_CHECKED), d = (SendMessage(g_hChkDir, BM_GETCHECK, 0, 0) == BST_CHECKED), b = (SendMessage(g_hChkBG, BM_GETCHECK, 0, 0) == BST_CHECKED);
                DWORD admin = (SendMessage(g_hChkAdmin, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (wcslen(name) > 0 && wcslen(path) > 0) {
                    HKEY hKey;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                        HKEY hSubKey;
                        if (RegCreateKeyExW(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hSubKey, NULL) == ERROR_SUCCESS) {
                            RegSetValueExW(hSubKey, L"Path", 0, REG_SZ, (BYTE*)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"Args", 0, REG_SZ, (BYTE*)args, (DWORD)(wcslen(args) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"IconPath", 0, REG_SZ, (BYTE*)iconPathLight, (DWORD)(wcslen(iconPathLight) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"IconPath_Light", 0, REG_SZ, (BYTE*)iconPathLight, (DWORD)(wcslen(iconPathLight) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"IconPath_Dark", 0, REG_SZ, (BYTE*)iconPathDark, (DWORD)(wcslen(iconPathDark) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"ShowFile", 0, REG_DWORD, (BYTE*)&f, sizeof(DWORD));
                            RegSetValueExW(hSubKey, L"ShowDir", 0, REG_DWORD, (BYTE*)&d, sizeof(DWORD));
                            RegSetValueExW(hSubKey, L"ShowBG", 0, REG_DWORD, (BYTE*)&b, sizeof(DWORD));
                            RegSetValueExW(hSubKey, L"RunAsAdmin", 0, REG_DWORD, (BYTE*)&admin, sizeof(DWORD));

                            // Maintain or write an Enabled flag when adding or editing commands
                            if (wmId == 100) { // Add
                                DWORD enabled = 1;
                                RegSetValueExW(hSubKey, L"Enabled_Files", 0, REG_DWORD, (BYTE*)&enabled, sizeof(DWORD));
                                RegSetValueExW(hSubKey, L"Enabled_Directory", 0, REG_DWORD, (BYTE*)&enabled, sizeof(DWORD));
                                RegSetValueExW(hSubKey, L"Enabled_Background", 0, REG_DWORD, (BYTE*)&enabled, sizeof(DWORD));
                            }
                            RegCloseKey(hSubKey);
                        }
                        RegCloseKey(hKey);
                    }
                    LoadCustomCommands();
                }
            } else if (wmId == 101) { // Delete
                WCHAR name[256]; GetWindowTextW(g_hEditName, name, 256);
                if (wcslen(name) > 0) {
                    HKEY hKey; if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) { RegDeleteKeyW(hKey, name); RegCloseKey(hKey); }
                    LoadCustomCommands();
                }
            } else if (wmId == 102) { // Browse
                OPENFILENAMEW ofn = { sizeof(ofn) }; WCHAR szFile[MAX_PATH] = { 0 };
                ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    SetWindowTextW(g_hEditPath, szFile);
                    WCHAR* pName = wcsrchr(szFile, L'\\'); if (pName) pName++; else pName = szFile;
                    std::wstring name = pName; size_t pos = name.find_last_of(L".");
                    if (pos != std::wstring::npos) name = name.substr(0, pos);
                    SetWindowTextW(g_hEditName, name.c_str());

                    // Also set icon paths if empty
                    WCHAR iconPathLight[MAX_PATH], iconPathDark[MAX_PATH];
                    GetWindowTextW(g_hEditIconLight, iconPathLight, MAX_PATH);
                    GetWindowTextW(g_hEditIconDark, iconPathDark, MAX_PATH);
                    if (wcslen(iconPathLight) == 0) SetWindowTextW(g_hEditIconLight, szFile);
                    if (wcslen(iconPathDark) == 0) SetWindowTextW(g_hEditIconDark, szFile);
                }
            } else if (wmId == 104 || wmId == 107) { // Browse Icon (Light or Dark)
                OPENFILENAMEW ofn = { sizeof(ofn) }; WCHAR szFile[MAX_PATH] = { 0 };
                ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Icons (EXE, DLL, ICO)\0*.exe;*.dll;*.ico\0All Files (*.*)\0*.*\0";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    UINT numIcons = ExtractIconExW(szFile, -1, NULL, NULL, 0);
                    HWND hTargetEdit = (wmId == 104) ? g_hEditIconLight : g_hEditIconDark;
                    if (numIcons > 1) {
                        int iconIndex = 0;
                        HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
                        typedef int (WINAPI* PFN_PickIconDlg)(HWND, LPWSTR, UINT, int*);
                        PFN_PickIconDlg pPickIconDlg = (PFN_PickIconDlg)GetProcAddress(hShell32, (LPCSTR)62);
                        if (pPickIconDlg && pPickIconDlg(hwnd, szFile, MAX_PATH, &iconIndex)) {
                            WCHAR finalIcon[MAX_PATH + 16];
                            swprintf_s(finalIcon, L"%s,%d", szFile, iconIndex);
                            SetWindowTextW(hTargetEdit, finalIcon);
                        } else {
                            SetWindowTextW(hTargetEdit, szFile);
                        }
                    } else {
                        SetWindowTextW(hTargetEdit, szFile);
                    }
                }
            } else if (wmId == 105) { // Backup
                OPENFILENAMEW ofn = { sizeof(ofn) };
                WCHAR szFile[MAX_PATH] = { 0 };
                wcscpy_s(szFile, L"xToolsMenu_Backup.reg");
                ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Registry Files (*.reg)\0*.reg\0";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                ofn.lpstrDefExt = L"reg";
                if (GetSaveFileNameW(&ofn)) {
                    std::wstring cmd = L"export \"HKEY_CURRENT_USER\\Software\\xToolsMenu\" \"" + std::wstring(szFile) + L"\" /y";
                    SHELLEXECUTEINFOW sei = { sizeof(sei) };
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.lpVerb = L"open";
                    sei.lpFile = L"reg.exe";
                    sei.lpParameters = cmd.c_str();
                    sei.nShow = SW_HIDE;
                    if (ShellExecuteExW(&sei)) {
                        if (sei.hProcess) {
                            WaitForSingleObject(sei.hProcess, INFINITE);
                            CloseHandle(sei.hProcess);
                        }
                        MessageBoxW(hwnd, L"Settings exported successfully!", L"Backup", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"Failed to export settings.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
            } else if (wmId == 106) { // Restore
                OPENFILENAMEW ofn = { sizeof(ofn) }; WCHAR szFile[MAX_PATH] = { 0 };
                ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Registry Files (*.reg)\0*.reg\0";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    std::wstring cmd = L"import \"" + std::wstring(szFile) + L"\"";
                    SHELLEXECUTEINFOW sei = { sizeof(sei) };
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.lpVerb = L"open";
                    sei.lpFile = L"reg.exe";
                    sei.lpParameters = cmd.c_str();
                    sei.nShow = SW_HIDE;
                    if (ShellExecuteExW(&sei)) {
                        if (sei.hProcess) {
                            WaitForSingleObject(sei.hProcess, INFINITE);
                            CloseHandle(sei.hProcess);
                        }
                        LoadCustomCommands();
                        MessageBoxW(hwnd, L"Settings imported successfully! Please reopen Settings to see all checkboxes update.", L"Restore", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"Failed to import settings.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
            } else {
                bool checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                int currentTab = TabCtrl_GetCurSel(g_hTab);
                auto UpdateGroup = [&](std::vector<SettingItem>& group) {
                    for (auto& item : group) {
                        if (item.hWnd == (HWND)lParam) {
                            if (item.isCustom) {
                                HKEY hSubKey;
                                std::wstring subPath = std::wstring(REG_CUSTOM) + L"\\" + item.regValue;
                                if (RegOpenKeyExW(HKEY_CURRENT_USER, subPath.c_str(), 0, KEY_SET_VALUE, &hSubKey) == ERROR_SUCCESS) {
                                    DWORD val = checked ? 1 : 0;
                                    const wchar_t* valName = L"Enabled_Files";
                                    if (currentTab == 1) valName = L"Enabled_Directory";
                                    else if (currentTab == 2) valName = L"Enabled_Background";
                                    RegSetValueExW(hSubKey, valName, 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
                                    RegCloseKey(hSubKey);
                                }
                            } else {
                                SetSetting(item.regValue.c_str(), checked);
                            }
                            break;
                        }
                    }
                };
                if (currentTab == 0) UpdateGroup(g_fileSettings);
                else if (currentTab == 1) UpdateGroup(g_dirSettings);
                else if (currentTab == 2) UpdateGroup(g_bgSettings);
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
    {
        if (DarkModeManager::IsDarkMode())
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, DarkModeManager::GetBackgroundBrush());
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
        if (lpDrawItem->hwndItem == g_hTab)
        {
            WCHAR szText[256];
            TCITEMW tci = { TCIF_TEXT };
            tci.pszText = szText;
            tci.cchTextMax = 256;
            TabCtrl_GetItem(g_hTab, lpDrawItem->itemID, &tci);

            HDC hdc = lpDrawItem->hDC;
            RECT rc = lpDrawItem->rcItem;

            bool isDarkMode = DarkModeManager::IsDarkMode();
            HBRUSH hbr = isDarkMode ? DarkModeManager::GetBackgroundBrush() : GetSysColorBrush(COLOR_BTNFACE);
            FillRect(hdc, &rc, hbr);

            SetTextColor(hdc, isDarkMode ? DarkModeManager::GetTextColor() : GetSysColor(COLOR_BTNTEXT));
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, szText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            if (lpDrawItem->itemState & ODS_SELECTED)
            {
                // Draw a simple underline or highlight for selected tab
                RECT rcHighlight = rc;
                rcHighlight.top = rcHighlight.bottom - 3;
                HBRUSH hbrSel = CreateSolidBrush(isDarkMode ? RGB(0, 120, 215) : GetSysColor(COLOR_HIGHLIGHT));
                FillRect(hdc, &rcHighlight, hbrSel);
                DeleteObject(hbrSel);
            }
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == TCN_SELCHANGE) { UpdateTabVisibility(); InvalidateRect(hwnd, NULL, TRUE); }
        return 0;
    }
    case WM_SETTINGCHANGE:
    {
        DarkModeManager::OnSettingChange(hwnd, lParam);
        return 0;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    {
        if (DarkModeManager::IsDarkMode())
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DarkModeManager::GetTextColor());
            SetBkColor(hdc, DarkModeManager::GetBackgroundColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)DarkModeManager::GetBackgroundBrush();
        }
        break;
    }
    case WM_DESTROY:
        DarkModeManager::Cleanup();
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Single Instance Guard via a Named Mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\xToolsMenuSettingsSingleInstanceMutex");
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        HWND hwndExisting = FindWindowW(L"SettingsDialogClass", L"xToolsMenu Settings");
        if (hwndExisting) {
            ShowWindow(hwndExisting, SW_RESTORE);
            SetForegroundWindow(hwndExisting);
        }
        return 0;
    }

    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.Settings");
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES }; InitCommonControlsEx(&icex);
    const wchar_t CLASS_NAME[] = L"SettingsDialogClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, hInstance, hIcon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, CLASS_NAME, hIcon };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"xToolsMenu Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 420, 490, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;
    RECT rect; GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Only run IsDialogMessageW if the current focused window is NOT an Edit control.
        // IsDialogMessageW swallows keystrokes like Ctrl+A inside standard Win32 Edit fields.
        HWND hFocus = GetFocus();
        WCHAR className[64];
        bool isEdit = false;
        if (hFocus && GetClassNameW(hFocus, className, 64)) {
            if (_wcsicmp(className, L"EDIT") == 0) {
                isEdit = true;
            }
        }

        // Process Ctrl+A manually for standard Edit controls since Win32 EDIT doesn't do it natively
        if (isEdit && msg.message == WM_KEYDOWN && GetAsyncKeyState(VK_CONTROL) < 0 && msg.wParam == 'A') {
            SendMessageW(hFocus, EM_SETSEL, 0, -1);
            continue;
        }

        if (isEdit) {
            // Tab, Arrows, etc. navigation keys inside the window when an edit is focused
            if (msg.message == WM_KEYDOWN && (msg.wParam == VK_TAB || msg.wParam == VK_ESCAPE || msg.wParam == VK_RETURN)) {
                if (IsDialogMessageW(hwnd, &msg)) continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            if (!IsDialogMessageW(hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    CloseHandle(hMutex);
    return 0;
}
