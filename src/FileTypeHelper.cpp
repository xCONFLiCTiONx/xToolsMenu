#include "FileTypeHelper.h"
#include <algorithm>
#include <map>

static std::map<std::wstring, FileTypeCategory> extensionMap = {
    // Images
    { L".jpg", FileTypeCategory::Images }, { L".jpeg", FileTypeCategory::Images }, { L".png", FileTypeCategory::Images },
    { L".gif", FileTypeCategory::Images }, { L".bmp", FileTypeCategory::Images }, { L".webp", FileTypeCategory::Images },
    { L".tif", FileTypeCategory::Images }, { L".tiff", FileTypeCategory::Images }, { L".ico", FileTypeCategory::Images },
    { L".svg", FileTypeCategory::Images }, { L".avif", FileTypeCategory::Images },

    // Text Files
    { L".txt", FileTypeCategory::TextFiles }, { L".log", FileTypeCategory::TextFiles }, { L".ini", FileTypeCategory::TextFiles },
    { L".cfg", FileTypeCategory::TextFiles }, { L".conf", FileTypeCategory::TextFiles },

    // Documents
    { L".pdf", FileTypeCategory::Documents }, { L".doc", FileTypeCategory::Documents }, { L".docx", FileTypeCategory::Documents },
    { L".xls", FileTypeCategory::Documents }, { L".xlsx", FileTypeCategory::Documents }, { L".ppt", FileTypeCategory::Documents },
    { L".pptx", FileTypeCategory::Documents }, { L".odt", FileTypeCategory::Documents }, { L".ods", FileTypeCategory::Documents },
    { L".odp", FileTypeCategory::Documents }, { L".rtf", FileTypeCategory::Documents },

    // Audio
    { L".mp3", FileTypeCategory::Audio }, { L".wav", FileTypeCategory::Audio }, { L".flac", FileTypeCategory::Audio },
    { L".ogg", FileTypeCategory::Audio }, { L".opus", FileTypeCategory::Audio }, { L".m4a", FileTypeCategory::Audio },
    { L".aac", FileTypeCategory::Audio }, { L".wma", FileTypeCategory::Audio },

    // Video
    { L".mp4", FileTypeCategory::Video }, { L".mkv", FileTypeCategory::Video }, { L".avi", FileTypeCategory::Video },
    { L".mov", FileTypeCategory::Video }, { L".webm", FileTypeCategory::Video }, { L".wmv", FileTypeCategory::Video },
    { L".m4v", FileTypeCategory::Video }, { L".mpeg", FileTypeCategory::Video }, { L".mpg", FileTypeCategory::Video },

    // Archives
    { L".zip", FileTypeCategory::Archives }, { L".7z", FileTypeCategory::Archives }, { L".rar", FileTypeCategory::Archives },
    { L".tar", FileTypeCategory::Archives }, { L".gz", FileTypeCategory::Archives }, { L".bz2", FileTypeCategory::Archives },
    { L".xz", FileTypeCategory::Archives },

    // Disk Images
    { L".iso", FileTypeCategory::DiskImages }, { L".img", FileTypeCategory::DiskImages }, { L".vhd", FileTypeCategory::DiskImages },
    { L".vhdx", FileTypeCategory::DiskImages }, { L".vmdk", FileTypeCategory::DiskImages }, { L".vdi", FileTypeCategory::DiskImages },
    { L".qcow", FileTypeCategory::DiskImages }, { L".qcow2", FileTypeCategory::DiskImages }, { L".vhx", FileTypeCategory::DiskImages },

    // Executables
    { L".exe", FileTypeCategory::Executables }, { L".msi", FileTypeCategory::Executables }, { L".com", FileTypeCategory::Executables },
    { L".scr", FileTypeCategory::Executables }, { L".bat", FileTypeCategory::Executables }, { L".cmd", FileTypeCategory::Executables },
    { L".ps1", FileTypeCategory::Executables }
};

FileTypeCategory FileTypeHelper::GetCategoryForExtension(const std::wstring& extension) {
    std::wstring lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);

    auto it = extensionMap.find(lowerExt);
    if (it != extensionMap.end()) {
        return it->second;
    }
    return FileTypeCategory::Other;
}

FileTypeCategory FileTypeHelper::GetCategoryForPath(const std::wstring& path) {
    std::filesystem::path p(path);
    return GetCategoryForExtension(p.extension().wstring());
}

std::wstring FileTypeHelper::GetCategoryName(FileTypeCategory category) {
    switch (category) {
        case FileTypeCategory::Images: return L"Images";
        case FileTypeCategory::TextFiles: return L"Text Files";
        case FileTypeCategory::Documents: return L"Documents";
        case FileTypeCategory::Audio: return L"Audio";
        case FileTypeCategory::Video: return L"Video";
        case FileTypeCategory::Archives: return L"Archives";
        case FileTypeCategory::DiskImages: return L"Disk Images";
        case FileTypeCategory::Executables: return L"Executables";
        case FileTypeCategory::Other: return L"Other";
        default: return L"Unknown";
    }
}

std::vector<FileTypeCategory> FileTypeHelper::GetAllCategories() {
    return {
        FileTypeCategory::Images,
        FileTypeCategory::TextFiles,
        FileTypeCategory::Documents,
        FileTypeCategory::Audio,
        FileTypeCategory::Video,
        FileTypeCategory::Archives,
        FileTypeCategory::DiskImages,
        FileTypeCategory::Executables,
        FileTypeCategory::Other
    };
}
