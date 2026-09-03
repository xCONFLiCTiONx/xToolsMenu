#include "ShellExtension.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

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

// IExplorerCommand implementation for root menu
IFACEMETHODIMP XToolsMenuCommand::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    return SHStrDupW(L"xToolsMenu", ppszName);
}

IFACEMETHODIMP XToolsMenuCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(g_hInst, szPath, ARRAYSIZE(szPath));

    // Walk up the directory tree to find ICON.ico in the root
    while (PathRemoveFileSpecW(szPath))
    {
        WCHAR szIconPath[MAX_PATH];
        wcscpy_s(szIconPath, szPath);
        PathAppendW(szIconPath, L"ICON.ico");

        if (PathFileExistsW(szIconPath))
        {
            return SHStrDupW(szIconPath, ppszIcon);
        }
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

// SubCommand implementation
IFACEMETHODIMP XToolsSubCommand::GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName)
{
    std::wstring title = _title;

    if (psiItemArray && (_action == XToolsAction::MakeHidden || _action == XToolsAction::MakeSuperHidden))
    {
        DWORD count = 0;
        psiItemArray->GetCount(&count);
        if (count == 1)
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item)))
            {
                LPWSTR name = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &name)))
                {
                    std::wstring type = (_action == XToolsAction::MakeHidden) ? L"Hidden" : L"Super Hidden";
                    title = L"Make '" + std::wstring(name) + L"' " + type;
                    CoTaskMemFree(name);
                }
            }
        }
        else if (count > 1)
        {
            std::wstring type = (_action == XToolsAction::MakeHidden) ? L"Hidden" : L"Super Hidden";
            title = L"Make " + std::to_wstring(count) + L" items " + type;
        }
    }

    return SHStrDupW(title.c_str(), ppszName);
}

IFACEMETHODIMP XToolsSubCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (_icon.empty())
    {
        *ppszIcon = nullptr;
        return E_NOTIMPL;
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

IFACEMETHODIMP XToolsSubCommand::GetState(IShellItemArray* psiItemArray, BOOL, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;

    if (psiItemArray)
    {
        DWORD count = 0;
        psiItemArray->GetCount(&count);
        if (count > 0)
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item)))
            {
                SFGAOF attributes;
                if (SUCCEEDED(item->GetAttributes(SFGAO_FOLDER, &attributes)))
                {
                    bool isFolder = (attributes & SFGAO_FOLDER);

                    if (_action == XToolsAction::EditWith && isFolder)
                    {
                        *pCmdState = ECS_HIDDEN;
                    }
                    else if (_action == XToolsAction::PasteToFile && !isFolder)
                    {
                        *pCmdState = ECS_HIDDEN;
                    }
                }
            }
        }
    }

    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray* psiItemArray, IBindCtx*)
{
    if (_action == XToolsAction::OpenExe || _action == XToolsAction::EditWith || _action == XToolsAction::SystemFolders)
    {
        WCHAR szModule[MAX_PATH];
        GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
        PathRemoveFileSpecW(szModule);

        std::wstring exeName = _data;
        if (_action == XToolsAction::EditWith) exeName = L"EditWithDialog.exe";
        else if (_action == XToolsAction::SystemFolders) exeName = L"SystemFoldersDialog.exe";

        PathAppendW(szModule, exeName.c_str());

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

        ShellExecuteW(NULL, L"open", szModule, params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
    }
    else if (_action == XToolsAction::Terminal)
    {
        if (psiItemArray)
        {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(psiItemArray->GetItemAt(0, &item)))
            {
                LPWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    WCHAR szDir[MAX_PATH];
                    wcscpy_s(szDir, path);

                    DWORD attrs = GetFileAttributesW(path);
                    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        PathRemoveFileSpecW(szDir);
                    }

                    ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, szDir, SW_SHOWNORMAL);
                    CoTaskMemFree(path);
                }
            }
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
                        if (_action == XToolsAction::CopyName)
                            text += PathFindFileNameW(path);
                        else
                            text += path;
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
                    if (pMem)
                    {
                        memcpy(pMem, text.c_str(), size);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
        }
    }
    else if (_action == XToolsAction::TakeOwnership)
    {
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
                        std::wstring cmd = L"/c takeown /f \"" + std::wstring(path) + L"\" /r /d y & icacls \"" + std::wstring(path) + L"\" /grant administrators:F /t";
                        ShellExecuteW(NULL, L"runas", L"cmd.exe", cmd.c_str(), NULL, SW_HIDE);
                        CoTaskMemFree(path);
                    }
                }
            }
        }
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
                    WCHAR szDir[MAX_PATH];
                    wcscpy_s(szDir, path);

                    DWORD attrs = GetFileAttributesW(path);
                    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        PathRemoveFileSpecW(szDir);
                    }

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
                                        // Write without BOM as is standard for UTF-8
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

// SubCommand Enumerator implementation
HRESULT XToolsCommandEnumerator::RuntimeClassInitialize()
{
    _current = 0;
    ComPtr<IExplorerCommand> cmd;

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Attributes", XToolsAction::OpenExe, L"C:\\Windows\\System32\\imageres.dll,-166", L"AttributesDialog.exe")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Terminal", XToolsAction::Terminal, L"C:\\Windows\\System32\\imageres.dll,-5324")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Edit with", XToolsAction::EditWith, L"C:\\Windows\\System32\\shell32.dll,-243")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"System Folders", XToolsAction::SystemFolders, L"C:\\Windows\\System32\\imageres.dll,-3")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Paste to File", XToolsAction::PasteToFile, L"C:\\Windows\\System32\\shell32.dll,-16763")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Name", XToolsAction::CopyName, L"C:\\Windows\\System32\\shell32.dll,-134")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Copy Path", XToolsAction::CopyPath, L"C:\\Windows\\System32\\shell32.dll,-135")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Create Junction", XToolsAction::CreateJunction, L"C:\\Windows\\System32\\shell32.dll,-214")))
        _commands.push_back(cmd);

    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Take Ownership", XToolsAction::TakeOwnership, L"C:\\Windows\\System32\\imageres.dll,-78")))
        _commands.push_back(cmd);

    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Next(ULONG celt, IExplorerCommand** apelt, ULONG* pceltFetched)
{
    ULONG fetched = 0;
    while (_current < _commands.size() && fetched < celt)
    {
        _commands[_current].CopyTo(&apelt[fetched]);
        _current++;
        fetched++;
    }

    if (pceltFetched) *pceltFetched = fetched;
    return fetched == celt ? S_OK : S_FALSE;
}

IFACEMETHODIMP XToolsCommandEnumerator::Skip(ULONG celt)
{
    _current += celt;
    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Reset()
{
    _current = 0;
    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Clone(IEnumExplorerCommand** ppenum)
{
    return MakeAndInitialize<XToolsCommandEnumerator>(ppenum);
}

// COM Class Factory
class XToolsClassFactory : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IClassFactory>
{
public:
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override
    {
        *ppvObject = nullptr;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        ComPtr<XToolsMenuCommand> instance;
        HRESULT hr = MakeAndInitialize<XToolsMenuCommand>(&instance);
        if (SUCCEEDED(hr))
        {
            hr = instance.CopyTo(riid, ppvObject);
        }
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
        if (SUCCEEDED(hr))
        {
            hr = factory.CopyTo(riid, ppv);
        }
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
    return Module<InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE;
}
