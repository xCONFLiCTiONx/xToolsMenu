#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include "resource.h"
#include "DarkMode.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")

HFONT g_hFont = nullptr;

struct FolderInfo {
    std::wstring name;
    int csidl;
    std::wstring path;
};

std::vector<FolderInfo> g_folders = {
    { L"Start Menu (User)", CSIDL_STARTMENU, L"" },
    { L"Start Menu (All Users)", CSIDL_COMMON_STARTMENU, L"" },
    { L"Temp Folder", -1, L"" }, // Special case for TEMP
    { L"AppData (Local)", CSIDL_LOCAL_APPDATA, L"" },
    { L"ProgramData", CSIDL_COMMON_APPDATA, L"" }
};

void OpenFolder(HWND hwnd, int index) {
    std::wstring path;
    if (g_folders[index].csidl != -1) {
        WCHAR szPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, g_folders[index].csidl, NULL, 0, szPath))) {
            path = szPath;
        }
    } else {
        WCHAR szPath[MAX_PATH];
        GetTempPathW(MAX_PATH, szPath);
        path = szPath;
    }

    if (!path.empty()) {
        AllowSetForegroundWindow(ASFW_ANY);
        ShellExecuteW(hwnd, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
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

        int y = 15;
        for (int i = 0; i < g_folders.size(); i++) {
            HWND hBtn = CreateWindowW(L"BUTTON", g_folders[i].name.c_str(), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, y, 260, 35, hwnd, (HMENU)(UINT_PTR)i, hInst, NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            y += 45;
        }
        DarkModeManager::ApplyToControls(hwnd);
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
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 0 && id < g_folders.size()) {
            OpenFolder(hwnd, id);
            PostQuitMessage(0);
        }
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
    // MANDATORY: Must be the absolute first line to override Shell identity
    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.App");

    const wchar_t CLASS_NAME[] = L"SystemFoldersDialogClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = NULL;
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"System Folders", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 315, 300, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // Explicitly set icons to ensure taskbar picks them up
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
