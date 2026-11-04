#ifndef CONCATCPPFILESINDIR_H
#define CONCATCPPFILESINDIR_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

inline std::string ToLower(std::string s)
{
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// 尝试把 stem 解析为 "<base>_<N>"，其中 N 为正整数（无前导 0）。
// 解析成功返回 true，base 为前缀，part 为 N。
inline bool ParsePartSuffix(const std::string& stem, std::string& base, int& part)
{
    auto pos = stem.rfind('_');
    if (pos == std::string::npos || pos == 0)
        return false;

    std::string numStr = stem.substr(pos + 1);
    if (numStr.empty() || numStr.size() > 9)
        return false; // 防溢出
    if (numStr.size() > 1 && numStr[0] == '0')
        return false; // 不允许 _01

    int n = 0;
    for (char c : numStr)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
        n = n * 10 + (c - '0');
    }
    if (n < 1)
        return false;

    base = stem.substr(0, pos);
    part = n;
    return true;
}

} // namespace

inline void ConcatCppFilesInDir(std::string_view inDir, std::string_view outDir)
{
    std::error_code ec;
    const fs::path inPath(inDir);
    const fs::path outPath(outDir);

    if (!fs::is_directory(inPath, ec))
        return;
    fs::create_directories(outPath, ec);

    // 1) 收集 inDir 下所有 .h / .cpp 文件
    struct Entry
    {
        fs::path path;
        std::string stem;     // 不含扩展名的文件名
        std::string ext;      // 原始扩展名（含点）
        std::string extLower; // 小写扩展名（含点），用于分类比较
        std::string baseStem; // 所属原始文件的 stem
        int part = 0;         // 0 表示原始文件本身；N 表示第 N 片
    };

    std::vector<Entry> entries;
    for (const auto& de : fs::directory_iterator(inPath, ec))
    {
        if (ec)
            break;
        if (!de.is_regular_file(ec))
            continue;

        std::string ext = de.path().extension().string();
        std::string extLower = ToLower(ext);
        if (extLower != ".h" && extLower != ".cpp")
            continue;

        Entry e;
        e.path = de.path();
        e.stem = de.path().stem().string();
        e.ext = ext;
        e.extLower = extLower;
        entries.push_back(std::move(e));
    }

    // 2) 建立 (extLower, stem) 查询表，用于判断某个候选 base 是否"作为文件存在"
    std::unordered_set<std::string> stemSet;
    stemSet.reserve(entries.size() * 2);
    for (const auto& e : entries)
        stemSet.insert(e.extLower + "\x01" + e.stem);

    // 3) 确定每个文件的 baseStem / part
    //    解析出 "<base>_<N>" 且 inDir 中确实存在 "<base>.<ext>" 时，视为分片；
    //    否则视为独立文件（baseStem = 自身 stem, part = 0）。
    //    这样既能正确处理 My.h / My_1.h / My_2.h，
    //    也能正确处理原文件名本来就以 "_数字" 结尾（如 Foo_1.h）被分割后的情况。
    for (auto& e : entries)
    {
        std::string base;
        int part = 0;
        if (ParsePartSuffix(e.stem, base, part) && stemSet.count(e.extLower + "\x01" + base))
        {
            e.baseStem = base;
            e.part = part;
        }
        else
        {
            e.baseStem = e.stem;
            e.part = 0;
        }
    }

    // 4) 按 (extLower, baseStem) 分组
    struct Group
    {
        std::string extLower;
        std::string baseStem;
        std::string outExt; // 输出时使用的扩展名（取 part0 的）
        std::vector<Entry*> parts;
    };
    std::unordered_map<std::string, Group> groups;

    for (auto& e : entries)
    {
        std::string key = e.extLower + "\x01" + e.baseStem;
        auto it = groups.find(key);
        if (it == groups.end())
        {
            Group g;
            g.extLower = e.extLower;
            g.baseStem = e.baseStem;
            g.outExt = e.ext;
            g.parts.push_back(&e);
            groups.emplace(std::move(key), std::move(g));
        }
        else
        {
            it->second.parts.push_back(&e);
            if (e.part == 0)
                it->second.outExt = e.ext;
        }
    }

    // 5) 逐组处理
    for (auto& [key, g] : groups)
    {
        std::sort(g.parts.begin(),
                  g.parts.end(),
                  [](const Entry* a, const Entry* b) { return a->part < b->part; });

        const fs::path target = outPath / (g.baseStem + g.outExt);

        // 未分割（只有一个 part0 文件）→ 直接拷贝
        if (g.parts.size() == 1 && g.parts[0]->part == 0)
        {
            std::error_code eqEc;
            if (fs::equivalent(g.parts[0]->path, target, eqEc) && !eqEc)
                continue; // inDir == outDir，源和目标是同一文件

            std::error_code cpEc;
            fs::copy_file(g.parts[0]->path, target, fs::copy_options::overwrite_existing, cpEc);
            continue;
        }

        // 已分割 → 按 part 顺序拼接（先全部读入内存，避免 inDir==outDir 时覆盖 part0）
        std::string merged;
        bool readOk = true;
        for (auto* p : g.parts)
        {
            std::ifstream in(p->path, std::ios::binary);
            if (!in)
            {
                readOk = false;
                break;
            }
            merged.append(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        if (!readOk)
            continue;

        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out)
            continue;
        if (!merged.empty())
            out.write(merged.data(), static_cast<std::streamsize>(merged.size()));
    }
}

inline void ConcatCppFilesInDirTest()
{
    std::string_view inDir = "D:/ThirdPartyLib/Split/out";
    std::string_view outDir = "D:/ThirdPartyLib/Split/ConcatOut";
    ConcatCppFilesInDir(std::string_view(inDir), std::string_view(outDir));
}

#endif // !CONCATCPPFILESINDIR_H
