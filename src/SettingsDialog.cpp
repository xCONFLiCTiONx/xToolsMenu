#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <map>
#include <uxtheme.h>
#include "resource.h"
#include "Theme.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

HWND g_hTab = nullptr;
HFONT g_hFont = nullptr;
HBRUSH g_hbrBackground = nullptr;
HBRUSH g_hbrControlBack = nullptr;

const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";
const wchar_t* REG_CUSTOM = L"Software\\xToolsMenu\\CustomCommands";

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
    { L"Enable Attributes", L"Background_Attributes" },
    { L"Enable Terminal", L"Background_Terminal" },
    { L"Enable Terminal (admin)", L"Background_TerminalAdmin" },
    { L"Enable System Folders", L"Background_SystemFolders" },
    { L"Enable Paste to File", L"Background_PasteToFile" }
};

HWND g_hListCustom = nullptr;
HWND g_hEditName = nullptr, g_hEditPath = nullptr, g_hEditArgs = nullptr;
HWND g_hBtnAdd = nullptr, g_hBtnDel = nullptr, g_hBtnBrowse = nullptr;
HWND g_hStaticName = nullptr, g_hStaticPath = nullptr, g_hStaticArgs = nullptr;
HWND g_hChkFile = nullptr, g_hChkDir = nullptr, g_hChkBG = nullptr;

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

void LoadCustomCommands() {
    ListView_DeleteAllItems(g_hListCustom);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD subKeys;
        RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        for (DWORD i = 0; i < subKeys; i++) {
            WCHAR name[256];
            DWORD nameSize = 256;
            if (RegEnumKeyExW(hKey, i, name, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                WCHAR path[MAX_PATH] = { 0 }, args[MAX_PATH] = { 0 };
                DWORD pSize = sizeof(path), aSize = sizeof(args);
                RegGetValueW(hKey, name, L"Path", RRF_RT_REG_SZ, NULL, path, &pSize);
                RegGetValueW(hKey, name, L"Args", RRF_RT_REG_SZ, NULL, args, &aSize);
                LVITEMW lvi = { LVIF_TEXT, (int)i };
                lvi.pszText = name;
                ListView_InsertItem(g_hListCustom, &lvi);
                ListView_SetItemText(g_hListCustom, (int)i, 1, path);
                ListView_SetItemText(g_hListCustom, (int)i, 2, args);
            }
        }
        RegCloseKey(hKey);
    }
}

void UpdateTabVisibility() {
    int sel = TabCtrl_GetCurSel(g_hTab);
    auto ToggleGroup = [&](std::vector<SettingItem>& group, bool show) {
        for (auto& item : group) ShowWindow(item.hWnd, show ? SW_SHOW : SW_HIDE);
    };
    ToggleGroup(g_fileSettings, sel == 0);
    ToggleGroup(g_dirSettings, sel == 1);
    ToggleGroup(g_bgSettings, sel == 2);
    BOOL bCustom = (sel == 3);
    ShowWindow(g_hListCustom, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditName, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditPath, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditArgs, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnAdd, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDel, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBrowse, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticName, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticPath, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hStaticArgs, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkFile, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkDir, bCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkBG, bCustom ? SW_SHOW : SW_HIDE);
}

LRESULT CALLBACK TabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        FillRect(hdc, &rect, g_hbrBackground);
        return 1;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, DARK_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)g_hbrBackground;
    }
    case WM_CTLCOLORDLG:
        return (LRESULT)g_hbrBackground;
    case WM_CTLCOLORBTN:
        return (LRESULT)g_hbrBackground;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, DARK_TEXT);
        SetBkColor(hdc, DARK_CONTROL_BACK);
        return (LRESULT)g_hbrControlBack;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, g_hbrBackground);
        return 1;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
        if (lpDrawItem->hwndItem == g_hTab) {
            DrawDarkTab(g_hTab, lpDrawItem);
            return TRUE;
        }
        if (lpDrawItem->CtlType == ODT_BUTTON) {
            DrawDarkButton(lpDrawItem);
            return TRUE;
        }
        break;
    }
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        g_hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | TCS_OWNERDRAWFIXED | WS_CLIPSIBLINGS, 10, 10, 380, 600, hwnd, NULL, hInst, NULL);
        SetWindowSubclass(g_hTab, TabSubclassProc, 0, 0);
        SendMessage(g_hTab, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        TCITEMW tie = { TCIF_TEXT };
        tie.pszText = (LPWSTR)L"Files"; TabCtrl_InsertItem(g_hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"Directory"; TabCtrl_InsertItem(g_hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"Directory Background"; TabCtrl_InsertItem(g_hTab, 2, &tie);
        tie.pszText = (LPWSTR)L"Custom"; TabCtrl_InsertItem(g_hTab, 3, &tie);

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

        g_hListCustom = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, 25, 50, 350, 200, hwnd, NULL, hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListCustom, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        ListView_SetBkColor(g_hListCustom, DARK_CONTROL_BACK);
        ListView_SetTextBkColor(g_hListCustom, DARK_CONTROL_BACK);
        ListView_SetTextColor(g_hListCustom, DARK_TEXT);
        SetWindowTheme(ListView_GetHeader(g_hListCustom), L"", L"");

        LVCOLUMNW lvc = { LVCF_TEXT | LVCF_WIDTH, 0, 90, (LPWSTR)L"Name" };
        ListView_InsertColumn(g_hListCustom, 0, &lvc);
        lvc.pszText = (LPWSTR)L"Path"; lvc.cx = 140; ListView_InsertColumn(g_hListCustom, 1, &lvc);
        lvc.pszText = (LPWSTR)L"Args"; lvc.cx = 80; ListView_InsertColumn(g_hListCustom, 2, &lvc);

        int y = 260;
        g_hStaticName = CreateWindowW(L"STATIC", L"Name:", WS_CHILD, 25, y, 50, 25, hwnd, NULL, hInst, NULL);
        g_hEditName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD, 80, y, 200, 25, hwnd, NULL, hInst, NULL);
        y += 35;
        g_hStaticPath = CreateWindowW(L"STATIC", L"Path:", WS_CHILD, 25, y, 50, 25, hwnd, NULL, hInst, NULL);
        g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD, 80, y, 240, 25, hwnd, NULL, hInst, NULL);
        g_hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | BS_OWNERDRAW, 325, y, 35, 25, hwnd, (HMENU)102, hInst, NULL);
        y += 35;
        g_hStaticArgs = CreateWindowW(L"STATIC", L"Args:", WS_CHILD, 25, y, 50, 25, hwnd, NULL, hInst, NULL);
        g_hEditArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD, 80, y, 280, 25, hwnd, NULL, hInst, NULL);
        y += 35;
        g_hChkFile = CreateWindowW(L"BUTTON", L"File", WS_CHILD | BS_AUTOCHECKBOX, 80, y, 60, 25, hwnd, NULL, hInst, NULL);
        g_hChkDir = CreateWindowW(L"BUTTON", L"Directory", WS_CHILD | BS_AUTOCHECKBOX, 150, y, 90, 25, hwnd, NULL, hInst, NULL);
        g_hChkBG = CreateWindowW(L"BUTTON", L"Background", WS_CHILD | BS_AUTOCHECKBOX, 250, y, 100, 25, hwnd, NULL, hInst, NULL);
        SendMessage(g_hChkFile, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkDir, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hChkBG, BM_SETCHECK, BST_CHECKED, 0);

        y += 45;
        g_hBtnAdd = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | BS_OWNERDRAW, 80, y, 100, 30, hwnd, (HMENU)100, hInst, NULL);
        g_hBtnDel = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | BS_OWNERDRAW, 190, y, 100, 30, hwnd, (HMENU)101, hInst, NULL);

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
        if (HIWORD(wParam) == BN_CLICKED) {
            if (wmId == 100) { // Add
                WCHAR name[256], path[MAX_PATH], args[MAX_PATH];
                GetWindowTextW(g_hEditName, name, 256);
                GetWindowTextW(g_hEditPath, path, MAX_PATH);
                GetWindowTextW(g_hEditArgs, args, MAX_PATH);
                DWORD f = (SendMessage(g_hChkFile, BM_GETCHECK, 0, 0) == BST_CHECKED), d = (SendMessage(g_hChkDir, BM_GETCHECK, 0, 0) == BST_CHECKED), b = (SendMessage(g_hChkBG, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (wcslen(name) > 0 && wcslen(path) > 0) {
                    HKEY hKey;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_CUSTOM, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                        HKEY hSubKey;
                        if (RegCreateKeyExW(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hSubKey, NULL) == ERROR_SUCCESS) {
                            RegSetValueExW(hSubKey, L"Path", 0, REG_SZ, (BYTE*)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"Args", 0, REG_SZ, (BYTE*)args, (DWORD)(wcslen(args) + 1) * sizeof(wchar_t));
                            RegSetValueExW(hSubKey, L"ShowFile", 0, REG_DWORD, (BYTE*)&f, sizeof(DWORD));
                            RegSetValueExW(hSubKey, L"ShowDir", 0, REG_DWORD, (BYTE*)&d, sizeof(DWORD));
                            RegSetValueExW(hSubKey, L"ShowBG", 0, REG_DWORD, (BYTE*)&b, sizeof(DWORD));
                            RegCloseKey(hSubKey);
                        }
                        RegCloseKey(hKey);
                    }
                    LoadCustomCommands();
                }
            } else if (wmId == 101) { // Delete
                int sel = ListView_GetNextItem(g_hListCustom, -1, LVNI_SELECTED);
                if (sel != -1) {
                    WCHAR name[256]; ListView_GetItemText(g_hListCustom, sel, 0, name, 256);
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
                }
            } else {
                bool checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                auto UpdateGroup = [&](std::vector<SettingItem>& group) {
                    for (auto& item : group) if (item.hWnd == (HWND)lParam) { SetSetting(item.regValue.c_str(), checked); break; }
                };
                UpdateGroup(g_fileSettings); UpdateGroup(g_dirSettings); UpdateGroup(g_bgSettings);
            }
        }
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == TCN_SELCHANGE) {
            UpdateTabVisibility();
            InvalidateRect(hwnd, NULL, TRUE);
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
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);
    g_hbrBackground = CreateSolidBrush(DARK_BACKGROUND);
    g_hbrControlBack = CreateSolidBrush(DARK_CONTROL_BACK);
    const wchar_t CLASS_NAME[] = L"SettingsDialogClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, hInstance, hIcon, LoadCursor(NULL, IDC_ARROW), g_hbrBackground, NULL, CLASS_NAME, hIcon };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"xToolsMenu Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 420, 660, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    RECT rect; GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(hwnd, nCmdShow);
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    DeleteObject(g_hbrBackground);
    DeleteObject(g_hbrControlBack);
    return 0;
}
