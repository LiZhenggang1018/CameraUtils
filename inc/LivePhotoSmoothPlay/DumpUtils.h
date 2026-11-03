#ifndef DUMPUTILS_H
#define DUMPUTILS_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

// 辅助模板：检查类型是否支持 << 运算符
template<typename T, typename = void>
struct is_printable : std::false_type {};

template<typename T>
struct is_printable<T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};

template<typename T>
inline constexpr bool is_printable_v = is_printable<T>::value;

// 基础写入函数，确保所有参数都支持 << 运算符
template<typename T>
void write_value(std::ostream& os, T&& value) {
    static_assert(is_printable_v<std::remove_reference_t<T>>,
        "Type must support << operator");
    os << value;
}

// 主模板函数
template<typename... Args>
void Dump(const std::string& fileName, Args&&... args) {
    // 确保所有参数都支持 << 运算符
    static_assert((is_printable_v<std::remove_reference_t<Args>> && ...),
        "All arguments must support << operator");

    // 1. 写入文件
    {
        // 确保目录存在
        fs::path filePath = fileName;
        if (filePath.has_parent_path()) {
            fs::create_directories(filePath.parent_path());
        }

        std::ofstream outFile(fileName, std::ios::trunc);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing: " + fileName);
        }

        // 使用折叠表达式写入所有参数，用空格分隔
        bool first = true;
        ((outFile << (first ? (first = false, "") : " ") << std::forward<Args>(args)), ...);
        outFile << std::endl;

        if (!outFile) {
            throw std::runtime_error("Failed to write to file: " + fileName);
        }
    }

    // 2. 从文件读取并打印
    {
        // 检查文件是否存在
        if (!fs::exists(fileName)) {
            throw std::runtime_error("File not found after writing: " + fileName);
        }

        std::ifstream inFile(fileName);
        if (!inFile) {
            throw std::runtime_error("Failed to open file for reading: " + fileName);
        }

        std::string line;
        if (std::getline(inFile, line)) {
            std::cout << "Read from file '" << fileName << "': " << line << std::endl;
        }

        // 如果需要按类型重新解析，可以使用以下代码
        /*
        inFile.clear();
        inFile.seekg(0);

        std::cout << "Parsed values: ";
        ((std::cout << args << " "), ...);
        std::cout << std::endl;
        */
    }
}

// 更复杂的版本：支持不同类型，并正确读取回来
template<typename... Args>
void DumpDetailed(const std::string& fileName, Args&&... args) {
    // 检查所有参数是否可打印
    static_assert((is_printable_v<std::remove_reference_t<Args>> && ...),
        "All arguments must support << operator");

    // 1. 写入文件
    {
        // 创建目录（如果需要）
        fs::path filePath = fileName;
        if (filePath.has_parent_path()) {
            fs::create_directories(filePath.parent_path());
        }

        std::ofstream outFile(fileName, std::ios::trunc);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing: " + fileName);
        }

        // 写入每个参数，用空格分隔
        outFile << "Values: ";
        ((outFile << std::forward<Args>(args) << ' '), ...);
        outFile << "\nTypes: ";
        ((outFile << typeid(std::remove_reference_t<Args>).name() << ' '), ...);

        // 写入统计信息
        outFile << "\nCount: " << sizeof...(args);
        outFile << "\nFile size: " << fs::file_size(filePath) << " bytes";

        if (!outFile) {
            throw std::runtime_error("Failed to write to file: " + fileName);
        }
    }

    // 2. 读取文件并打印详细内容
    {
        std::cout << "\n=== File Content of '" << fileName << "' ===\n";

        std::ifstream inFile(fileName);
        if (!inFile) {
            throw std::runtime_error("Failed to open file for reading: " + fileName);
        }

        // 逐行读取并显示
        std::string line;
        int lineNum = 1;
        while (std::getline(inFile, line)) {
            std::cout << lineNum++ << ": " << line << std::endl;
        }

        // 显示文件信息
        std::cout << "\n=== File Information ===\n";
        try {
            auto fileSize = fs::file_size(fileName);
            auto lastWriteTime = fs::last_write_time(fileName);
            auto filePath = fs::absolute(fileName);

