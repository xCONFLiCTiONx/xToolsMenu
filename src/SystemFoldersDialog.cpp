#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include "resource.h"
#include "Theme.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

HBRUSH g_hbrBackground = nullptr;
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

void OpenFolder(int index) {
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
        ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
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
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
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

        int y = 15;
        for (int i = 0; i < g_folders.size(); i++) {
            HWND hBtn = CreateWindowW(L"BUTTON", g_folders[i].name.c_str(), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW, 20, y, 260, 35, hwnd, (HMENU)(UINT_PTR)i, hInst, NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            y += 45;
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 0 && id < g_folders.size()) {
            OpenFolder(id);
            PostQuitMessage(0);
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
    // MANDATORY: Must be the absolute first line to override Shell identity
    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.App");

    g_hbrBackground = CreateSolidBrush(DARK_BACKGROUND);

    const wchar_t CLASS_NAME[] = L"SystemFoldersDialogClass";
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

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"System Folders", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 315, 300, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // Explicitly set icons to ensure taskbar picks them up
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
