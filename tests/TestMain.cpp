#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstring>
#include <iostream>
#include <string>

#include "cli/CommandTable.h"
#include "cli/TestAssert.h"

namespace {

void PrintList()
{
    for (const auto& cmd : Cli::AllCommands()) {
        std::cout << Cli::LabelName(cmd.label) << "\t" << cmd.name << "\n";
    }
}

int RunOne(const std::string& name)
{
    for (const auto& cmd : Cli::AllCommands()) {
        if (cmd.name != name) {
            continue;
        }

        if (cmd.label == Cli::Label::MEDIA && !Cli::MediaDataAvailable()) {
            std::cerr << "[SKIP] " << name << " 需要 data/ 目录\n";
            return 77; // CTest 约定的 skip 码
        }

        cu::ResetFailures();
        const int rc = cmd.run();
        if (rc != 0) {
            std::cerr << "[FAIL] " << name << " 返回 " << rc << "\n";
            return rc;
        }
        if (cu::FailureCount() != 0) {
            std::cerr << "[FAIL] " << name << " 断言失败 " << cu::FailureCount() << " 处\n";
            return 1;
        }
        std::cout << "[PASS] " << name << "\n";
        return 0;
    }

    std::cerr << "未知测试: " << name << "\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    // 注：Debug 下刻意不启用 CRT 内存泄漏检测（_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF)）。
    // 链接的 OpenCV/FFmpeg 在退出时留有全局单例/静态对象未析构，CRT 会在 main 返回后
    // 把它们全量误报为泄漏（连纯内存的 magic-enum 测试也报）。真正的内存问题排查
    // 改用 x64-asan preset（AddressSanitizer，能精确到本工程代码行）。

    SetConsoleOutputCP(65001);

    if (argc < 2) {
        std::cerr << "Usage: camera_utils_tests <test-name> | --list\n";
        return 2;
    }

    const std::string arg = argv[1];
    if (arg == "--list") {
        PrintList();
        return 0;
    }

    return RunOne(arg);
}
