#ifndef FILESUTILS_H
#define FILESUTILS_H

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "CheckUtils.h"

// Get Filenames in dir
inline std::vector<std::string> GetFilesInDir(const std::string& dir, bool recursive = true)
{
    std::vector<std::string> filePaths;
    std::filesystem::path folderPath(dir);
    CHECK_AND_RETURN_RET(!std::filesystem::exists(folderPath), filePaths);
    CHECK_AND_RETURN_RET(!std::filesystem::is_directory(folderPath), filePaths);
    if (recursive)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file())
            {
                filePaths.push_back(entry.path().filename().string());
            }
        }
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(folderPath))
        {
            if (entry.is_regular_file())
            {
                filePaths.push_back(entry.path().filename().string());
            }
        }
    }

    return filePaths;
}

// Check if File is in dir
inline bool IsFileInDir(const std::string& file, const std::string& folder)
{
    std::vector<std::string> files = GetFilesInDir(folder);
    return std::find(files.cbegin(), files.cend(), file) != files.cend();
}

#endif // FILESUTILS_H
