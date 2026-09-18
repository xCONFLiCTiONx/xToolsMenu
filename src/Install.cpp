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

bool SetRegistryKeyString(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& valueData) {
    HKEY hKey;
    LONG lResult = RegCreateKeyExW(hKeyParent, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    lResult = RegSetValueExW(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(valueData.c_str()), static_cast<DWORD>((valueData.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return lResult == ERROR_SUCCESS;
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
        std::wcerr << L"Error: This installer must be run as Administrator." << std::endl;
        return 1;
    }

    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        std::wcerr << L"Error: Failed to get executable path." << std::endl;
        return 1;
    }

    fs::path baseDir = fs::path(exePath).parent_path();
    fs::path dllPath = baseDir / "xToolsMenu.dll";

    if (!fs::exists(dllPath)) {
        std::wcerr << L"Error: xToolsMenu.dll not found in: " << baseDir.wstring() << std::endl;
        return 1;
    }

    fs::path registrationDir = baseDir;
    fs::path manifestPath = baseDir / "AppxManifest.xml";

    if (!fs::exists(manifestPath) && fs::exists(baseDir / "AppPackage" / "AppxManifest.xml")) {
        manifestPath = baseDir / "AppPackage" / "AppxManifest.xml";
    }

    if (!fs::exists(manifestPath)) {
        std::wcerr << L"Error: AppxManifest.xml not found." << std::endl;
        return 1;
    }

    std::wcout << L"Registering COM Server (Windows 11 Menu)..." << std::endl;
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}\\InprocServer32", L"", dllPath.wstring());
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}\\InprocServer32", L"ThreadingModel", L"Apartment");

    std::wcout << L"Registering Classic COM Server (Classic Menu)..." << std::endl;
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}\\InprocServer32", L"", dllPath.wstring());
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}\\InprocServer32", L"ThreadingModel", L"Apartment");

    std::wcout << L"Registering Classic Context Menu Handlers..." << std::endl;
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");

    std::wcout << L"Unregistering Sparse Package if exists..." << std::endl;
    RunCommand(L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage -ErrorAction SilentlyContinue\"");

    std::wcout << L"Registering Sparse Package..." << std::endl;
    std::wstring addPackageCmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Add-AppxPackage -Register -Path '" + manifestPath.wstring() + L"' -ExternalLocation '" + registrationDir.wstring() + L"'\"";

    if (RunCommand(addPackageCmd)) {
        std::wcout << L"Restarting Explorer to apply updates..." << std::endl;
        RunCommand(L"powershell.exe -NoProfile -Command \"Stop-Process -Name explorer -Force\"");

        std::wcout << L"Registration completed successfully!" << std::endl;
        return 0;
    } else {
        std::wcerr << L"Error: Failed to register sparse package." << std::endl;
        return 1;
    }
}
