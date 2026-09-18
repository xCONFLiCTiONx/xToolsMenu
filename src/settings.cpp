#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <map>
#include "resource.h"
#include "DarkMode.h"
#include "FileTypeHelper.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

HWND g_hSidebar = nullptr;
HWND g_hPageContent = nullptr;
HFONT g_hFont = nullptr;
HFONT g_hSidebarFont = nullptr;
HWND g_hHeaderCommand = nullptr, g_hHeaderNew = nullptr, g_hHeaderOld = nullptr;

const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";
const wchar_t* REG_CUSTOM = L"Software\\xToolsMenu\\CustomCommands";

struct SettingItem {
    std::wstring label;
    std::wstring regValue;
    HWND hWndLabel = nullptr;
    HWND hWndNew = nullptr;
    HWND hWndOld = nullptr;
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
HWND g_hComboCustom = nullptr;
HWND g_hEditName = nullptr, g_hEditPath = nullptr, g_hEditArgs = nullptr, g_hEditIconLight = nullptr, g_hEditIconDark = nullptr;
HWND g_hBtnAdd = nullptr, g_hBtnEdit = nullptr, g_hBtnDel = nullptr, g_hBtnBrowse = nullptr, g_hBtnBrowseIconLight = nullptr, g_hBtnBrowseIconDark = nullptr;
HWND g_hStaticSelect = nullptr, g_hStaticName = nullptr, g_hStaticPath = nullptr, g_hStaticArgs = nullptr, g_hStaticIconLight = nullptr, g_hStaticIconDark = nullptr;
HWND g_hChkFile = nullptr, g_hChkDir = nullptr, g_hChkBG = nullptr, g_hChkAdmin = nullptr;
std::map<FileTypeCategory, HWND> g_hFileTypeChecks;
HWND g_hBtnBackup = nullptr, g_hBtnRestore = nullptr;

bool GetSetting(const wchar_t* name) {
    DWORD defaultValue = (wcsstr(name, L"_Old") != nullptr) ? 0 : 1;
    DWORD value = defaultValue, size = sizeof(value);
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

LRESULT CALLBACK ForwardMouseWheelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

bool ElevateSelf() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)) == 0) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // Triggers UAC prompt
    sei.lpFile = szPath;   // Restarts current executable
    sei.nShow = SW_NORMAL;

    return ShellExecuteExW(&sei);
}

LRESULT CALLBACK SidebarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_NCPAINT: {
        HDC hdc = GetWindowDC(hWnd);
        if (hdc) {
            RECT rc;
            GetWindowRect(hWnd, &rc);
            OffsetRect(&rc, -rc.left, -rc.top);
            // Gray border for the sidebar
            HBRUSH hbr = CreateSolidBrush(DarkModeManager::IsDarkMode() ? RGB(80, 80, 80) : RGB(160, 160, 160));
            FrameRect(hdc, &rc, hbr);
            DeleteObject(hbr);
            ReleaseDC(hWnd, hdc);
        }
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, SidebarSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK PageContentProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
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
        for (auto& pair : g_hFileTypeChecks) SendMessage(pair.second, BM_SETCHECK, BST_CHECKED, 0);
        return;
    }

    WCHAR name[256];
    SendMessage(g_hComboCustom, CB_GETLBTEXT, sel, (LPARAM)name);
    SetWindowTextW(g_hEditName, name);

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

    DWORD allowedFileTypes = 0xFFFF;
    dwSize = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), L"AllowedFileTypes", RRF_RT_REG_DWORD, NULL, &allowedFileTypes, &dwSize);
    for (auto& pair : g_hFileTypeChecks) {
        bool allowed = (allowedFileTypes & (DWORD)pair.first);
        SendMessage(pair.second, BM_SETCHECK, allowed ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void UpdateSidebarVisibility() {
    int sel = (int)SendMessage(g_hSidebar, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;

    auto ToggleGroup = [&](std::vector<SettingItem>& group, bool show) {
        for (auto& item : group) {
            if (item.hWndLabel) ShowWindow(item.hWndLabel, show ? SW_SHOW : SW_HIDE);
            if (item.hWndNew) {
                if (item.label == L"System Folders" || item.label == L"Edit With") ShowWindow(item.hWndNew, SW_HIDE);
                else ShowWindow(item.hWndNew, show ? SW_SHOW : SW_HIDE);
            }
            if (item.hWndOld) ShowWindow(item.hWndOld, show ? SW_SHOW : SW_HIDE);
        }
    };
    auto ClearCustomCheckboxes = [&](std::vector<SettingItem>& group) {
        auto it = group.begin();
        while (it != group.end()) {
            if (it->isCustom) {
                if (it->hWndLabel) DestroyWindow(it->hWndLabel);
                if (it->hWndNew) DestroyWindow(it->hWndNew);
                if (it->hWndOld) DestroyWindow(it->hWndOld);
                it = group.erase(it);
            } else {
                ++it;
            }
        }
    };

    int maxY = 0;
    int col1X = 20, col2X = 320;
    int yStart = 40;

    if (sel < 3) {
        ClearCustomCheckboxes(g_fileSettings);
        ClearCustomCheckboxes(g_dirSettings);
        ClearCustomCheckboxes(g_bgSettings);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_hSidebar, GWLP_HINSTANCE);
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD subKeys;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

            auto& currentGroup = (sel == 0) ? g_fileSettings : (sel == 1 ? g_dirSettings : g_bgSettings);
            std::wstring enabledVal = (sel == 0) ? L"Enabled_Files" : (sel == 1 ? L"Enabled_Directory" : L"Enabled_Background");

            for (DWORD i = 0; i < subKeys; i++) {
                WCHAR name[256];
                DWORD nameSize = 256;
                if (RegEnumKeyExW(hKey, i, name, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    std::wstring subPath = std::wstring(REG_CUSTOM) + L"\\" + name;
                    DWORD show = 1, dwSize = sizeof(DWORD);
                    std::wstring showVal = (sel == 0) ? L"ShowFile" : (sel == 1 ? L"ShowDir" : L"ShowBG");
                    RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), showVal.c_str(), RRF_RT_REG_DWORD, NULL, &show, &dwSize);

                    if (show) {
                        SettingItem item = { name, std::wstring(name), nullptr, nullptr, nullptr, true };
                        item.hWndLabel = CreateWindowW(L"STATIC", name, WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
                        item.hWndNew = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
                        item.hWndOld = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);

                        DWORD enabledNew = 1, enabledOld = 0;
                        dwSize = sizeof(DWORD);
                        RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), enabledVal.c_str(), RRF_RT_REG_DWORD, NULL, &enabledNew, &dwSize);
                        RegGetValueW(HKEY_CURRENT_USER, subPath.c_str(), (enabledVal + L"_Old").c_str(), RRF_RT_REG_DWORD, NULL, &enabledOld, &dwSize);

                        SendMessage(item.hWndLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWndNew, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWndOld, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                        SendMessage(item.hWndNew, BM_SETCHECK, enabledNew ? BST_CHECKED : BST_UNCHECKED, 0);
                        SendMessage(item.hWndOld, BM_SETCHECK, enabledOld ? BST_CHECKED : BST_UNCHECKED, 0);
                        currentGroup.push_back(item);
                    }
                }
            }
            RegCloseKey(hKey);

            // Re-layout standard and custom items in 2 columns
            auto LayoutGroup = [&](std::vector<SettingItem>& group) {
                int y = yStart;
                for (size_t i = 0; i < group.size(); ++i) {
                    int x = (i % 2 == 0) ? col1X : col2X;
                    SetWindowPos(group[i].hWndLabel, NULL, x, y + 3, 200, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
                    if (group[i].label == L"System Folders" || group[i].label == L"Edit With") {
                        ShowWindow(group[i].hWndNew, SW_HIDE);
                    } else {
                        SetWindowPos(group[i].hWndNew, NULL, x + 215, y, 25, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
                    }
                    SetWindowPos(group[i].hWndOld, NULL, x + 265, y, 25, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
                    if (i % 2 == 1 || i == group.size() - 1) y += 35;
                }
                return y;
            };

            maxY = LayoutGroup(currentGroup);

            // Update header positions
            SetWindowPos(g_hHeaderCommand, NULL, col1X, 10, 200, 20, SWP_NOZORDER);
            SetWindowPos(g_hHeaderNew, NULL, col1X + 215, 10, 60, 20, SWP_NOZORDER);
            SetWindowPos(g_hHeaderOld, NULL, col1X + 265, 10, 60, 20, SWP_NOZORDER);
        }
        DarkModeManager::ApplyToControls(g_hPageContent);
    } else {
        // Layout Custom tab side-by-side
        int leftX = 20, rightX = 350;
        int y = 20;
        SetWindowPos(g_hStaticSelect, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hComboCustom, NULL, leftX + 110, y, 200, 200, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 40;
        SetWindowPos(g_hStaticName, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hEditName, NULL, leftX + 110, y, 200, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 35;
        SetWindowPos(g_hStaticPath, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hEditPath, NULL, leftX + 110, y, 165, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnBrowse, NULL, leftX + 275, y, 35, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 35;
        SetWindowPos(g_hStaticArgs, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hEditArgs, NULL, leftX + 110, y, 200, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 35;
        SetWindowPos(g_hStaticIconLight, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hEditIconLight, NULL, leftX + 110, y, 165, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnBrowseIconLight, NULL, leftX + 275, y, 35, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 35;
        SetWindowPos(g_hStaticIconDark, NULL, leftX, y + 3, 100, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hEditIconDark, NULL, leftX + 110, y, 165, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnBrowseIconDark, NULL, leftX + 275, y, 35, 25, SWP_NOZORDER | SWP_SHOWWINDOW);

        // Right side: File types
        y = 20;
        HWND hStaticTypes = FindWindowExW(g_hPageContent, NULL, L"STATIC", L"File Types (only for 'File' target):");
        SetWindowPos(hStaticTypes, NULL, rightX, y, 280, 20, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 30;
        int typeY = y;
        int i = 0;
        for (auto& pair : g_hFileTypeChecks) {
            SetWindowPos(pair.second, NULL, rightX + (i % 2) * 140, typeY + (i / 2) * 25, 130, 22, SWP_NOZORDER | SWP_SHOWWINDOW);
            i++;
        }
        y += 130;

        // Target checkboxes on the right below file types
        SetWindowPos(g_hChkFile, NULL, rightX, y, 60, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hChkDir, NULL, rightX + 70, y, 90, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hChkBG, NULL, rightX + 170, y, 100, 25, SWP_NOZORDER | SWP_SHOWWINDOW);
        y += 30;
        SetWindowPos(g_hChkAdmin, NULL, rightX, y, 200, 25, SWP_NOZORDER | SWP_SHOWWINDOW);

        // Buttons at the bottom
        int btnY = 500;
        SetWindowPos(g_hBtnAdd, NULL, leftX, btnY, 90, 30, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnEdit, NULL, leftX + 95, btnY, 90, 30, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnDel, NULL, leftX + 190, btnY, 90, 30, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnBackup, NULL, rightX, btnY, 135, 30, SWP_NOZORDER | SWP_SHOWWINDOW);
        SetWindowPos(g_hBtnRestore, NULL, rightX + 140, btnY, 135, 30, SWP_NOZORDER | SWP_SHOWWINDOW);

        DarkModeManager::ApplyToControls(g_hPageContent);
    }

    ShowWindow(g_hHeaderCommand, sel < 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hHeaderNew, sel < 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hHeaderOld, sel < 3 ? SW_SHOW : SW_HIDE);

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
    for (auto& pair : g_hFileTypeChecks) ShowWindow(pair.second, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBackup, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnRestore, bCustom ? SW_SHOW : SW_HIDE);

    // Also need to hide the "File Types" static label
    HWND hStaticTypes = FindWindowExW(g_hPageContent, NULL, L"STATIC", L"File Types (only for 'File' target):");
    if (hStaticTypes) ShowWindow(hStaticTypes, bCustom ? SW_SHOW : SW_HIDE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        ApplyNativeDarkMode(hwnd);
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        int logHeightSidebar = -MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_hSidebarFont = CreateFontW(logHeightSidebar, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        g_hSidebar = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 10, 10, 180, 540, hwnd, (HMENU)500, hInst, NULL);
        SetWindowSubclass(g_hSidebar, SidebarSubclassProc, 0, 0);
        SendMessage(g_hSidebar, LB_ADDSTRING, 0, (LPARAM)L"Files");
        SendMessage(g_hSidebar, LB_ADDSTRING, 0, (LPARAM)L"Directory");
        SendMessage(g_hSidebar, LB_ADDSTRING, 0, (LPARAM)L"Background");
        SendMessage(g_hSidebar, LB_ADDSTRING, 0, (LPARAM)L"Custom");
        SendMessage(g_hSidebar, LB_SETITEMHEIGHT, 0, 40);
        SendMessage(g_hSidebar, LB_SETCURSEL, 0, 0);
        SendMessage(g_hSidebar, WM_SETFONT, (WPARAM)g_hSidebarFont, TRUE);

        g_hPageContent = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 200, 10, 630, 540, hwnd, NULL, hInst, NULL);
        SetWindowSubclass(g_hPageContent, PageContentProc, 0, 0);

        g_hHeaderCommand = CreateWindowW(L"STATIC", L"Command", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hHeaderNew = CreateWindowW(L"STATIC", L"New", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hHeaderOld = CreateWindowW(L"STATIC", L"Classic", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        SendMessage(g_hHeaderCommand, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(g_hHeaderNew, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(g_hHeaderOld, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        auto CreateCheckboxes = [&](std::vector<SettingItem>& group) {
            for (auto& item : group) {
                item.hWndLabel = CreateWindowW(L"STATIC", item.label.c_str(), WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
                item.hWndNew = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
                item.hWndOld = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);

                SendMessage(item.hWndLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SendMessage(item.hWndNew, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SendMessage(item.hWndOld, WM_SETFONT, (WPARAM)g_hFont, TRUE);

                SendMessage(item.hWndNew, BM_SETCHECK, GetSetting(item.regValue.c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(item.hWndOld, BM_SETCHECK, GetSetting((item.regValue + L"_Old").c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
            }
        };
        CreateCheckboxes(g_fileSettings);
        CreateCheckboxes(g_dirSettings);
        CreateCheckboxes(g_bgSettings);

        g_hStaticSelect = CreateWindowW(L"STATIC", L"Select Entry:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hComboCustom = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, g_hPageContent, (HMENU)200, hInst, NULL);
        g_hStaticName = CreateWindowW(L"STATIC", L"Name:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hEditName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hStaticPath = CreateWindowW(L"STATIC", L"Path:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)102, hInst, NULL);
        g_hStaticArgs = CreateWindowW(L"STATIC", L"Args:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hEditArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hStaticIconLight = CreateWindowW(L"STATIC", L"Light Icon:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hEditIconLight = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowseIconLight = CreateWindowW(L"BUTTON", L"...", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)104, hInst, NULL);
        g_hStaticIconDark = CreateWindowW(L"STATIC", L"Dark Icon:", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hEditIconDark = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hBtnBrowseIconDark = CreateWindowW(L"BUTTON", L"...", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)107, hInst, NULL);
        g_hChkFile = CreateWindowW(L"BUTTON", L"File", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hChkDir = CreateWindowW(L"BUTTON", L"Directory", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hChkBG = CreateWindowW(L"BUTTON", L"Background", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        g_hChkAdmin = CreateWindowW(L"BUTTON", L"Run as administrator", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);

        CreateWindowW(L"STATIC", L"File Types (only for 'File' target):", WS_CHILD, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
        auto categories = FileTypeHelper::GetAllCategories();
        for (size_t i = 0; i < categories.size(); i++) {
            std::wstring catName = FileTypeHelper::GetCategoryName(categories[i]);
            HWND hChk = CreateWindowW(L"BUTTON", catName.c_str(), WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, g_hPageContent, NULL, hInst, NULL);
            g_hFileTypeChecks[categories[i]] = hChk;
        }

        g_hBtnAdd = CreateWindowW(L"BUTTON", L"Add", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)100, hInst, NULL);
        g_hBtnEdit = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)103, hInst, NULL);
        g_hBtnDel = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)101, hInst, NULL);
        g_hBtnBackup = CreateWindowW(L"BUTTON", L"Backup Settings...", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)105, hInst, NULL);
        g_hBtnRestore = CreateWindowW(L"BUTTON", L"Restore Settings...", WS_CHILD, 0, 0, 0, 0, g_hPageContent, (HMENU)106, hInst, NULL);

        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            if (hChild != g_hSidebar && hChild != g_hPageContent) {
                SetWindowSubclass(hChild, ForwardMouseWheelProc, 1, 0);
            }
            return TRUE;
        }, 0);
        LoadCustomCommands();
        UpdateSidebarVisibility();
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == 500 && HIWORD(wParam) == LBN_SELCHANGE) { UpdateSidebarVisibility(); InvalidateRect(hwnd, NULL, TRUE); return 0; }
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

                            DWORD allowedFileTypes = 0;
                            for (auto& pair : g_hFileTypeChecks) {
                                if (SendMessage(pair.second, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                                    allowedFileTypes |= (DWORD)pair.first;
                                }
                            }
                            RegSetValueExW(hSubKey, L"AllowedFileTypes", 0, REG_DWORD, (BYTE*)&allowedFileTypes, sizeof(DWORD));

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
                HWND hTargetEdit = (wmId == 104) ? g_hEditIconLight : g_hEditIconDark;
                WCHAR szFile[MAX_PATH] = { 0 };
                int iconIndex = 0;

                GetWindowTextW(hTargetEdit, szFile, MAX_PATH);
                if (wcslen(szFile) > 0) {
                    WCHAR* pComma = wcsrchr(szFile, L',');
                    if (pComma) {
                        *pComma = L'\0';
                        iconIndex = _wtoi(pComma + 1);
                    }
                } else {
                    ExpandEnvironmentStringsW(L"%SystemRoot%\\System32\\imageres.dll", szFile, MAX_PATH);
                    iconIndex = 0;
                }

                HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
                typedef int (WINAPI* PFN_PickIconDlg)(HWND, LPWSTR, UINT, int*);
                PFN_PickIconDlg pPickIconDlg = (PFN_PickIconDlg)GetProcAddress(hShell32, (LPCSTR)62);

                if (pPickIconDlg && pPickIconDlg(hwnd, szFile, MAX_PATH, &iconIndex)) {
                    WCHAR finalIcon[MAX_PATH + 16];
                    swprintf_s(finalIcon, L"%s,%d", szFile, iconIndex);
                    SetWindowTextW(hTargetEdit, finalIcon);
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
                    } else {
                        MessageBoxW(hwnd, L"Failed to import settings.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
            } else {
                bool checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                int currentTab = (int)SendMessage(g_hSidebar, LB_GETCURSEL, 0, 0);
                auto UpdateGroup = [&](std::vector<SettingItem>& group) {
                    for (auto& item : group) {
                        bool isNew = (item.hWndNew == (HWND)lParam);
                        bool isOld = (item.hWndOld == (HWND)lParam);
                        if (isNew || isOld) {
                            if (item.isCustom) {
                                HKEY hSubKey;
                                std::wstring subPath = std::wstring(REG_CUSTOM) + L"\\" + item.regValue;
                                if (RegOpenKeyExW(HKEY_CURRENT_USER, subPath.c_str(), 0, KEY_SET_VALUE, &hSubKey) == ERROR_SUCCESS) {
                                    DWORD val = checked ? 1 : 0;
                                    std::wstring valName;
                                    if (currentTab == 0) valName = L"Enabled_Files";
                                    else if (currentTab == 1) valName = L"Enabled_Directory";
                                    else if (currentTab == 2) valName = L"Enabled_Background";

                                    if (isOld) valName += L"_Old";
                                    RegSetValueExW(hSubKey, valName.c_str(), 0, REG_DWORD, (BYTE*)&val, sizeof(DWORD));
                                    RegCloseKey(hSubKey);
                                }
                            } else {
                                std::wstring regVal = item.regValue;
                                if (isOld) regVal += L"_Old";
                                SetSetting(regVal.c_str(), checked);
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
        if (lpDrawItem->hwndItem == g_hSidebar)
        {
            WCHAR szText[256];
            SendMessage(g_hSidebar, LB_GETTEXT, lpDrawItem->itemID, (LPARAM)szText);

            HDC hdc = lpDrawItem->hDC;
            RECT rc = lpDrawItem->rcItem;

            bool isDarkMode = DarkModeManager::IsDarkMode();
            bool isSelected = (lpDrawItem->itemState & ODS_SELECTED);

            HBRUSH hbr = isDarkMode ? DarkModeManager::GetBackgroundBrush() : GetSysColorBrush(COLOR_WINDOW);
            if (isSelected) hbr = CreateSolidBrush(isDarkMode ? RGB(45, 45, 45) : GetSysColor(COLOR_HIGHLIGHT));

            FillRect(hdc, &rc, hbr);
            if (isSelected && !isDarkMode) DeleteObject(hbr);

            SetTextColor(hdc, isDarkMode ? DarkModeManager::GetTextColor() : (isSelected ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT)));
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hSidebarFont);

            RECT textRc = rc;
            textRc.left += 15;
            DrawTextW(hdc, szText, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            if (isSelected)
            {
                RECT rcHighlight = rc;
                rcHighlight.right = rcHighlight.left + 4;
                HBRUSH hbrSel = CreateSolidBrush(RGB(0, 120, 215));
                FillRect(hdc, &rcHighlight, hbrSel);
                DeleteObject(hbrSel);
            }
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
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
    case WM_CTLCOLORLISTBOX:
    {
        if (DarkModeManager::IsDarkMode())
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, DarkModeManager::GetTextColor());
            SetBkColor(hdc, DarkModeManager::GetBackgroundColor());
            // Dropdowns need OPAQUE background to look right
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)DarkModeManager::GetBackgroundBrush();
        }
        break;
    }
    case WM_DESTROY:
        DarkModeManager::Cleanup();
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hSidebarFont) DeleteObject(g_hSidebarFont);
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
        HWND hwndExisting = FindWindowW(L"SettingsClass", L"xToolsMenu Settings");
        if (hwndExisting) {
            ShowWindow(hwndExisting, SW_RESTORE);
            SetForegroundWindow(hwndExisting);
        }
        return 0;
    }

    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.Settings");
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES }; InitCommonControlsEx(&icex);
    const wchar_t CLASS_NAME[] = L"SettingsClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, hInstance, hIcon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, CLASS_NAME, hIcon };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"xToolsMenu Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 850, 600, NULL, NULL, hInstance, NULL);
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
