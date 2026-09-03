#include "ShellExtension.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <sddl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

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

    WCHAR szPath[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szPath, ARRAYSIZE(szPath)))
    {
        while (PathRemoveFileSpecW(szPath))
        {
            WCHAR szIconPath[MAX_PATH];
            wcscpy_s(szIconPath, szPath);
            PathAppendW(szIconPath, pszRelativePath);
            if (PathFileExistsW(szIconPath))
            {
                return SHStrDupW(szIconPath, ppszIcon);
            }
        }
    }
    return E_FAIL;
}

// IExplorerCommand implementation for root menu
IFACEMETHODIMP XToolsMenuCommand::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    return SHStrDupW(L"xToolsMenu", ppszName);
}

IFACEMETHODIMP XToolsMenuCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (SUCCEEDED(ResolveRelativeIconPath(L"Icons\\ICON.ico", ppszIcon))) return S_OK;
    if (SUCCEEDED(ResolveRelativeIconPath(L"ICON.ico", ppszIcon))) return S_OK;
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

static bool IsFeatureEnabled(XToolsAction action, bool isFolder, bool isBackground)
{
    if (action == XToolsAction::Settings || action == XToolsAction::Custom) return true;
    const wchar_t* REG_PATH = L"Software\\xToolsMenu\\Settings";
    std::wstring prefix;
    if (isBackground) prefix = L"Background_";
    else if (isFolder) prefix = L"Directory_";
    else prefix = L"Files_";
    std::wstring name;
    switch (action)
    {
    case XToolsAction::OpenExe: name = L"Attributes"; break;
    case XToolsAction::Terminal: name = L"Terminal"; break;
    case XToolsAction::TerminalAdmin: name = L"TerminalAdmin"; break;
    case XToolsAction::EditWith: name = L"EditWith"; break;
    case XToolsAction::SystemFolders: name = L"SystemFolders"; break;
    case XToolsAction::PasteToFile: name = L"PasteToFile"; break;
    case XToolsAction::CopyName: name = L"CopyName"; break;
    case XToolsAction::CopyPath: name = L"CopyPath"; break;
    case XToolsAction::TakeOwnership: name = L"TakeOwnership"; break;
    default: return true;
    }
    std::wstring valueName = prefix + name;
    DWORD value = 1;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, REG_PATH, valueName.c_str(), RRF_RT_REG_DWORD, NULL, &value, &size);
    return value != 0;
}

IFACEMETHODIMP XToolsSubCommand::GetState(IShellItemArray* psiItemArray, BOOL, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;
    bool isFolder = false, isBackground = false;
    if (!psiItemArray)
    {
        isBackground = true;
        if (_action == XToolsAction::Custom && !_showBG)
        {
            *pCmdState = ECS_HIDDEN;
            return S_OK;
        }
        if (_action == XToolsAction::CopyName || _action == XToolsAction::CopyPath || _action == XToolsAction::EditWith || _action == XToolsAction::TakeOwnership)
        {
            *pCmdState = ECS_HIDDEN;
            return S_OK;
        }
    }
    else
    {
        DWORD count = 0;
        psiItemArray->GetCount(&count);
        if (count == 0) isBackground = true;
        else
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item)))
            {
                SFGAOF attrs;
                if (SUCCEEDED(item->GetAttributes(SFGAO_FOLDER, &attrs)))
                {
                    isFolder = (attrs & SFGAO_FOLDER);
                    if (_action == XToolsAction::Custom)
                    {
                        if (isFolder && !_showDir) { *pCmdState = ECS_HIDDEN; return S_OK; }
                        if (!isFolder && !_showFile) { *pCmdState = ECS_HIDDEN; return S_OK; }
                    }
                    if (isFolder)
                    {
                        if (_action == XToolsAction::SystemFolders || _action == XToolsAction::PasteToFile || _action == XToolsAction::EditWith)
                        {
                            *pCmdState = ECS_HIDDEN;
                            return S_OK;
                        }
                    }
                    else
                    {
                        if (_action == XToolsAction::Terminal || _action == XToolsAction::TerminalAdmin || _action == XToolsAction::SystemFolders || _action == XToolsAction::PasteToFile)
                        {
                            *pCmdState = ECS_HIDDEN;
                            return S_OK;
                        }
                    }
                }
            }
        }
    }
    if (!IsFeatureEnabled(_action, isFolder, isBackground)) *pCmdState = ECS_HIDDEN;
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

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray* psiItemArray, IBindCtx*)
{
    if (_action == XToolsAction::OpenExe || _action == XToolsAction::EditWith || _action == XToolsAction::SystemFolders || _action == XToolsAction::Settings || _action == XToolsAction::Custom)
    {
        std::wstring exePath, baseArgs;
        if (_action == XToolsAction::Custom)
        {
            exePath = _exePath;
            baseArgs = _data;
        }
        else
        {
            WCHAR szModule[MAX_PATH];
            GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
            PathRemoveFileSpecW(szModule);
            std::wstring exeName = _data;
            if (_action == XToolsAction::EditWith) exeName = L"EditWithDialog.exe";
            else if (_action == XToolsAction::SystemFolders) exeName = L"SystemFoldersDialog.exe";
            else if (_action == XToolsAction::Settings) exeName = L"Settings.exe";
            PathAppendW(szModule, exeName.c_str());
            exePath = szModule;
        }

        std::vector<std::wstring> paths = GetTargetPaths(psiItemArray, _spUnkSite.Get());

        if (baseArgs.find(L"%1") != std::wstring::npos)
        {
            // Execute for each path
            for (const auto& path : paths)
            {
                std::wstring args = baseArgs;
                ReplaceAll(args, L"%1", path);
                ShellExecuteW(NULL, L"open", exePath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
            }
        }
        else
        {
            // Append all paths to one command
            std::wstring fullArgs = baseArgs;
            for (const auto& path : paths)
            {
                if (!fullArgs.empty()) fullArgs += L" ";
                fullArgs += L"\""; fullArgs += path; fullArgs += L"\"";
            }
            ShellExecuteW(NULL, L"open", exePath.c_str(), fullArgs.empty() ? NULL : fullArgs.c_str(), NULL, SW_SHOWNORMAL);
        }
    }
    else if (_action == XToolsAction::Terminal || _action == XToolsAction::TerminalAdmin)
    {
        LPWSTR path = nullptr;
        if (psiItemArray)
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item))) item->GetDisplayName(SIGDN_FILESYSPATH, &path);
        }
        else if (_spUnkSite)
        {
            ComPtr<IServiceProvider> sp;
            if (SUCCEEDED(_spUnkSite.As(&sp)))
            {
                ComPtr<IShellBrowser> sb;
                if (SUCCEEDED(sp->QueryService(SID_SShellBrowser, IID_PPV_ARGS(&sb))))
                {
                    ComPtr<IShellView> sv;
                    if (SUCCEEDED(sb->QueryActiveShellView(&sv)))
                    {
                        ComPtr<IFolderView> fv;
                        if (SUCCEEDED(sv->QueryInterface(IID_PPV_ARGS(&fv))))
                        {
                            ComPtr<IShellItem> item;
                            if (SUCCEEDED(fv->GetFolder(IID_PPV_ARGS(&item)))) item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                        }
                    }
                }
            }
        }
        if (path)
        {
            WCHAR szDir[MAX_PATH]; wcscpy_s(szDir, path);
            DWORD attrs = GetFileAttributesW(path);
            if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) PathRemoveFileSpecW(szDir);
            std::wstring parameters = L"-d \"" + std::wstring(szDir) + L"\"";
            ShellExecuteW(NULL, _action == XToolsAction::TerminalAdmin ? L"runas" : L"open", L"wt.exe", parameters.c_str(), szDir, SW_SHOWNORMAL);
            CoTaskMemFree(path);
        }
    }
    else if (_action == XToolsAction::CopyName || _action == XToolsAction::CopyPath)
    {
        if (psiItemArray)
        {
            std::wstring text;
            DWORD count = 0;
            psiItemArray->GetCount(&count);
            for (DWORD i = 0; i < count; i++)
            {
                ComPtr<IShellItem> item;
                if (SUCCEEDED(psiItemArray->GetItemAt(i, &item)))
                {
                    LPWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                    {
                        if (!text.empty()) text += L"\r\n";
                        if (_action == XToolsAction::CopyName) text += PathFindFileNameW(path);
                        else text += path;
                        CoTaskMemFree(path);
                    }
                }
            }
            if (OpenClipboard(NULL))
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
    }
    else if (_action == XToolsAction::TakeOwnership)
    {
        WCHAR szModule[MAX_PATH];
        GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
        PathRemoveFileSpecW(szModule);
        PathAppendW(szModule, L"TakeOwnership.exe");

        std::wstring params;
        if (psiItemArray)
        {
            DWORD count = 0;
            psiItemArray->GetCount(&count);
            for (DWORD i = 0; i < count; i++)
            {
                ComPtr<IShellItem> item;
                if (SUCCEEDED(psiItemArray->GetItemAt(i, &item)))
                {
                    LPWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                    {
                        params += L"\"";
                        params += path;
                        params += L"\" ";
                        CoTaskMemFree(path);
                    }
                }
            }
        }
        ShellExecuteW(NULL, L"runas", szModule, params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
    }
    else if (_action == XToolsAction::PasteToFile)
    {
        if (psiItemArray)
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item)))
            {
                LPWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    WCHAR szDir[MAX_PATH]; wcscpy_s(szDir, path);
                    DWORD attrs = GetFileAttributesW(path);
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
                    CoTaskMemFree(path);
                }
            }
        }
    }
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
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Attributes", XToolsAction::OpenExe, L"Icons\\Attributes.svg", L"AttributesDialog.exe"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Terminal", XToolsAction::Terminal, L"Icons\\Terminals.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Terminal (admin)", XToolsAction::TerminalAdmin, L"Icons\\Terminals.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Edit with", XToolsAction::EditWith, L"Icons\\Edit with.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"System Folders", XToolsAction::SystemFolders, L"Icons\\System Folders.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Paste to File", XToolsAction::PasteToFile, L"Icons\\Paste to File.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Name", XToolsAction::CopyName, L"Icons\\Copy Name.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Path", XToolsAction::CopyPath, L"Icons\\Copy Path.svg"))) _commands.push_back(cmd);
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Take Ownership", XToolsAction::TakeOwnership, L"Icons\\Take Ownership.svg"))) _commands.push_back(cmd);

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
                WCHAR path[MAX_PATH], args[MAX_PATH], iconPath[MAX_PATH];
                DWORD pSize = sizeof(path), aSize = sizeof(args), iSize = sizeof(iconPath);
                DWORD showFile = 1, showDir = 1, showBG = 1;
                DWORD dwSize = sizeof(DWORD);

                RegGetValueW(hKey, name, L"Path", RRF_RT_REG_SZ, NULL, path, &pSize);
                RegGetValueW(hKey, name, L"Args", RRF_RT_REG_SZ, NULL, args, &aSize);

                if (RegGetValueW(hKey, name, L"IconPath", RRF_RT_REG_SZ, NULL, iconPath, &iSize) != ERROR_SUCCESS)
                {
                    wcscpy_s(iconPath, path);
                }

                RegGetValueW(hKey, name, L"ShowFile", RRF_RT_REG_DWORD, NULL, &showFile, &dwSize);
                RegGetValueW(hKey, name, L"ShowDir", RRF_RT_REG_DWORD, NULL, &showDir, &dwSize);
                RegGetValueW(hKey, name, L"ShowBG", RRF_RT_REG_DWORD, NULL, &showBG, &dwSize);

                if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, name, XToolsAction::Custom, iconPath, args, showFile, showDir, showBG, path))) _commands.push_back(cmd);
            }
        }
        RegCloseKey(hKey);
    }

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Settings", XToolsAction::Settings, L"Icons\\Settings.svg"))) _commands.push_back(cmd);
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

class XToolsClassFactory : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IClassFactory>
{
public:
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override
    {
        *ppvObject = nullptr; if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        ComPtr<XToolsMenuCommand> instance;
        HRESULT hr = MakeAndInitialize<XToolsMenuCommand>(&instance);
        if (SUCCEEDED(hr)) hr = instance.CopyTo(riid, ppvObject);
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL fLock) override { return S_OK; }
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    *ppv = nullptr;
    if (rclsid == __uuidof(XToolsMenuCommand))
    {
        ComPtr<XToolsClassFactory> factory;
        HRESULT hr = MakeAndInitialize<XToolsClassFactory>(&factory);
        if (SUCCEEDED(hr)) hr = factory.CopyTo(riid, ppv);
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() { return Module<InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE; }
