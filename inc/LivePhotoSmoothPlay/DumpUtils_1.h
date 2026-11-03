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