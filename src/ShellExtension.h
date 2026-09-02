#pragma once

#include <windows.h>
#include <shobjidl_core.h>
#include <wrl.h>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

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

class XToolsSubCommand : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand>
{
public:
    HRESULT RuntimeClassInitialize(PCWSTR title, PCWSTR exeName) {
        _title = title;
        _exeName = exeName;
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

private:
    std::wstring _title;
    std::wstring _exeName;
};

class XToolsCommandEnumerator : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumExplorerCommand>
{
public:
    HRESULT RuntimeClassInitialize() {
        _current = 0;
        ComPtr<IExplorerCommand> subCommand;
        return MakeAndInitialize<XToolsSubCommand>(&subCommand, L"Attributes", L"AttributesDialog.exe");
    }
    IFACEMETHODIMP Next(ULONG celt, __out_ecount_part(celt, *pceltFetched) IExplorerCommand** apelt, __out_opt ULONG* pceltFetched) override;
    IFACEMETHODIMP Skip(ULONG celt) override;
    IFACEMETHODIMP Reset() override;
    IFACEMETHODIMP Clone(__deref_out IEnumExplorerCommand** ppenum) override;

private:
    std::vector<ComPtr<IExplorerCommand>> _commands;
    ULONG _current;
};
