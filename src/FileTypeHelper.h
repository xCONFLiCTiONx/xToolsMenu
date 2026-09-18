#pragma once
#include <string>
#include <vector>
#include <filesystem>

enum class FileTypeCategory {
    Images = 1 << 0,
    TextFiles = 1 << 1,
    Documents = 1 << 2,
    Audio = 1 << 3,
    Video = 1 << 4,
    Archives = 1 << 5,
    DiskImages = 1 << 6,
    Executables = 1 << 7,
    Other = 1 << 8,
    All = 0xFFFF
};

class FileTypeHelper {
public:
    static FileTypeCategory GetCategoryForExtension(const std::wstring& extension);
    static FileTypeCategory GetCategoryForPath(const std::wstring& path);
    static std::wstring GetCategoryName(FileTypeCategory category);
    static std::vector<FileTypeCategory> GetAllCategories();
};
