#include <windows.h>
#include <filesystem>

namespace fs = std::filesystem;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileName(NULL, exePath, MAX_PATH))
    {
        fs::path root = fs::path(exePath).parent_path();
        fs::path settingsPath = root / "Settings.exe";

        if (fs::exists(settingsPath))
        {
            ShellExecute(NULL, L"open", settingsPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    }
    return 0;
}
