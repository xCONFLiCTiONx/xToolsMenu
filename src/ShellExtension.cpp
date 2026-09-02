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

IFACEMETHODIMP XToolsSubCommand::GetState(IShellItemArray*, BOOL, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray* psiItemArray, IBindCtx*)
{
    if (_action == XToolsAction::OpenExe)
    {
        WCHAR szModule[MAX_PATH];
        GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
        PathRemoveFileSpecW(szModule);
        PathAppendW(szModule, _data.c_str());

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

    // Guaranteed Wrench icon in Windows (Modern Fluent style in Win11)
    if (SUCCEEDED(MakeAndInitialize<XToolsSubCommand>(&cmd, L"Attributes", XToolsAction::OpenExe, L"shell32.dll,-255", L"AttributesDialog.exe")))
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
