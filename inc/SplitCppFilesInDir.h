#ifndef SPLITFILESINDIR_H
#define SPLITFILESINDIR_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

void SplitCppFilesInDir(std::string_view inDir, std::string_view outDir)
{
    constexpr std::size_t kMaxLines = 150;

    std::error_code ec;
    const fs::path inPath(inDir);
    const fs::path outPath(outDir);

    if (!fs::is_directory(inPath, ec))
        return;

    // 确保输出目录存在
    fs::create_directories(outPath, ec);

    for (const auto& entry : fs::directory_iterator(inPath, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file(ec))
            continue;

        // 只处理 .h / .cpp（扩展名不区分大小写）
        std::string ext = entry.path().extension().string();
        for (auto& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".h" && ext != ".cpp")
            continue;

        // 读入整个文件
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in)
            continue;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // 按行切分，保留原始换行符
        std::vector<std::string> lines;
        lines.reserve(content.size() / 32 + 1);
        std::size_t pos = 0;
        while (pos < content.size())
        {
            auto nl = content.find('\n', pos);
            if (nl == std::string::npos)
            {
                lines.emplace_back(content.substr(pos));
                break;
            }
            lines.emplace_back(content.substr(pos, nl - pos + 1));
            pos = nl + 1;
        }

        const fs::path originalPath = entry.path();
        const std::string stem = originalPath.stem().string();
        const std::string extStr = originalPath.extension().string();

        // 场景 1：≤150 行 —— 原样拷贝到输出目录
        if (lines.size() <= kMaxLines)
        {
            const fs::path target = outPath / originalPath.filename();

            // inDir == outDir 时会指向同一文件，直接跳过以免自我覆盖
            std::error_code eqEc;
            if (fs::equivalent(originalPath, target, eqEc) && !eqEc)
                continue;

            std::error_code cpEc;
            fs::copy_file(originalPath, target, fs::copy_options::overwrite_existing, cpEc);
            continue;
        }

        // 场景 2：>150 行 —— 按 150 行分片写入输出目录
        std::size_t partIndex = 0;
        for (std::size_t start = 0; start < lines.size(); start += kMaxLines, ++partIndex)
        {
            const fs::path target =
                (partIndex == 0) ? outPath / originalPath.filename() // My.h（前 150 行）
                                 : outPath / (stem + "_" + std::to_string(partIndex) + extStr);

            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            if (!out)
                continue;

            const std::size_t end = std::min(start + kMaxLines, lines.size());
            for (std::size_t i = start; i < end; ++i)
                out << lines[i];
        }
    }
}

inline void SplitCppFilesInDirTest()
{
    std::string_view inDir = "D:/ThirdPartyLib/Split/in";
    std::string_view outDir = "D:/ThirdPartyLib/Split/out";
    SplitCppFilesInDir(std::string_view(inDir), std::string_view(outDir));
}
#endif // !SPLITFILESINDIR_H
