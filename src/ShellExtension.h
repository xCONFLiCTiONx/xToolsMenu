#pragma once

#include <windows.h>
#include <shobjidl_core.h>
#include <wrl.h>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

enum class XToolsAction {
    OpenExe,
    MakeHidden,
    MakeSuperHidden,
    Terminal,
    TerminalAdmin,
    EditWith,
    SystemFolders,
    CopyName,
    CopyPath,
    TakeOwnership,
    PasteToFile,
    Settings,
    Custom
};

class __declspec(uuid("D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A"))
XToolsMenuCommand : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand, IObjectWithSite>
{
public:
    // IExplorerCommand
    IFACEMETHODIMP GetTitle(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszName) override;
    IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszIcon) override;
    IFACEMETHODIMP GetToolTip(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszInfotip) override;
    IFACEMETHODIMP GetCanonicalName(_Out_ GUID* pguidCommandName) override;
    IFACEMETHODIMP GetState(_In_opt_ IShellItemArray* psiItemArray, _In_ BOOL fOkToBeSlow, _Out_ EXPCMDSTATE* pCmdState) override;
    IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray* psiItemArray, _In_opt_ IBindCtx* pbc) override;
    IFACEMETHODIMP GetFlags(_Out_ EXPCMDFLAGS* pFlags) override;
    IFACEMETHODIMP EnumSubCommands(_Outptr_ IEnumExplorerCommand** ppEnum) override;

    // IObjectWithSite
    IFACEMETHODIMP SetSite(_In_ IUnknown* pUnkSite) override;
    IFACEMETHODIMP GetSite(_In_ REFIID riid, _Outptr_ void** ppvSite) override;

private:
    ComPtr<IUnknown> _spUnkSite;
};

class XToolsSubCommand : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand, IObjectWithSite>
{
public:
    HRESULT RuntimeClassInitialize(PCWSTR title, XToolsAction action, PCWSTR icon = nullptr, PCWSTR data = nullptr,
                                 BOOL showFile = TRUE, BOOL showDir = TRUE, BOOL showBG = TRUE, PCWSTR exePath = nullptr) {
        _title = title;
        _action = action;
        _icon = icon ? icon : L"";
        _data = data ? data : L"";
        _showFile = showFile;
        _showDir = showDir;
        _showBG = showBG;
        _exePath = exePath ? exePath : L"";
        return S_OK;
    }

    IFACEMETHODIMP GetTitle(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszName) override;
    IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszIcon) override;
    IFACEMETHODIMP GetToolTip(_In_opt_ IShellItemArray* psiItemArray, _Outptr_ LPWSTR* ppszInfotip) override;
    IFACEMETHODIMP GetCanonicalName(_Out_ GUID* pguidCommandName) override;
    IFACEMETHODIMP GetState(_In_opt_ IShellItemArray* psiItemArray, _In_ BOOL fOkToBeSlow, _Out_ EXPCMDSTATE* pCmdState) override;
    IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray* psiItemArray, _In_opt_ IBindCtx* pbc) override;
    IFACEMETHODIMP GetFlags(_Out_ EXPCMDFLAGS* pFlags) override;
    IFACEMETHODIMP EnumSubCommands(_Outptr_ IEnumExplorerCommand** ppEnum) override;

    // IObjectWithSite
    IFACEMETHODIMP SetSite(_In_ IUnknown* pUnkSite) override;
    IFACEMETHODIMP GetSite(_In_ REFIID riid, _Outptr_ void** ppvSite) override;

private:
    std::wstring _title;
    XToolsAction _action;
    std::wstring _icon;
    std::wstring _data;
    BOOL _showFile;
    BOOL _showDir;
    BOOL _showBG;
    std::wstring _exePath;
    ComPtr<IUnknown> _spUnkSite;
};

class XToolsCommandEnumerator : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumExplorerCommand>
{
public:
    HRESULT RuntimeClassInitialize();
    IFACEMETHODIMP Next(ULONG celt, __out_ecount_part(celt, *pceltFetched) IExplorerCommand** apelt, __out_opt ULONG* pceltFetched) override;
    IFACEMETHODIMP Skip(ULONG celt) override;
    IFACEMETHODIMP Reset() override;
    IFACEMETHODIMP Clone(__deref_out IEnumExplorerCommand** ppenum) override;

private:
    std::vector<ComPtr<IExplorerCommand>> _commands;
    ULONG _current;
};
