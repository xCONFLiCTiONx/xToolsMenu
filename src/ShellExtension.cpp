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
    *pguidCommandName = __uuidof(this);
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
    *pguidCommandName = __uuidof(this);
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::GetState(IShellItemArray*, BOOL, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::Invoke(IShellItemArray* psiArray, IBindCtx*)
{
    // Handle menu execution based on which subcommand was clicked
    if (_id == 1) {
        // Launch AttributesDialog.exe or perform action
        ShellExecuteW(NULL, L"open", L"AttributesDialog.exe", NULL, NULL, SW_SHOWNORMAL);
    }
    return S_OK;
}

IFACEMETHODIMP XToolsSubCommand::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

// SubCommand Enumerator
XToolsCommandEnumerator::XToolsCommandEnumerator(const std::vector<ComPtr<IExplorerCommand>>& commands)
    : _commands(commands), _index(0) {}

IFACEMETHODIMP XToolsCommandEnumerator::Next(ULONG celt, IExplorerCommand** rgelt, ULONG* pceltFetched)
{
    if (!rgelt) return E_POINTER;
    ULONG fetched = 0;
    while (_index < _commands.size() && fetched < celt)
    {
        _commands[_index].CopyTo(&rgelt[fetched]);
        fetched++;
        _index++;
    }
    if (pceltFetched) *pceltFetched = fetched;
    return fetched == celt ? S_OK : S_FALSE;
}

IFACEMETHODIMP XToolsCommandEnumerator::Skip(ULONG celt)
{
    _index += celt;
    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Reset()
{
    _index = 0;
    return S_OK;
}

IFACEMETHODIMP XToolsCommandEnumerator::Clone(IEnumExplorerCommand** ppEnum)
{
    return MakeAndInitialize<XToolsCommandEnumerator>(ppEnum, _commands);
}

// COM Class Factory
class XToolsClassFactory : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IClassFactory>
{
public:
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override
    {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        return MakeAndInitialize<XToolsMenuCommand>(ppvObject);
    }
    IFACEMETHODIMP LockServer(BOOL fLock) override { return S_OK; }
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (rclsid == __uuidof(XToolsMenuCommand))
    {
        return MakeAndInitialize<XToolsClassFactory>(ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
    return Module<InProc>::GetModule().GetObjectCount() == 0 ? S_OK : S_FALSE;
}
