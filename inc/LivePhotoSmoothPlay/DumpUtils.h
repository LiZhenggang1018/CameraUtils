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

            std::cout << "Absolute path: " << filePath << std::endl;
            std::cout << "File size: " << fileSize << " bytes" << std::endl;
            std::cout << "File exists: " << (fs::exists(fileName) ? "Yes" : "No") << std::endl;
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Error accessing file info: " << e.what() << std::endl;
        }
    }
}

// 测试函数
inline void testDumpFunctions() {
    try {
        std::cout << "=== Test Dump Function ===\n";

        // 测试基本类型
        Dump("test_basic.txt", 42, 3.14159, "Hello", 'A');

        std::cout << "\n=== Test DumpDetailed Function ===\n";

        // 测试详细版本
        DumpDetailed("test_detailed.txt",
            100,                    // int
            3.14159265359,          // double
            "World",                // const char*
            std::string("C++17"),   // std::string
            true,                   // bool
            static_cast<short>(10));// short

        // 测试带路径的文件
        DumpDetailed("logs/test_path/data.txt",
            "Timestamp:", 1234567890,
            "Message:", "Operation completed",
            "Status:", "OK");

        // 测试更多参数
        std::cout << "\n=== Test with Many Arguments ===\n";
        Dump("test_many.txt",
            "Argument", 1, "Argument", 2, "Argument", 3,
            "Argument", 4, "Argument", 5, "Argument", 6,
            "Argument", 7, "Argument", 8, "Argument", 9,
            "Argument", 10);

        // 测试错误处理（取消注释以下代码测试错误情况）
        // Dump("/invalid/path/file.txt", "test");  // 无法创建目录

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// 模板特化示例：为特定类型提供不同的处理
template<typename T>
void write_specialized(std::ostream& os, const std::vector<T>& vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) os << ", ";
        os << vec[i];
    }
    os << "]";
}

template<typename... Args>
void DumpWithVector(const std::string& fileName, Args&&... args) {
    static_assert((is_printable_v<std::remove_reference_t<Args>> && ...),
        "All arguments must support << operator");

    // 创建目录
    fs::path filePath = fileName;
    if (filePath.has_parent_path()) {
        fs::create_directories(filePath.parent_path());
    }

    // 写入文件
    std::ofstream outFile(fileName, std::ios::trunc);
    ((outFile << args << ' '), ...);
    outFile << std::endl;

    // 读取并显示
    std::ifstream inFile(fileName);
    std::string content;
    std::getline(inFile, content);
    std::cout << "Content: " << content << std::endl;

    // 显示文件统计
    if (fs::exists(fileName)) {
        std::cout << "File created: " << fileName
            << " (" << fs::file_size(fileName) << " bytes)" << std::endl;
    }
}

inline void TestDumpUtils() {
    std::cout << "C++17 Dump Function Example\n";
    std::cout << "============================\n\n";

    testDumpFunctions();

    //// 额外测试：使用vector等容器
    //std::cout << "\n=== Test with Containers ===\n";
    //std::vector<int> numbers = { 1, 2, 3, 4, 5 };
    //std::vector<std::string> words = { "apple", "banana", "cherry" };

    //DumpWithVector("test_containers.txt",
    //    "Numbers:", numbers,
    //    "Words:", words,
    //    "End");

}

#endif // !DUMPUTILS_H