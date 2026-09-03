#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <algorithm>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

HFONT g_hFont = nullptr;

struct EditorInfo {
    std::wstring name;
    std::wstring path;
};

std::vector<EditorInfo> g_editors;
std::vector<std::wstring> g_filesToOpen;

void AddEditor(const std::wstring& name, const std::wstring& path) {
    if (path.empty()) return;

    WCHAR szFull[MAX_PATH];
    if (GetFullPathNameW(path.c_str(), MAX_PATH, szFull, NULL) == 0) {
        wcsncpy_s(szFull, path.c_str(), _TRUNCATE);
    }

    if (!PathFileExistsW(szFull)) return;

    // Deduplicate by path and name
    for (const auto& e : g_editors) {
        if (_wcsicmp(e.path.c_str(), szFull) == 0) return;

        if (_wcsicmp(e.name.c_str(), name.c_str()) == 0) {
            const wchar_t* f1 = PathFindFileNameW(e.path.c_str());
            const wchar_t* f2 = PathFindFileNameW(szFull);
            if (_wcsicmp(f1, f2) == 0) return;
        }
    }
    g_editors.push_back({ name, szFull });
}

void FindEditors() {
    g_editors.clear();

    std::wstring ext = L".txt";
    if (!g_filesToOpen.empty()) {
        ext = PathFindExtensionW(g_filesToOpen[0].c_str());
        if (ext.empty()) ext = L".txt";
    }

    auto ScanHandlers = [&](const wchar_t* extension) {
        IEnumAssocHandlers* pEnum = nullptr;
        if (SUCCEEDED(SHAssocEnumHandlers(extension, ASSOC_FILTER_RECOMMENDED, &pEnum))) {
            IAssocHandler* pHandler = nullptr;
            ULONG fetched = 0;
            while (pEnum->Next(1, &pHandler, &fetched) == S_OK && fetched == 1) {
                LPWSTR name = nullptr;
                LPWSTR path = nullptr;
                if (SUCCEEDED(pHandler->GetUIName(&name))) {
                    if (SUCCEEDED(pHandler->GetName(&path))) {
                        if (PathFileExistsW(path)) {
                            AddEditor(name, path);
                        } else {
                            WCHAR szPath[MAX_PATH];
                            DWORD dwSize = MAX_PATH;
                            if (SUCCEEDED(AssocQueryStringW(ASSOCF_INIT_BYEXENAME, ASSOCSTR_EXECUTABLE, path, NULL, szPath, &dwSize))) {
                                AddEditor(name, szPath);
                            }
                        }
                        CoTaskMemFree(path);
                    }
                    CoTaskMemFree(name);
                }
                pHandler->Release();
            }
            pEnum->Release();
        }
    };

    ScanHandlers(ext.c_str());
    if (ext != L".txt") ScanHandlers(L".txt");

    // Common editors lookup via App Paths
    const std::pair<std::wstring, std::wstring> common[] = {
        { L"VS Code", L"code.exe" },
        { L"Notepad++", L"notepad++.exe" },
        { L"Sublime Text", L"sublime_text.exe" }
    };

    for (const auto& c : common) {
        WCHAR szPath[MAX_PATH];
        DWORD dwSize = MAX_PATH;
        if (SUCCEEDED(AssocQueryStringW(ASSOCF_INIT_BYEXENAME, ASSOCSTR_EXECUTABLE, c.second.c_str(), NULL, szPath, &dwSize))) {
            AddEditor(c.first, szPath);
        }
    }

    // Fallbacks
    WCHAR szNotepad[MAX_PATH];
    GetSystemDirectoryW(szNotepad, MAX_PATH);
    PathAppendW(szNotepad, L"notepad.exe");
    AddEditor(L"Notepad", szNotepad);
}

void OpenWithEditor(int index) {
    if (index < 0 || index >= (int)g_editors.size()) return;

    std::wstring params;
    for (const auto& f : g_filesToOpen) {
        params += L"\"" + f + L"\" ";
    }

    // Allow the launched process to take the foreground
    AllowSetForegroundWindow(ASFW_ANY);

    ShellExecuteW(NULL, L"open", g_editors[index].path.c_str(), params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        int y = 15;
        for (int i = 0; i < (int)g_editors.size(); i++) {
            HWND hBtn = CreateWindowW(L"BUTTON", g_editors[i].name.c_str(), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, y, 260, 35, hwnd, (HMENU)(UINT_PTR)i, hInst, NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            y += 45;
        }

        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 0 && id < (int)g_editors.size()) {
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
    // MANDATORY: Must be the absolute first line to override Shell identity
    SetCurrentProcessExplicitAppUserModelID(L"xToolsMenu.App");

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) g_filesToOpen.push_back(argv[i]);
        LocalFree(argv);
    }

    FindEditors();

    const wchar_t CLASS_NAME[] = L"EditWithDialogClass";
    HICON hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;

    RegisterClassExW(&wc);

    int height = (int)g_editors.size() * 45 + 65;
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Edit With", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 315, height, NULL, NULL, hInstance, NULL);

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
