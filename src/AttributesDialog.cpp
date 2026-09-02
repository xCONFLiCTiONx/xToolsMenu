#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Colors for Dark Theme
const COLORREF BACKGROUND_COLOR = RGB(32, 32, 32);
const COLORREF TEXT_COLOR = RGB(240, 240, 240);
const COLORREF GROUP_TEXT_COLOR = RGB(200, 200, 200);
HBRUSH g_hbrBackground = nullptr;

const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";

HWND g_hChkShowHidden = nullptr, g_hChkShowSystem = nullptr;
HWND g_hChkSetHidden = nullptr, g_hChkSetSystem = nullptr, g_hChkSetReadOnly = nullptr;

struct TimePickerPair {
    HWND hDate;
    HWND hTime;
    FILETIME original;
};

TimePickerPair g_timeCreation, g_timeAccess, g_timeWrite;

struct SelectionState {
    bool allHidden = true, someHidden = false;
    bool allSystem = true, someSystem = false;
    bool allReadOnly = true, someReadOnly = false;
    bool hasSelection = false;
};

SelectionState GetSelectionState() {
    SelectionState state;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        state.hasSelection = true;
        for (int i = 1; i < argc; i++) {
            DWORD attrs = GetFileAttributesW(argv[i]);
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                bool isHidden = (attrs & FILE_ATTRIBUTE_HIDDEN);
                bool isSystem = (attrs & FILE_ATTRIBUTE_SYSTEM);
                bool isReadOnly = (attrs & FILE_ATTRIBUTE_READONLY);

                if (isHidden) state.someHidden = true; else state.allHidden = false;
                if (isSystem) state.someSystem = true; else state.allSystem = false;
                if (isReadOnly) state.someReadOnly = true; else state.allReadOnly = false;
            }
        }
        LocalFree(argv);
    } else {
        state.allHidden = state.allSystem = state.allReadOnly = false;
    }
    return state;
}

bool GetRegistryValue(const wchar_t* name) {
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, REG_PATH, name, RRF_RT_REG_DWORD, NULL, &value, &size) == ERROR_SUCCESS) {
        if (wcscmp(name, L"Hidden") == 0) return (value == 1);
        return (value == 1);
    }
    return false;
}

void SetRegistryValue(const wchar_t* name, bool enabled) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : (wcscmp(name, L"ShowSuperHidden") == 0 ? 0 : 2);
        if (wcscmp(name, L"Hidden") == 0) value = enabled ? 1 : 2;
        RegSetValueExW(hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void ApplyFileAttributes(int wmId, bool enable) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        for (int i = 1; i < argc; i++) {
            DWORD attrs = GetFileAttributesW(argv[i]);
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                if (wmId == 3) { // Hidden
                    if (enable) attrs |= FILE_ATTRIBUTE_HIDDEN; else attrs &= ~FILE_ATTRIBUTE_HIDDEN;
                } else if (wmId == 4) { // System
                    if (enable) attrs |= FILE_ATTRIBUTE_SYSTEM; else attrs &= ~FILE_ATTRIBUTE_SYSTEM;
                } else if (wmId == 5) { // ReadOnly
                    if (enable) attrs |= FILE_ATTRIBUTE_READONLY; else attrs &= ~FILE_ATTRIBUTE_READONLY;
                }
                SetFileAttributesW(argv[i], attrs);
                SHChangeNotify(SHCNE_ATTRIBUTES, SHCNF_PATHW, argv[i], NULL);
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, argv[i], NULL);

                // Also notify parent to refresh visibility
                WCHAR szParent[MAX_PATH];
                wcscpy_s(szParent, argv[i]);
                if (PathRemoveFileSpecW(szParent)) {
                    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, szParent, NULL);
                }
            }
        }
        LocalFree(argv);
    }
}

void LoadTimeFromSelection() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        HANDLE hFile = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            GetFileTime(hFile, &g_timeCreation.original, &g_timeAccess.original, &g_timeWrite.original);
            CloseHandle(hFile);

            auto SetPicker = [](TimePickerPair& pair) {
                SYSTEMTIME st;
                FileTimeToSystemTime(&pair.original, &st);
                DateTime_SetSystemtime(pair.hDate, GDT_VALID, &st);
                DateTime_SetSystemtime(pair.hTime, GDT_VALID, &st);
            };
            SetPicker(g_timeCreation);
            SetPicker(g_timeAccess);
            SetPicker(g_timeWrite);
        }
        LocalFree(argv);
    }
}

void ApplyTimeChanges(TimePickerPair& pair, int type) {
    SYSTEMTIME stDate, stTime, stFinal;
    DateTime_GetSystemtime(pair.hDate, &stDate);
    DateTime_GetSystemtime(pair.hTime, &stTime);

    stFinal = stDate;
    stFinal.wHour = stTime.wHour;
    stFinal.wMinute = stTime.wMinute;
    stFinal.wSecond = stTime.wSecond;
    stFinal.wMilliseconds = stTime.wMilliseconds;

    FILETIME ft;
    SystemTimeToFileTime(&stFinal, &ft);

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        for (int i = 1; i < argc; i++) {
            HANDLE hFile = CreateFileW(argv[i], FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                SetFileTime(hFile, (type == 0 ? &ft : NULL), (type == 1 ? &ft : NULL), (type == 2 ? &ft : NULL));
                CloseHandle(hFile);
            }
        }
        LocalFree(argv);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, TEXT_COLOR);
        SetBkColor(hdc, BACKGROUND_COLOR);
        return (LRESULT)g_hbrBackground;
    }
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // --- System Attributes ---
        CreateWindowW(L"STATIC", L"System Attributes", WS_VISIBLE | WS_CHILD, 15, 10, 150, 20, hwnd, NULL, hInst, NULL);
        g_hChkShowHidden = CreateWindowW(L"BUTTON", L"Show Hidden Files", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 35, 180, 25, hwnd, (HMENU)1, hInst, NULL);
        g_hChkShowSystem = CreateWindowW(L"BUTTON", L"Show System Files", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 60, 180, 25, hwnd, (HMENU)2, hInst, NULL);

        // --- File Attributes ---
        CreateWindowW(L"STATIC", L"File Attributes", WS_VISIBLE | WS_CHILD, 210, 10, 150, 20, hwnd, NULL, hInst, NULL);
        g_hChkSetHidden = CreateWindowW(L"BUTTON", L"Set as Hidden", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 210, 35, 180, 25, hwnd, (HMENU)3, hInst, NULL);
        g_hChkSetSystem = CreateWindowW(L"BUTTON", L"Set as System", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 210, 60, 180, 25, hwnd, (HMENU)4, hInst, NULL);
        g_hChkSetReadOnly = CreateWindowW(L"BUTTON", L"Set as Read-only", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 210, 85, 180, 25, hwnd, (HMENU)5, hInst, NULL);

        // --- Time Attributes ---
        CreateWindowW(L"STATIC", L"Time Attributes", WS_VISIBLE | WS_CHILD, 15, 115, 150, 20, hwnd, NULL, hInst, NULL);

        auto CreateTimeRow = [&](const wchar_t* label, int y, TimePickerPair& pair, int baseId) {
            CreateWindowW(L"STATIC", label, WS_VISIBLE | WS_CHILD, 15, y, 100, 25, hwnd, NULL, hInst, NULL);
            pair.hDate = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_VISIBLE | WS_CHILD | DTS_SHORTDATECENTURYFORMAT, 120, y - 3, 160, 25, hwnd, (HMENU)(UINT_PTR)baseId, hInst, NULL);
            pair.hTime = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_VISIBLE | WS_CHILD | DTS_TIMEFORMAT, 290, y - 3, 100, 25, hwnd, (HMENU)(UINT_PTR)(baseId + 1), hInst, NULL);
        };

        CreateTimeRow(L"Creation Time:", 140, g_timeCreation, 100);
        CreateTimeRow(L"Last Access:", 170, g_timeAccess, 110);
        CreateTimeRow(L"Last Write:", 200, g_timeWrite, 120);

        // Init Values
        SendMessage(g_hChkShowHidden, BM_SETCHECK, GetRegistryValue(L"Hidden") ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_hChkShowSystem, BM_SETCHECK, GetRegistryValue(L"ShowSuperHidden") ? BST_CHECKED : BST_UNCHECKED, 0);

        SelectionState sel = GetSelectionState();
        SendMessage(g_hChkSetHidden, BM_SETCHECK, sel.allHidden ? BST_CHECKED : (sel.someHidden ? BST_INDETERMINATE : BST_UNCHECKED), 0);
        SendMessage(g_hChkSetSystem, BM_SETCHECK, sel.allSystem ? BST_CHECKED : (sel.someSystem ? BST_INDETERMINATE : BST_UNCHECKED), 0);
        SendMessage(g_hChkSetReadOnly, BM_SETCHECK, sel.allReadOnly ? BST_CHECKED : (sel.someReadOnly ? BST_INDETERMINATE : BST_UNCHECKED), 0);

        LoadTimeFromSelection();
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr->code == DTN_DATETIMECHANGE) {
            if (hdr->idFrom >= 100 && hdr->idFrom < 102) ApplyTimeChanges(g_timeCreation, 0);
            if (hdr->idFrom >= 110 && hdr->idFrom < 112) ApplyTimeChanges(g_timeAccess, 1);
            if (hdr->idFrom >= 120 && hdr->idFrom < 122) ApplyTimeChanges(g_timeWrite, 2);
        }
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (HIWORD(wParam) == BN_CLICKED) {
            if (wmId == 1 || wmId == 2) {
                SetRegistryValue(L"Hidden", SendMessage(g_hChkShowHidden, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetRegistryValue(L"ShowSuperHidden", SendMessage(g_hChkShowSystem, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ShellState", SMTO_ABORTIFHUNG, 5000, NULL);
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
            } else if (wmId >= 3 && wmId <= 5) {
                ApplyFileAttributes(wmId, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_DATE_CLASSES;
    InitCommonControlsEx(&icex);

    g_hbrBackground = CreateSolidBrush(BACKGROUND_COLOR);

    const wchar_t CLASS_NAME[] = L"AttributesDialogClass";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = g_hbrBackground;

    RegisterClassW(&wc);

    // Dynamic Title
    std::wstring title = L"Attributes";
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        title += L" - ";
        title += PathFindFileNameW(argv[1]);
        LocalFree(argv);
    }

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, title.c_str(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 280, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // Set Window Icon
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
    while (PathRemoveFileSpecW(szPath)) {
        WCHAR szIconPath[MAX_PATH];
        wcscpy_s(szIconPath, szPath);
        PathAppendW(szIconPath, L"ICON.ico");
        if (PathFileExistsW(szIconPath)) {
            HICON hIcon = (HICON)LoadImageW(NULL, szIconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            if (hIcon) {
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }
            break;
        }
    }

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
