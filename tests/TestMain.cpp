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
    for (const auto& cmd : cli::AllCommands()) {
        std::cout << cli::LabelName(cmd.label) << "\t" << cmd.name << "\n";
    }
}

int RunOne(const std::string& name)
{
    for (const auto& cmd : cli::AllCommands()) {
        if (cmd.name != name) {
            continue;
        }

        if (cmd.label == cli::Label::Media && !cli::MediaDataAvailable()) {
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
