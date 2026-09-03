#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

const COLORREF BACKGROUND_COLOR = RGB(32, 32, 32);
const COLORREF TEXT_COLOR = RGB(240, 240, 240);
HBRUSH g_hbrBackground = nullptr;
HFONT g_hFont = nullptr;

struct EditorInfo {
    std::wstring name;
    std::wstring path;
};

std::vector<EditorInfo> g_editors;
std::vector<std::wstring> g_filesToOpen;

void FindEditors() {
    // 1. Notepad (System)
    WCHAR szNotepad[MAX_PATH];
    GetSystemDirectoryW(szNotepad, MAX_PATH);
    PathAppendW(szNotepad, L"notepad.exe");
    g_editors.push_back({ L"Notepad", szNotepad });

    // 2. Notepad++
    auto CheckPath = [](const wchar_t* name, const wchar_t* relativePath) {
        WCHAR szPath[MAX_PATH];
        ExpandEnvironmentStringsW(relativePath, szPath, MAX_PATH);
        if (PathFileExistsW(szPath)) {
            g_editors.push_back({ name, szPath });
        }
    };

    CheckPath(L"Notepad++", L"%ProgramFiles%\\Notepad++\\notepad++.exe");
    CheckPath(L"Notepad++ (x86)", L"%ProgramFiles(x86)%\\Notepad++\\notepad++.exe");

    // 3. VS Code
    CheckPath(L"VS Code", L"%LocalAppData%\\Programs\\Microsoft VS Code\\Code.exe");
    CheckPath(L"VS Code (System)", L"%ProgramFiles%\\Microsoft VS Code\\Code.exe");

    // 4. Sublime Text
    CheckPath(L"Sublime Text", L"%ProgramFiles%\\Sublime Text\\sublime_text.exe");
    CheckPath(L"Sublime Text 3", L"%ProgramFiles%\\Sublime Text 3\\sublime_text.exe");

    // 5. Try to find more from Registry (OpenWithList for .txt)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.txt\\OpenWithList", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // This is a simplified search, usually you'd want to look in OpenWithProgids too
        RegCloseKey(hKey);
    }
}

void OpenWithEditor(int index) {
    if (index < 0 || index >= g_editors.size()) return;

    std::wstring params;
    for (const auto& f : g_filesToOpen) {
        params += L"\"" + f + L"\" ";
    }

    ShellExecuteW(NULL, L"open", g_editors[index].path.c_str(), params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
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

        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        int y = 15;
        for (int i = 0; i < g_editors.size(); i++) {
            HWND hBtn = CreateWindowW(L"BUTTON", g_editors[i].name.c_str(), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, y, 260, 35, hwnd, (HMENU)(UINT_PTR)i, hInst, NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            y += 45;
        }

        // Add a "Browse..." button
        HWND hBrowse = CreateWindowW(L"BUTTON", L"Browse for editor...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, y, 260, 35, hwnd, (HMENU)999, hInst, NULL);
        SendMessage(hBrowse, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 999) {
            WCHAR szFile[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                g_editors.push_back({ L"Custom", szFile });
                OpenWithEditor(g_editors.size() - 1);
                PostQuitMessage(0);
            }
        } else if (id >= 0 && id < g_editors.size()) {
            OpenWithEditor(id);
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
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) g_filesToOpen.push_back(argv[i]);
        LocalFree(argv);
    }

    FindEditors();

    g_hbrBackground = CreateSolidBrush(BACKGROUND_COLOR);

    const wchar_t CLASS_NAME[] = L"EditWithDialogClass";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = g_hbrBackground;

    RegisterClassW(&wc);

    int height = (g_editors.size() + 2) * 45 + 20;
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Edit With", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 315, height, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

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
