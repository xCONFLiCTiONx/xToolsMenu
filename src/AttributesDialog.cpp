#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";

HWND g_hChkHidden = nullptr;
HWND g_hChkSystem = nullptr;
HWND g_hChkMakeHidden = nullptr;
HWND g_hChkMakeSuper = nullptr;

struct SelectionState {
    bool allHidden = true;
    bool allSuperHidden = true;
    bool someHidden = false;
    bool someSuperHidden = false;
};

SelectionState GetSelectionState() {
    SelectionState state;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        state.allHidden = true;
        state.allSuperHidden = true;
        for (int i = 1; i < argc; i++) {
            DWORD attrs = GetFileAttributesW(argv[i]);
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                bool isHidden = (attrs & FILE_ATTRIBUTE_HIDDEN);
                bool isSuper = (attrs & FILE_ATTRIBUTE_SYSTEM) && isHidden;

                if (isHidden) state.someHidden = true; else state.allHidden = false;
                if (isSuper) state.someSuperHidden = true; else state.allSuperHidden = false;
            }
        }
        LocalFree(argv);
    } else {
        state.allHidden = state.allSuperHidden = false;
    }
    return state;
}

// We need to store the selection if we want to act on files
// However, the current AttributesDialog is launched via ShellExecute from the extension.
// The extension currently doesn't pass the file paths to the exe.
// I should update the extension to pass paths, but first let's add the UI.

bool GetRegistryValue(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, REG_PATH, name, RRF_RT_REG_DWORD, NULL, &value, &size) == ERROR_SUCCESS) {
        if (wcscmp(name, L"Hidden") == 0) return (value == 1); // 1 = Show, 2 = Hide
        return (value == 1); // ShowSuperHidden: 1 = Show, 0 = Hide
    }
    // Default values if registry keys are missing
    if (wcscmp(name, L"Hidden") == 0) return false; // Default is Hide (2)
    return false; // Default is Hide (0)
}

void SetRegistryValue(const wchar_t* name, bool enabled) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD value = enabled ? 1 : (wcscmp(name, L"ShowSuperHidden") == 0 ? 0 : 2); // Hidden is 2 for hide, 1 for show
        if (wcscmp(name, L"Hidden") == 0) value = enabled ? 1 : 2;
        RegSetValueExW(hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Group Box for Explorer Settings
        CreateWindowW(L"BUTTON", L"Explorer Settings", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            10, 5, 225, 90, hwnd, NULL, NULL, NULL);

        g_hChkHidden = CreateWindowW(L"BUTTON", L"Show Hidden Files", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 25, 200, 25, hwnd, (HMENU)1, NULL, NULL);
        g_hChkSystem = CreateWindowW(L"BUTTON", L"Show System Files", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 55, 200, 25, hwnd, (HMENU)2, NULL, NULL);

        // Group Box for Selected Item Actions
        CreateWindowW(L"BUTTON", L"Selection Actions", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            10, 100, 225, 90, hwnd, NULL, NULL, NULL);

        g_hChkMakeHidden = CreateWindowW(L"BUTTON", L"Hidden", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 120, 205, 25, hwnd, (HMENU)3, NULL, NULL);
        g_hChkMakeSuper = CreateWindowW(L"BUTTON", L"Super Hidden", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 155, 205, 25, hwnd, (HMENU)4, NULL, NULL);

        SendMessage(g_hChkHidden, BM_SETCHECK, GetRegistryValue(L"Hidden") ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_hChkSystem, BM_SETCHECK, GetRegistryValue(L"ShowSuperHidden") ? BST_CHECKED : BST_UNCHECKED, 0);

        SelectionState sel = GetSelectionState();
        SendMessage(g_hChkMakeHidden, BM_SETCHECK, sel.allHidden ? BST_CHECKED : (sel.someHidden ? BST_INDETERMINATE : BST_UNCHECKED), 0);
        SendMessage(g_hChkMakeSuper, BM_SETCHECK, sel.allSuperHidden ? BST_CHECKED : (sel.someSuperHidden ? BST_INDETERMINATE : BST_UNCHECKED), 0);

        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (HIWORD(wParam) == BN_CLICKED) {
            if (wmId == 1 || wmId == 2) {
                bool hidden = SendMessage(g_hChkHidden, BM_GETCHECK, 0, 0) == BST_CHECKED;
                bool system = SendMessage(g_hChkSystem, BM_GETCHECK, 0, 0) == BST_CHECKED;

                SetRegistryValue(L"Hidden", hidden);
                SetRegistryValue(L"ShowSuperHidden", system);

                // Notify Explorer to refresh settings
                SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ShellState", SMTO_ABORTIFHUNG, 5000, NULL);
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
            }
            else if (wmId == 3 || wmId == 4) {
                bool enable = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;

                int argc;
                LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
                if (argv && argc > 1) {
                    for (int i = 1; i < argc; i++) {
                        DWORD attrs = GetFileAttributesW(argv[i]);
                        if (attrs != INVALID_FILE_ATTRIBUTES) {
                            if (wmId == 3) { // Hidden
                                if (enable) attrs |= FILE_ATTRIBUTE_HIDDEN;
                                else attrs &= ~FILE_ATTRIBUTE_HIDDEN;
                            } else { // Super Hidden (System + Hidden)
                                if (enable) attrs |= (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
                                else attrs &= ~FILE_ATTRIBUTE_SYSTEM;
                            }
                            SetFileAttributesW(argv[i], attrs);
                            SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, argv[i], NULL);
                        }
                    }
                    LocalFree(argv);
                }
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
    const wchar_t CLASS_NAME[] = L"AttributesDialogClass";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"xTools Attributes", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 260, 240, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // Dark Mode
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    // Center screen
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, NULL, (screenWidth - (rect.right - rect.left)) / 2, (screenHeight - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
