#include "ShellExtension.h"
#include "resource.h"
#include "DarkMode.h"
#include "FileTypeHelper.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <sddl.h>
#include <appmodel.h>
#include <filesystem>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

extern HINSTANCE g_hInst;
HINSTANCE g_hInst = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

static HRESULT ResolveRelativeIconPath(PCWSTR pszRelativePath, LPWSTR* ppszIcon)
{
    if (!pszRelativePath || !*pszRelativePath) return E_INVALIDARG;

    // If it's already an absolute path (contains :) or a resource string (contains ,), return as is
    if (wcschr(pszRelativePath, L':') || wcschr(pszRelativePath, L','))
    {
        return SHStrDupW(pszRelativePath, ppszIcon);
    }

    std::wstring themeSubDir = DarkModeManager::GetThemeSubDir();
    std::wstring relStr = pszRelativePath;

    // Construct the theme-specific relative path
    std::wstring themePath;
    if (relStr.find(L"Assets\\") == 0)
        themePath = L"Assets\\" + themeSubDir + L"\\" + relStr.substr(7);
    else if (relStr.find(L"Icons\\") == 0)
        themePath = L"Icons\\" + themeSubDir + L"\\" + relStr.substr(6);
    else
        themePath = themeSubDir + L"\\" + relStr;

    WCHAR szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule)))
    {
        PathRemoveFileSpecW(szModule); // Directory containing the DLL (AppPackage)

        // 1. Try absolute theme path
        WCHAR szFull[MAX_PATH];
        wcscpy_s(szFull, szModule);
        PathAppendW(szFull, themePath.c_str());
        if (PathFileExistsW(szFull))
        {
            return SHStrDupW(szFull, ppszIcon);
        }

        // 2. Try absolute original path
        wcscpy_s(szFull, szModule);
        PathAppendW(szFull, pszRelativePath);
        if (PathFileExistsW(szFull))
        {
            return SHStrDupW(szFull, ppszIcon);
        }
    }

    return E_FAIL;
}

static HBITMAP CreateMenuBitmapFromIcon(PCWSTR pszIconPath)
{
    LPWSTR pszFull = nullptr;
    if (FAILED(ResolveRelativeIconPath(pszIconPath, &pszFull))) return NULL;

    HICON hIcon = (HICON)LoadImageW(NULL, pszFull, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

    if (!hIcon)
    {
        std::wstring cleanPath = pszFull;
        int iconIndex = 0;
        size_t commaPos = cleanPath.find_last_of(L',');
        if (commaPos != std::wstring::npos)
        {
            std::wstring indexStr = cleanPath.substr(commaPos + 1);
            bool isIndex = !indexStr.empty();
            for (size_t i = 0; i < indexStr.size(); ++i)
            {
                if (i == 0 && indexStr[i] == L'-') continue;
                if (!iswdigit(indexStr[i]))
                {
                    isIndex = false;
                    break;
                }
            }
            if (isIndex)
            {
                iconIndex = _wtoi(indexStr.c_str());
                cleanPath = cleanPath.substr(0, commaPos);
            }
        }

        if (cleanPath.size() >= 2 && cleanPath.front() == L'"' && cleanPath.back() == L'"')
        {
            cleanPath = cleanPath.substr(1, cleanPath.size() - 2);
        }

        HICON hSmallIcon = NULL;
        if (ExtractIconExW(cleanPath.c_str(), iconIndex, NULL, &hSmallIcon, 1) > 0 && hSmallIcon)
        {
            hIcon = hSmallIcon;
        }
    }

    CoTaskMemFree(pszFull);

    if (!hIcon) return NULL;

    HBITMAP hBitmap = NULL;
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx == 0) cx = 16;
    if (cy == 0) cy = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (hdcMem)
    {
        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = cx;
        bmi.bmiHeader.biHeight = cy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        if (hBitmap && pBits)
        {
            HGDIOBJ hOldBmp = SelectObject(hdcMem, hBitmap);

            RECT rect = { 0, 0, cx, cy };
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdcMem, &rect, hBrush);
            DeleteObject(hBrush);

            DrawIconEx(hdcMem, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

            unsigned char* pPixel = (unsigned char*)pBits;
            for (int i = 0; i < cx * cy; i++)
            {
                if (pPixel[3] == 0)
                {
                    pPixel[0] = 0;
                    pPixel[1] = 0;
                    pPixel[2] = 0;
                }
                pPixel += 4;
            }

            SelectObject(hdcMem, hOldBmp);
        }
        DeleteDC(hdcMem);
    }
    ReleaseDC(NULL, hdcScreen);

    DestroyIcon(hIcon);
    return hBitmap;
}

// IExplorerCommand implementation for root menu
IFACEMETHODIMP XToolsMenuCommand::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    return SHStrDupW(L"xToolsMenu", ppszName);
}

IFACEMETHODIMP XToolsMenuCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (SUCCEEDED(ResolveRelativeIconPath(L"Assets\\ICON.ico", ppszIcon)))
    {
        return S_OK;
    }

    WCHAR szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule)))
    {
        std::wstring icon = szModule;
        icon += L",-101";
        return SHStrDupW(icon.c_str(), ppszIcon);
    }
    return SHStrDupW(L"shell32.dll,-16769", ppszIcon);
}

IFACEMETHODIMP XToolsMenuCommand::GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip)
{
    return SHStrDupW(L"xToolsMenu - Power User Tools", ppszInfotip);
}

IFACEMETHODIMP XToolsMenuCommand::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = __uuidof(XToolsMenuCommand);
    return S_OK;
}

IFACEMETHODIMP XToolsMenuCommand::GetState(IShellItemArray*, BOOL, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

IFACEMETHODIMP XToolsMenuCommand::Invoke(IShellItemArray*, IBindCtx*)
{
    return S_OK;
}

IFACEMETHODIMP XToolsMenuCommand::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_HASSUBCOMMANDS;
    return S_OK;
}

IFACEMETHODIMP XToolsMenuCommand::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    return MakeAndInitialize<XToolsCommandEnumerator>(ppEnum);
}

IFACEMETHODIMP XToolsMenuCommand::SetSite(IUnknown* pUnkSite)
{
    _spUnkSite = pUnkSite;
    return S_OK;
}

IFACEMETHODIMP XToolsMenuCommand::GetSite(REFIID riid, void** ppvSite)
{
    return _spUnkSite.CopyTo(riid, ppvSite);
}

static std::vector<std::wstring> GetTargetPaths(IShellItemArray* psiItemArray, IUnknown* pUnkSite);


// SubCommand implementation
IFACEMETHODIMP XToolsSubCommand::GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName)
{
    std::wstring title = _title;
    return SHStrDupW(title.c_str(), ppszName);
}

IFACEMETHODIMP XToolsSubCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (_icon.empty())
    {
        *ppszIcon = nullptr;
        return E_NOTIMPL;
    }

    if (!_icon.empty() && _icon[0] == L'#')
    {
        WCHAR szModule[MAX_PATH];
        if (GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule)))
        {
            std::wstring iconRes = szModule;
            iconRes += L",-";
            iconRes += _icon.substr(1);
            return SHStrDupW(iconRes.c_str(), ppszIcon);
        }
    }

    if (SUCCEEDED(ResolveRelativeIconPath(_icon.c_str(), ppszIcon)))
    {
        return S_OK;
    }

    return SHStrDupW(_icon.c_str(), ppszIcon);
}

IFACEMETHODIMP XToolsSubCommand::GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip)
{
    *ppszInfotip = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP XToolsSubCommand::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = GUID_NULL;
    return E_NOTIMPL;
}

bool IsCommandVisible(XToolsAction action, const std::wstring& customName, bool isNewMenu, bool isFolder, bool isBackground, const std::vector<std::wstring>& selectedPaths)
{
    if (action == XToolsAction::Settings) return true;

    if (action == XToolsAction::Custom)
    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\xToolsMenu\\CustomCommands", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            HKEY hSubKey;
            if (RegOpenKeyExW(hKey, customName.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS)
            {
                DWORD showFile = 1, showDir = 1, showBG = 1, allowedFileTypes = 0xFFFF;
                DWORD dwSize = sizeof(DWORD);
                RegGetValueW(hSubKey, NULL, L"ShowFile", RRF_RT_REG_DWORD, NULL, &showFile, &dwSize);
                RegGetValueW(hSubKey, NULL, L"ShowDir", RRF_RT_REG_DWORD, NULL, &showDir, &dwSize);
                RegGetValueW(hSubKey, NULL, L"ShowBG", RRF_RT_REG_DWORD, NULL, &showBG, &dwSize);
                RegGetValueW(hSubKey, NULL, L"AllowedFileTypes", RRF_RT_REG_DWORD, NULL, &allowedFileTypes, &dwSize);

                const wchar_t* valName = L"Enabled_Files";
                if (isBackground) valName = L"Enabled_Background";
                else if (isFolder) valName = L"Enabled_Directory";

                std::wstring valNameStr = valName;
                if (!isNewMenu) valNameStr += L"_Old";

                DWORD enabled = (isNewMenu && !isBackground && !isFolder && wcsstr(valNameStr.c_str(), L"_Old") == nullptr) ? 1 : 0;
                // Actually the default for New Menu is 1, Old Menu is 0.
                enabled = (wcsstr(valNameStr.c_str(), L"_Old") != nullptr) ? 0 : 1;

                dwSize = sizeof(DWORD);
                RegGetValueW(hSubKey, NULL, valNameStr.c_str(), RRF_RT_REG_DWORD, NULL, &enabled, &dwSize);

                RegCloseKey(hSubKey);
                RegCloseKey(hKey);

                if (enabled == 0) return false;
                if (isBackground && !showBG) return false;
                if (isFolder && !showDir) return false;
                if (!isBackground && !isFolder)
                {
                    if (!showFile) return false;
                    for (const auto& path : selectedPaths)
                    {
                        FileTypeCategory cat = FileTypeHelper::GetCategoryForPath(path.c_str());
                        if (!((DWORD)cat & allowedFileTypes)) return false;
                    }
                }
                return true;
            }
            RegCloseKey(hKey);
        }
        return false;
    }

    // Built-in commands
    if (isBackground)
    {
        if (action == XToolsAction::CopyName || action == XToolsAction::CopyPath || action == XToolsAction::EditWith || action == XToolsAction::TakeOwnership) return false;
    }
    else if (isFolder)
    {
        if (action == XToolsAction::SystemFolders || action == XToolsAction::PasteToFile || action == XToolsAction::EditWith) return false;
    }
    else // File
    {
        if (action == XToolsAction::Terminal || action == XToolsAction::TerminalAdmin || action == XToolsAction::SystemFolders || action == XToolsAction::PasteToFile) return false;
    }

    const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";
    std::wstring valName;
    if (isBackground) valName = L"Background_";
    else if (isFolder) valName = L"Directory_";
    else valName = L"Files_";

    switch (action)
    {
    case XToolsAction::OpenExe: valName += L"Attributes"; break;
    case XToolsAction::Terminal: valName += L"Terminal"; break;
    case XToolsAction::TerminalAdmin: valName += L"TerminalAdmin"; break;
    case XToolsAction::EditWith: valName += L"EditWith"; break;
    case XToolsAction::SystemFolders: valName += L"SystemFolders"; break;
    case XToolsAction::PasteToFile: valName += L"PasteToFile"; break;
    case XToolsAction::CopyName: valName += L"CopyName"; break;
    case XToolsAction::CopyPath: valName += L"CopyPath"; break;
    case XToolsAction::TakeOwnership: valName += L"TakeOwnership"; break;
    default: return true;
    }

    if (!isNewMenu) valName += L"_Old";

    DWORD value = (wcsstr(valName.c_str(), L"_Old") != nullptr) ? 0 : 1;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, REG_PATH, valName.c_str(), RRF_RT_REG_DWORD, NULL, &value, &size);
    return value != 0;
}

IFACEMETHODIMP XToolsSubCommand::GetState(IShellItemArray* psiItemArray, BOOL, EXPCMDSTATE* pCmdState)
{
    std::vector<std::wstring> paths = GetTargetPaths(psiItemArray, _spUnkSite.Get());
    bool isBackground = paths.empty();
    bool isFolder = false;
    if (!paths.empty()) {
        DWORD attrs = GetFileAttributesW(paths[0].c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) isFolder = (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    if (IsCommandVisible(_action, _title, true, isFolder, isBackground, paths))
        *pCmdState = ECS_ENABLED;
    else
        *pCmdState = ECS_HIDDEN;

    return S_OK;
}

static bool RunElevatedCommand(const std::wstring& parameters)
{
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = parameters.c_str();
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei))
    {
        if (sei.hProcess != NULL)
        {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
        return true;
    }
    return false;
}

static HWND GetHWNDFromSite(IUnknown* pUnkSite)
{
    HWND hwnd = NULL;
    if (pUnkSite)
    {
        ComPtr<IOleWindow> pow;
        if (SUCCEEDED(pUnkSite->QueryInterface(IID_PPV_ARGS(&pow))))
        {
            pow->GetWindow(&hwnd);
        }
    }
    return hwnd;
}

static std::wstring GetCurrentUserSidString()
{
    std::wstring sidString;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<BYTE> buffer(dwSize);
            if (GetTokenInformation(hToken, TokenUser, buffer.data(), dwSize, &dwSize))
            {
                PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                LPWSTR pSid = NULL;
                if (ConvertSidToStringSidW(pTokenUser->User.Sid, &pSid))
                {
                    sidString = pSid;
                    LocalFree(pSid);
                }
            }
        }
        CloseHandle(hToken);
    }
    return sidString;
}

static bool TakeOwnershipRecursive(const std::wstring& targetPath)
{
    std::wstring sid = GetCurrentUserSidString();
    if (sid.empty()) return false;
    std::wstring parameters = L"/c takeown.exe /f \"" + targetPath + L"\" /r /d y "
        L"& icacls.exe \"" + targetPath + L"\" /grant Administrators:F /grant *\"" + sid + L"\":(OI)(CI)F "
        L"& icacls.exe \"" + targetPath + L"\" /inheritance:e /t /c /q";
    return RunElevatedCommand(parameters);
}

static void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

static std::vector<std::wstring> GetTargetPaths(IShellItemArray* psiItemArray, IUnknown* pUnkSite) {
    std::vector<std::wstring> paths;
    if (psiItemArray) {
        DWORD count = 0;
        psiItemArray->GetCount(&count);
        for (DWORD i = 0; i < count; i++) {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(i, &item))) {
                LPWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    paths.push_back(path);
                    CoTaskMemFree(path);
                }
            }
        }
    }
    if (paths.empty() && pUnkSite) {
        ComPtr<IServiceProvider> sp;
        if (SUCCEEDED(pUnkSite->QueryInterface(IID_PPV_ARGS(&sp)))) {
            ComPtr<IShellBrowser> sb;
            if (SUCCEEDED(sp->QueryService(SID_SShellBrowser, IID_PPV_ARGS(&sb)))) {
                ComPtr<IShellView> sv;
                if (SUCCEEDED(sb->QueryActiveShellView(&sv))) {
                    ComPtr<IFolderView> fv;
                    if (SUCCEEDED(sv->QueryInterface(IID_PPV_ARGS(&fv)))) {
                        ComPtr<IShellItem> item;
                        if (SUCCEEDED(fv->GetFolder(IID_PPV_ARGS(&item)))) {
                            LPWSTR path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                                paths.push_back(path);
                                CoTaskMemFree(path);
                            }
                        }
                    }
                }
            }
        }
    }
    return paths;
}

static void ExecuteAction(XToolsAction action, const std::wstring& title, const std::wstring& data, const std::wstring& exePath, BOOL runAsAdmin, const std::vector<std::wstring>& paths, HWND hwnd)
{
    if (action == XToolsAction::SystemFolders && !data.empty())
    {
        std::wstring path;
        if (data == L"startmenu_user") {
            WCHAR szPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTMENU, NULL, 0, szPath))) path = szPath;
        }
        else if (data == L"startmenu_all") {
            WCHAR szPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_STARTMENU, NULL, 0, szPath))) path = szPath;
        }
        else if (data == L"temp") {
            WCHAR szPath[MAX_PATH];
            GetTempPathW(MAX_PATH, szPath);
            path = szPath;
        }
        else if (data == L"appdata_local") {
            WCHAR szPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath))) path = szPath;
        }
        else if (data == L"programdata") {
            WCHAR szPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath))) path = szPath;
        }

        if (!path.empty()) {
            AllowSetForegroundWindow(ASFW_ANY);
            ShellExecuteW(hwnd, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        return;
    }

    if (action == XToolsAction::OpenExe || action == XToolsAction::EditWith || action == XToolsAction::SystemFolders || action == XToolsAction::Settings || action == XToolsAction::Custom)
    {
        std::wstring targetExe, baseArgs;
        if (action == XToolsAction::Custom)
        {
            targetExe = exePath;
            baseArgs = data;
        }
        else
        {
            WCHAR szModule[MAX_PATH];
            GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
            PathRemoveFileSpecW(szModule);
            std::wstring exeName = data;
            if (action == XToolsAction::EditWith) exeName = L"EditWithDialog.exe";
            else if (action == XToolsAction::SystemFolders) exeName = L"SystemFoldersDialog.exe";
            else if (action == XToolsAction::Settings) exeName = L"Settings.exe";
            PathAppendW(szModule, exeName.c_str());
            targetExe = szModule;
        }

        AllowSetForegroundWindow(ASFW_ANY);

        if (baseArgs.find(L"%1") != std::wstring::npos)
        {
            for (const auto& path : paths)
            {
                std::wstring args = baseArgs;
                ReplaceAll(args, L"%1", path);
                ShellExecuteW(hwnd, runAsAdmin ? L"runas" : L"open", targetExe.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
            }
        }
        else
        {
            std::wstring fullArgs = baseArgs;
            for (const auto& path : paths)
            {
                if (!fullArgs.empty()) fullArgs += L" ";
                std::wstring p = path;
                if (!p.empty() && p.back() == L'\\') p += L'\\';
                fullArgs += L"\""; fullArgs += p; fullArgs += L"\"";
            }
            ShellExecuteW(hwnd, runAsAdmin ? L"runas" : L"open", targetExe.c_str(), fullArgs.empty() ? NULL : fullArgs.c_str(), NULL, SW_SHOWNORMAL);
        }
    }
    else if (action == XToolsAction::Terminal || action == XToolsAction::TerminalAdmin)
    {
        if (!paths.empty())
        {
            WCHAR szDir[MAX_PATH]; wcscpy_s(szDir, paths[0].c_str());
            DWORD attrs = GetFileAttributesW(paths[0].c_str());
            if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) PathRemoveFileSpecW(szDir);
            std::wstring dirStr(szDir);
            if (!dirStr.empty() && dirStr.back() == L'\\') dirStr += L'\\';
            std::wstring parameters = L"-d \"" + dirStr + L"\"";
            AllowSetForegroundWindow(ASFW_ANY);
            ShellExecuteW(hwnd, action == XToolsAction::TerminalAdmin ? L"runas" : L"open", L"wt.exe", parameters.c_str(), szDir, SW_SHOWNORMAL);
        }
    }
    else if (action == XToolsAction::CopyName || action == XToolsAction::CopyPath)
    {
        std::wstring text;
        for (const auto& path : paths)
        {
            if (!text.empty()) text += L"\r\n";
            if (action == XToolsAction::CopyName) text += PathFindFileNameW(path.c_str());
            else text += path;
        }
        if (!text.empty() && OpenClipboard(NULL))
        {
            EmptyClipboard();
            size_t size = (text.length() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem)
            {
                void* pMem = GlobalLock(hMem);
                if (pMem) { memcpy(pMem, text.c_str(), size); GlobalUnlock(hMem); SetClipboardData(CF_UNICODETEXT, hMem); }
            }
            CloseClipboard();
        }
    }
    else if (action == XToolsAction::TakeOwnership)
    {
        WCHAR szModule[MAX_PATH];
        GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
        PathRemoveFileSpecW(szModule);
        PathAppendW(szModule, L"TakeOwnership.exe");

        std::wstring params;
        for (const auto& path : paths)
        {
            params += L"\"";
            params += path;
            params += L"\" ";
        }
        AllowSetForegroundWindow(ASFW_ANY);
        ShellExecuteW(hwnd, L"runas", szModule, params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
    }
    else if (action == XToolsAction::PasteToFile)
    {
        if (!paths.empty())
        {
            WCHAR szDir[MAX_PATH]; wcscpy_s(szDir, paths[0].c_str());
            DWORD attrs = GetFileAttributesW(paths[0].c_str());
            if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) PathRemoveFileSpecW(szDir);
            PathAppendW(szDir, L"Clipboard.txt");
            if (OpenClipboard(NULL))
            {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData)
                {
                    LPCWSTR pText = (LPCWSTR)GlobalLock(hData);
                    if (pText)
                    {
                        HANDLE hFile = CreateFileW(szDir, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pText, -1, NULL, 0, NULL, NULL);
                            if (utf8Len > 0)
                            {
                                std::vector<char> utf8Text(utf8Len);
                                WideCharToMultiByte(CP_UTF8, 0, pText, -1, utf8Text.data(), utf8Len, NULL, NULL);
                                DWORD written;
                                WriteFile(hFile, utf8Text.data(), (DWORD)(utf8Len - 1), &written, NULL);
                            }
                            CloseHandle(hFile);
                        }
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
                SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, szDir, NULL);
            }
        }
    }
}

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray* psiItemArray, IBindCtx*)
{
    std::vector<std::wstring> paths = GetTargetPaths(psiItemArray, _spUnkSite.Get());
    HWND hwnd = GetHWNDFromSite(_spUnkSite.Get());
    ExecuteAction(_action, _title, _data, _exePath, _runAsAdmin, paths, hwnd);
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    *ppEnum = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP XToolsSubCommand::SetSite(IUnknown* pUnkSite)
{
    _spUnkSite = pUnkSite;
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::GetSite(REFIID riid, void** ppvSite)
{
    return _spUnkSite.CopyTo(riid, ppvSite);
}

// SubCommand Enumerator implementation
HRESULT XToolsCommandEnumerator::RuntimeClassInitialize()
{
    _current = 0;
    ComPtr<IExplorerCommand> cmd;
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Attributes", XToolsAction::OpenExe, L"Assets\\Attributes.ico", L"AttributesDialog.exe"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Terminal", XToolsAction::Terminal, L"Assets\\Terminals.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Terminal (admin)", XToolsAction::TerminalAdmin, L"Assets\\Terminals.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Edit with", XToolsAction::EditWith, L"Assets\\Edit with.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Paste to File", XToolsAction::PasteToFile, L"Assets\\Paste to File.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Name", XToolsAction::CopyName, L"Assets\\Copy Name.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Path", XToolsAction::CopyPath, L"Assets\\Copy Path.ico"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Take Ownership", XToolsAction::TakeOwnership, L"Assets\\Take Ownership.ico"))) _commands.push_back(cmd);

    // Load custom commands from registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\xToolsMenu\\CustomCommands", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD subKeys;
        RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        for (DWORD i = 0; i < subKeys; i++)
        {
            WCHAR name[256]; DWORD nSize = 256;
            if (RegEnumKeyExW(hKey, i, name, &nSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            {
                WCHAR path[MAX_PATH], args[32768], iconPath[MAX_PATH];
                DWORD pSize = sizeof(path), aSize = sizeof(args), iSize = sizeof(iconPath);
                DWORD showFile = 1, showDir = 1, showBG = 1, runAsAdmin = 0, enabled = 1;
                DWORD dwSize = sizeof(DWORD);

                RegGetValueW(hKey, name, L"Path", RRF_RT_REG_SZ, NULL, path, &pSize);
                RegGetValueW(hKey, name, L"Args", RRF_RT_REG_SZ, NULL, args, &aSize);

                bool isDark = DarkModeManager::IsDarkMode();
                const wchar_t* iconValueName = isDark ? L"IconPath_Dark" : L"IconPath_Light";
                if (RegGetValueW(hKey, name, iconValueName, RRF_RT_REG_SZ, NULL, iconPath, &iSize) != ERROR_SUCCESS || wcslen(iconPath) == 0)
                {
                    iSize = sizeof(iconPath);
                    if (RegGetValueW(hKey, name, L"IconPath", RRF_RT_REG_SZ, NULL, iconPath, &iSize) != ERROR_SUCCESS)
                    {
                        wcscpy_s(iconPath, path);
                    }
                }

                RegGetValueW(hKey, name, L"ShowFile", RRF_RT_REG_DWORD, NULL, &showFile, &dwSize);
                RegGetValueW(hKey, name, L"ShowDir", RRF_RT_REG_DWORD, NULL, &showDir, &dwSize);
                RegGetValueW(hKey, name, L"ShowBG", RRF_RT_REG_DWORD, NULL, &showBG, &dwSize);
                RegGetValueW(hKey, name, L"RunAsAdmin", RRF_RT_REG_DWORD, NULL, &runAsAdmin, &dwSize);

                DWORD enabledFile = 1, enabledDir = 1, enabledBg = 1, allowedFileTypes = 0xFFFF;
                RegGetValueW(hKey, name, L"Enabled_Files", RRF_RT_REG_DWORD, NULL, &enabledFile, &dwSize);
                RegGetValueW(hKey, name, L"Enabled_Directory", RRF_RT_REG_DWORD, NULL, &enabledDir, &dwSize);
                RegGetValueW(hKey, name, L"Enabled_Background", RRF_RT_REG_DWORD, NULL, &enabledBg, &dwSize);

                dwSize = sizeof(DWORD);
                RegGetValueW(hKey, name, L"AllowedFileTypes", RRF_RT_REG_DWORD, NULL, &allowedFileTypes, &dwSize);

                if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, name, XToolsAction::Custom, iconPath, args, showFile && enabledFile, showDir && enabledDir, showBG && enabledBg, path, runAsAdmin, allowedFileTypes))) _commands.push_back(cmd);
            }
        }
        RegCloseKey(hKey);
    }

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Settings", XToolsAction::Settings, L"Assets\\Settings.ico"))) _commands.push_back(cmd);
    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Next(ULONG celt, IExplorerCommand** apelt, ULONG* pceltFetched)
{
    ULONG fetched = 0;
    while (_current < _commands.size() && fetched < celt) { _commands[_current].CopyTo(&apelt[fetched]); _current++; fetched++; }
    if (pceltFetched) *pceltFetched = fetched;
    return fetched == celt ? S_OK : S_FALSE;
}

IFACEMETHODIMP XToolsCommandEnumerator::Skip(ULONG celt) { _current += celt; return S_OK; }
IFACEMETHODIMP XToolsCommandEnumerator::Reset() { _current = 0; return S_OK; }
IFACEMETHODIMP XToolsCommandEnumerator::Clone(IEnumExplorerCommand** ppenum) { return MakeAndInitialize<XToolsCommandEnumerator>(ppenum); }

// XToolsClassicMenu implementation
IFACEMETHODIMP XToolsClassicMenu::Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID)
{
    _selectedPaths.clear();
    _isBackground = (pdtobj == nullptr);
    _isFolder = false;

    if (pdtobj)
    {
        FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stm;
        if (SUCCEEDED(pdtobj->GetData(&fe, &stm)))
        {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
            if (hDrop)
            {
                UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
                for (UINT i = 0; i < count; i++)
                {
                    WCHAR szPath[MAX_PATH];
                    if (DragQueryFileW(hDrop, i, szPath, MAX_PATH))
                    {
                        _selectedPaths.push_back(szPath);
                    }
                }
                GlobalUnlock(stm.hGlobal);
            }
            ReleaseStgMedium(&stm);
        }
    }

    if (_selectedPaths.empty() && pidlFolder)
    {
        WCHAR szPath[MAX_PATH];
        if (SHGetPathFromIDListW(pidlFolder, szPath))
        {
            _selectedPaths.push_back(szPath);
        }
    }

    if (!_selectedPaths.empty())
    {
        DWORD attrs = GetFileAttributesW(_selectedPaths[0].c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) _isFolder = (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    return S_OK;
}

IFACEMETHODIMP XToolsClassicMenu::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    if (uFlags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

    for (HBITMAP hbmp : _bitmaps) { if (hbmp) DeleteObject(hbmp); }
    _bitmaps.clear();
    _visibleItems.clear();

    std::vector<ClassicMenuItemInternal> allPossibleItems = {
        { L"Attributes", XToolsAction::OpenExe, L"AttributesDialog.exe", L"", L"Assets\\Attributes.ico", FALSE },
        { L"Terminal", XToolsAction::Terminal, L"", L"", L"Assets\\Terminals.ico", FALSE },
        { L"Terminal (admin)", XToolsAction::TerminalAdmin, L"", L"", L"Assets\\Terminals.ico", FALSE },
        { L"Edit with", XToolsAction::EditWith, L"", L"", L"Assets\\Edit with.ico", FALSE },
        { L"System Folders", XToolsAction::SystemFolders, L"", L"", L"Assets\\System Folders.ico", FALSE },
        { L"Paste to File", XToolsAction::PasteToFile, L"", L"", L"Assets\\Paste to File.ico", FALSE },
        { L"Copy Name", XToolsAction::CopyName, L"", L"", L"Assets\\Copy Name.ico", FALSE },
        { L"Copy Path", XToolsAction::CopyPath, L"", L"", L"Assets\\Copy Path.ico", FALSE },
        { L"Take Ownership", XToolsAction::TakeOwnership, L"", L"", L"Assets\\Take Ownership.ico", FALSE }
    };

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\xToolsMenu\\CustomCommands", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD subKeys;
        RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        for (DWORD i = 0; i < subKeys; i++)
        {
            WCHAR name[256]; DWORD nSize = 256;
            if (RegEnumKeyExW(hKey, i, name, &nSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            {
                WCHAR path[MAX_PATH], args[32768], iconPath[MAX_PATH];
                DWORD pSize = sizeof(path), aSize = sizeof(args), iSize = sizeof(iconPath), runAsAdmin = 0;
                DWORD dwSize = sizeof(DWORD);
                RegGetValueW(hKey, name, L"Path", RRF_RT_REG_SZ, NULL, path, &pSize);
                RegGetValueW(hKey, name, L"Args", RRF_RT_REG_SZ, NULL, args, &aSize);
                RegGetValueW(hKey, name, L"RunAsAdmin", RRF_RT_REG_DWORD, NULL, &runAsAdmin, &dwSize);

                bool isDark = DarkModeManager::IsDarkMode();
                const wchar_t* iconValueName = isDark ? L"IconPath_Dark" : L"IconPath_Light";
                iSize = sizeof(iconPath);
                if (RegGetValueW(hKey, name, iconValueName, RRF_RT_REG_SZ, NULL, iconPath, &iSize) != ERROR_SUCCESS || wcslen(iconPath) == 0)
                {
                    iSize = sizeof(iconPath);
                    if (RegGetValueW(hKey, name, L"IconPath", RRF_RT_REG_SZ, NULL, iconPath, &iSize) != ERROR_SUCCESS)
                    {
                        wcscpy_s(iconPath, path);
                    }
                }
                allPossibleItems.push_back({ name, XToolsAction::Custom, args, path, iconPath, (BOOL)runAsAdmin });
            }
        }
        RegCloseKey(hKey);
    }
    allPossibleItems.push_back({ L"Settings", XToolsAction::Settings, L"", L"", L"Assets\\Settings.ico", FALSE });

    HMENU hSubMenu = CreatePopupMenu();
    UINT count = 0;
    UINT menuPos = 0;
    for (const auto& item : allPossibleItems)
    {
        if (IsCommandVisible(item.action, item.title, false, _isFolder, _isBackground, _selectedPaths))
        {
            if (item.action == XToolsAction::SystemFolders)
            {
                HMENU hSysFoldersSubMenu = CreatePopupMenu();

                struct SubFolderItem {
                    std::wstring title;
                    std::wstring data;
                };
                std::vector<SubFolderItem> subFolders = {
                    { L"Start Menu (User)", L"startmenu_user" },
                    { L"Start Menu (All Users)", L"startmenu_all" },
                    { L"Temp Folder", L"temp" },
                    { L"AppData (Local)", L"appdata_local" },
                    { L"ProgramData", L"programdata" }
                };

                UINT subCount = 0;
                for (const auto& sf : subFolders)
                {
                    MENUITEMINFOW sfMii = { sizeof(sfMii) };
                    sfMii.fMask = MIIM_STRING | MIIM_ID;
                    sfMii.wID = idCmdFirst + count;
                    sfMii.dwTypeData = (LPWSTR)sf.title.c_str();

                    HBITMAP sfHbmp = CreateMenuBitmapFromIcon(item.icon.c_str());
                    if (sfHbmp)
                    {
                        sfMii.fMask |= MIIM_BITMAP;
                        sfMii.hbmpItem = sfHbmp;
                        _bitmaps.push_back(sfHbmp);
                    }

                    InsertMenuItemW(hSysFoldersSubMenu, subCount, TRUE, &sfMii);

                    ClassicMenuItemInternal sfInternalItem = { sf.title, XToolsAction::SystemFolders, sf.data, L"", item.icon, FALSE };
                    _visibleItems.push_back(sfInternalItem);

                    count++;
                    subCount++;
                }

                MENUITEMINFOW mii = { sizeof(mii) };
                mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
                mii.wID = idCmdFirst + count;
                mii.hSubMenu = hSysFoldersSubMenu;
                mii.dwTypeData = (LPWSTR)item.title.c_str();

                HBITMAP hbmp = CreateMenuBitmapFromIcon(item.icon.c_str());
                if (hbmp)
                {
                    mii.fMask |= MIIM_BITMAP;
                    mii.hbmpItem = hbmp;
                    _bitmaps.push_back(hbmp);
                }

                InsertMenuItemW(hSubMenu, menuPos, TRUE, &mii);
                menuPos++;

                // Add placeholder for the submenu header slot so that IDs line up
                _visibleItems.push_back({ item.title, XToolsAction::Custom, L"", L"", L"", FALSE });
                count++;
            }
            else
            {
                MENUITEMINFOW mii = { sizeof(mii) };
                mii.fMask = MIIM_STRING | MIIM_ID;
                mii.wID = idCmdFirst + count;
                mii.dwTypeData = (LPWSTR)item.title.c_str();

                HBITMAP hbmp = CreateMenuBitmapFromIcon(item.icon.c_str());
                if (hbmp)
                {
                    mii.fMask |= MIIM_BITMAP;
                    mii.hbmpItem = hbmp;
                    _bitmaps.push_back(hbmp);
                }

                InsertMenuItemW(hSubMenu, menuPos, TRUE, &mii);
                menuPos++;
                _visibleItems.push_back(item);
                count++;
            }
        }
    }

    if (count > 0)
    {
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
        mii.wID = idCmdFirst + count;
        mii.hSubMenu = hSubMenu;
        mii.dwTypeData = (LPWSTR)L"xToolsMenu";

        HBITMAP hbmpRoot = CreateMenuBitmapFromIcon(L"Assets\\ICON.ico");
        if (hbmpRoot)
        {
            mii.fMask |= MIIM_BITMAP;
            mii.hbmpItem = hbmpRoot;
            _bitmaps.push_back(hbmpRoot);
        }

        InsertMenuItemW(hmenu, indexMenu, TRUE, &mii);
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, count + 1);
    }

    DestroyMenu(hSubMenu);
    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
}


IFACEMETHODIMP XToolsClassicMenu::InvokeCommand(LPCMINVOKECOMMANDINFO lpici)
{
    if (HIWORD(lpici->lpVerb) != 0) return E_INVALIDARG;
    UINT id = LOWORD(lpici->lpVerb);
    if (id < _visibleItems.size())
    {
        const auto& item = _visibleItems[id];
        ExecuteAction(item.action, item.title, item.data, item.exePath, item.runAsAdmin, _selectedPaths, lpici->hwnd);
        return S_OK;
    }
    return E_INVALIDARG;
}

IFACEMETHODIMP XToolsClassicMenu::GetCommandString(UINT_PTR idCmd, UINT uType, UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
    return E_NOTIMPL;
}

class XToolsClassFactory : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IClassFactory>
{
public:
    HRESULT RuntimeClassInitialize(REFCLSID rclsid) { _rclsid = rclsid; return S_OK; }

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override
    {
        *ppvObject = nullptr; if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        if (_rclsid == __uuidof(XToolsMenuCommand))
        {
            ComPtr<XToolsMenuCommand> instance;
            HRESULT hr = MakeAndInitialize<XToolsMenuCommand>(&instance);
            if (SUCCEEDED(hr)) hr = instance.CopyTo(riid, ppvObject);
            return hr;
        }
        else if (_rclsid == __uuidof(XToolsClassicMenu))
        {
            ComPtr<XToolsClassicMenu> instance;
            HRESULT hr = MakeAndInitialize<XToolsClassicMenu>(&instance);
            if (SUCCEEDED(hr)) hr = instance.CopyTo(riid, ppvObject);
            return hr;
        }
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP LockServer(BOOL fLock) override { return S_OK; }

private:
    CLSID _rclsid;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    *ppv = nullptr;
    if (rclsid == __uuidof(XToolsMenuCommand) || rclsid == __uuidof(XToolsClassicMenu))
    {
        ComPtr<XToolsClassFactory> factory;
        HRESULT hr = MakeAndInitialize<XToolsClassFactory>(&factory, rclsid);
        if (SUCCEEDED(hr)) hr = factory.CopyTo(riid, ppv);
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() { return Module<InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE; }
