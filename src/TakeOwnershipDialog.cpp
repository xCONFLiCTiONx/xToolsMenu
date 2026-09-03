#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <accctrl.h>
#include <aclapi.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

HWND g_hProgress = nullptr;
HWND g_hLabel = nullptr;
HWND g_hBtnOK = nullptr;
HWND g_hList = nullptr;
HFONT g_hFont = nullptr;
std::vector<std::wstring> g_targets;
std::atomic<bool> g_processing{ false };
std::atomic<bool> g_finished{ false };

// Helper to get current user's SID
PSID GetCurrentUserSid() {
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            PTOKEN_USER pTokenUser = (PTOKEN_USER)LocalAlloc(LPTR, dwSize);
            if (pTokenUser && GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
                PSID pSid = (PSID)LocalAlloc(LPTR, GetLengthSid(pTokenUser->User.Sid));
                if (pSid) CopySid(GetLengthSid(pTokenUser->User.Sid), pSid, pTokenUser->User.Sid);
                LocalFree(pTokenUser);
                CloseHandle(hToken);
                return pSid;
            }
        }
        CloseHandle(hToken);
    }
    return NULL;
}

// Perform the actual ownership/ACL change on a single item
void ProcessItem(const std::wstring& path, PSID pSid) {
    if (!pSid) return;

    // 1. Take Ownership
    SetNamedSecurityInfoW((LPWSTR)path.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, pSid, NULL, NULL, NULL);

    // 2. Grant Full Control
    EXPLICIT_ACCESSW ea = { 0 };
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = (LPWSTR)pSid;

    PACL pOldDacl = NULL, pNewDacl = NULL;
    if (SetEntriesInAclW(1, &ea, NULL, &pNewDacl) == ERROR_SUCCESS) {
        SetNamedSecurityInfoW((LPWSTR)path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);
        if (pNewDacl) LocalFree(pNewDacl);
    }
}

void WorkerThread(HWND hwnd, PSID pSid) {
    std::vector<std::wstring> allFiles;

    auto Scan = [&](const std::wstring& root, auto& self) -> void {
        allFiles.push_back(root);
        WIN32_FIND_DATAW ffd;
        std::wstring search = root + L"\\*";
        HANDLE hFind = FindFirstFileW(search.c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0) {
                    std::wstring sub = root + L"\\" + ffd.cFileName;
                    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) self(sub, self);
                    else allFiles.push_back(sub);
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    };

    for (const auto& t : g_targets) {
        DWORD attr = GetFileAttributesW(t.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) Scan(t, Scan);
        else allFiles.push_back(t);
    }

    SendMessage(g_hProgress, PBM_SETRANGE32, 0, (LPARAM)allFiles.size());

    for (size_t i = 0; i < allFiles.size(); i++) {
        SetWindowTextW(g_hLabel, allFiles[i].c_str());
        ProcessItem(allFiles[i], pSid);
        SendMessage(g_hProgress, PBM_SETPOS, (WPARAM)(i + 1), 0);
    }

    SetWindowTextW(g_hLabel, L"Completed!");
    SetWindowTextW(g_hBtnOK, L"Close");
    EnableWindow(g_hBtnOK, TRUE);
    g_finished = true;
    g_processing = false;
    if (pSid) LocalFree(pSid);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        HDC hdc = GetDC(hwnd);
        int logHeight = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        ReleaseDC(hwnd, hdc);
        g_hFont = CreateFontW(logHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        CreateWindowW(L"STATIC", L"Items to process:", WS_VISIBLE | WS_CHILD, 15, 10, 300, 20, hwnd, NULL, hInst, NULL);
        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_VSCROLL | LBS_HASSTRINGS, 15, 35, 450, 100, hwnd, NULL, hInst, NULL);

        for (const auto& t : g_targets) SendMessage(g_hList, LB_ADDSTRING, 0, (LPARAM)t.c_str());

        g_hLabel = CreateWindowW(L"STATIC", L"Click OK to start taking ownership...", WS_VISIBLE | WS_CHILD | SS_PATHELLIPSIS, 15, 145, 450, 20, hwnd, NULL, hInst, NULL);
        g_hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 15, 170, 450, 25, hwnd, NULL, hInst, NULL);
        g_hBtnOK = CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 365, 205, 100, 30, hwnd, (HMENU)IDOK, hInst, NULL);

        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            return TRUE;
        }, 0);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDOK) {
            if (g_finished) {
                PostQuitMessage(0);
            } else if (!g_processing) {
                g_processing = true;
                EnableWindow(g_hBtnOK, FALSE);
                PSID pSid = GetCurrentUserSid();
                std::thread(WorkerThread, hwnd, pSid).detach();
            }
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
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) g_targets.push_back(argv[i]);
        LocalFree(argv);
    }

    if (g_targets.empty()) return 0;

    const wchar_t CLASS_NAME[] = L"TakeOwnershipDialogClass";
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, hInstance,
        (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), NULL, CLASS_NAME, NULL };
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Take Ownership", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 290, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    RECT rect; GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, nCmdShow);
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
