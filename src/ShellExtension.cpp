#include "ShellExtension.h"
#include <shlwapi.h>
#include <vector>

#pragma comment(lib, "shlwapi.lib")

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
    return SHStrDupW(L"shell32.dll,-16769", ppszIcon); // Just an example icon
}

IFACEMETHODIMP XToolsMenuCommand::GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip)
{
    *ppszInfotip = nullptr;
    return E_NOTIMPL;
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
IFACEMETHODIMP XToolsSubCommand::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    return SHStrDupW(_title.c_str(), ppszName);
}

IFACEMETHODIMP XToolsSubCommand::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    *ppszIcon = nullptr;
    return E_NOTIMPL;
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

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray*, IBindCtx*)
{
    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule));
    PathRemoveFileSpecW(szModule);
    PathAppendW(szModule, _exeName.c_str());

    ShellExecuteW(NULL, L"open", szModule, NULL, NULL, SW_SHOWNORMAL);
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
