#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <ctime>

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

static std::wofstream g_logFile;

void InitLog(const std::wstring& filename) {
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) > 0) {
        fs::path logPath = fs::path(tempPath) / filename;
        g_logFile.open(logPath, std::ios::out | std::ios::trunc);
    }
}

void Log(const std::wstring& msg) {
    std::wcout << msg << std::endl;
    if (g_logFile.is_open()) {
        std::time_t now = std::time(nullptr);
        std::tm tmNow;
        localtime_s(&tmNow, &now);
        wchar_t timeBuf[32];
        wcsftime(timeBuf, ARRAYSIZE(timeBuf), L"[%Y-%m-%d %H:%M:%S] ", &tmNow);
        g_logFile << timeBuf << msg << std::endl;
        g_logFile.flush();
    }
}

bool SetRegistryKeyString(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& valueData) {
    HKEY hKey;
    LONG lResult = RegCreateKeyExW(hKeyParent, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lResult != ERROR_SUCCESS) {
        Log(L"RegCreateKeyExW failed for " + subKey + L" (error " + std::to_wstring(lResult) + L")");
        return false;
    }
    lResult = RegSetValueExW(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(valueData.c_str()), static_cast<DWORD>((valueData.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    if (lResult != ERROR_SUCCESS) {
        Log(L"RegSetValueExW failed for " + subKey + L" (error " + std::to_wstring(lResult) + L")");
    }
    return lResult == ERROR_SUCCESS;
}

bool RunCommand(const std::wstring& cmd, DWORD timeoutMs = 60000) {
    Log(L"Running: " + cmd);
    std::wstring command = cmd;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, &command[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        Log(L"CreateProcessW failed (error " + std::to_wstring(err) + L")");
        return false;
    }

    CloseHandle(hWritePipe);

    DWORD startTick = GetTickCount();
    std::string output;
    char buf[4096];
    bool timedOut = false;

    while (true) {
        DWORD avail = 0;
        if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD bytesRead = 0;
            if (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                output.append(buf, bytesRead);
            }
            continue;
        }
        DWORD waitRes = WaitForSingleObject(pi.hProcess, 100);
        if (waitRes == WAIT_OBJECT_0) {
            // Drain any remaining output
            while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD bytesRead = 0;
                if (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buf[bytesRead] = '\0';
                    output.append(buf, bytesRead);
                } else break;
            }
            break;
        }
        if (GetTickCount() - startTick >= timeoutMs) {
            timedOut = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    CloseHandle(hReadPipe);

    if (!output.empty()) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), static_cast<int>(output.size()), NULL, 0);
        if (wlen > 0) {
            std::wstring wout(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, output.c_str(), static_cast<int>(output.size()), &wout[0], wlen);
            Log(L"Output:\n" + wout);
        }
    }

    if (timedOut) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Log(L"Command TIMED OUT after " + std::to_wstring(timeoutMs) + L" ms.");
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Log(L"Exit code: " + std::to_wstring(exitCode));
    return exitCode == 0;
}

int main() {
    InitLog(L"xToolsMenu_Install.log");
    Log(L"Install.exe started. IsAdmin=" + std::wstring(IsAdmin() ? L"true" : L"false"));

    if (!IsAdmin()) {
        if (ElevateSelf()) return 0;
        Log(L"Error: This installer must be run as Administrator.");
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
        Log(L"Error: xToolsMenu.dll not found in: " + baseDir.wstring());
        return 1;
    }

    fs::path registrationDir = baseDir;
    fs::path manifestPath = baseDir / "AppxManifest.xml";

    if (!fs::exists(manifestPath) && fs::exists(baseDir / "AppPackage" / "AppxManifest.xml")) {
        manifestPath = baseDir / "AppPackage" / "AppxManifest.xml";
    }

    if (!fs::exists(manifestPath)) {
        Log(L"Error: AppxManifest.xml not found.");
        return 1;
    }

    Log(L"Registering COM Server (Windows 11 Menu)...");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}\\InprocServer32", L"", dllPath.wstring());
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}\\InprocServer32", L"ThreadingModel", L"Apartment");

    Log(L"Registering Classic COM Server (Classic Menu)...");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}\\InprocServer32", L"", dllPath.wstring());
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}\\InprocServer32", L"ThreadingModel", L"Apartment");

    Log(L"Registering Classic Context Menu Handlers...");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");
    SetRegistryKeyString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\xToolsMenu", L"", L"{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}");

    Log(L"Resetting AppX Deployment Service...");
    RunCommand(L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"$ProgressPreference = 'SilentlyContinue'; Restart-Service AppXSvc -Force -ErrorAction SilentlyContinue\"", 15000);

    Log(L"Unregistering Sparse Package if exists...");
    RunCommand(L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"$ProgressPreference = 'SilentlyContinue'; Stop-Process -Name Launcher -Force -ErrorAction SilentlyContinue; Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage -ErrorAction SilentlyContinue\"");

    Log(L"Registering Sparse Package...");
    std::wstring addPackageCmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"$ProgressPreference = 'SilentlyContinue'; Add-AppxPackage -Register -ForceUpdateFromAnyVersion -Path '" + manifestPath.wstring() + L"' -ExternalLocation '" + registrationDir.wstring() + L"'\"";

    if (RunCommand(addPackageCmd)) {
        Log(L"Restarting Explorer to apply updates...");
        RunCommand(L"powershell.exe -NoProfile -NonInteractive -Command \"Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 500; if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }\"", 15000);

        Log(L"Registration completed successfully!");
        return 0;
    } else {
        Log(L"Error: Failed to register sparse package.");
        return 1;
    }
}
