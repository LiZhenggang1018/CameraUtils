#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef _DEBUG
#include <crtdbg.h>
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
#ifdef _DEBUG
    // Route the leak report (a _CRT_WARN message) to stderr so it shows up in
    // the console and under ctest; otherwise it goes to the debugger only.
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);

    // Debug 构建下启用 CRT 内存泄漏检测：程序退出时自动 dump 未释放的内存块。
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

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
