#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool ElevateSelf() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)) == 0) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // Triggers UAC prompt
    sei.lpFile = szPath;   // Restarts current executable
    sei.nShow = SW_NORMAL;

    return ShellExecuteExW(&sei);
}

bool DeleteRegistryKeyTree(HKEY hKeyParent, const std::wstring& subKey) {
    LONG lResult = RegDeleteTreeW(hKeyParent, subKey.c_str());
    return lResult == ERROR_SUCCESS || lResult == ERROR_FILE_NOT_FOUND;
}

bool RunCommand(const std::wstring& cmd) {
    std::wstring command = cmd;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
}

int main() {
    if (!IsAdmin()) {
        if (ElevateSelf()) return 0;
        std::wcerr << L"Error: This uninstaller must be run as Administrator." << std::endl;
        return 1;
    }

    std::wcout << L"Unregistering Sparse Package..." << std::endl;
    RunCommand(L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage -ErrorAction SilentlyContinue\"");

    std::wcout << L"Deleting Registry Entries..." << std::endl;
    DeleteRegistryKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}");
    DeleteRegistryKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");
    DeleteRegistryKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\xToolsMenu");
    DeleteRegistryKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\xToolsMenu");
    DeleteRegistryKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\xToolsMenu");

    std::wcout << L"Restarting Explorer..." << std::endl;
    RunCommand(L"powershell.exe -NoProfile -Command \"Stop-Process -Name explorer -Force\"");

    std::wcout << L"Unregistration complete!" << std::endl;
    return 0;
}
