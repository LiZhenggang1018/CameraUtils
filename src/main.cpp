#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "CoreUtils.h"
#include "cli/CommandTable.h"

using namespace std;

namespace
{

void PrintUsage(const char* exeName)
{
    std::cout << "Usage: " << exeName << " <command> [options]\n\n"
              << "Commands:\n";

    size_t width = 0;
    for (const auto& cmd : cli::AllCommands())
    {
        width = std::max(width, cmd.name.size());
    }

    const char* currentLabel = nullptr;
    for (const auto& cmd : cli::AllCommands())
    {
        const char* label = cli::LabelName(cmd.label);
        if (currentLabel == nullptr || std::string(currentLabel) != label)
        {
            std::cout << "\n  [" << label << "]\n";
            currentLabel = label;
        }
        std::cout << "    " << std::left << std::setw(static_cast<int>(width) + 2)
                  << cmd.name << cmd.description << "\n";
    }

    std::cout << "\nOptions:\n"
              << "    -l, --list    列出所有命令\n"
              << "    -h, --help    显示本帮助\n"
              << "\n无参数运行时进入交互式菜单。\n";
}

bool ParseLabel(const std::string& text, cli::Label& out)
{
    for (auto label : {cli::Label::Unit, cli::Label::Media, cli::Label::Gui})
    {
        if (text == cli::LabelName(label))
        {
            out = label;
            return true;
        }
    }
    return false;
}

/// 交互式菜单：按模块分组列出命令，输入编号执行
int RunMenu()
{
    const auto& commands = cli::AllCommands();
    const bool hasMedia = cli::MediaDataAvailable();

    std::cout << "CameraUtils 交互菜单\n";
    if (!hasMedia)
    {
        std::cout << "[WARN] 未找到 data/ 目录，media 类命令可能失败\n";
    }
    std::cout << "\n";
    std::cout << std::setw(4) << "No."
              << "  " << std::setw(6) << "类别"
              << "  " << "命令\n";
    std::cout << std::string(72, '-') << "\n";

    for (size_t i = 0; i < commands.size(); ++i)
    {
        const auto& cmd = commands[i];
        std::cout << std::setw(4) << (i + 1)
                  << "  " << std::setw(6) << cli::LabelName(cmd.label)
                  << "  " << cmd.name << "  -  " << cmd.description << "\n";
    }
    std::cout << std::string(72, '-') << "\n";
    std::cout << "输入编号执行，输入 q 退出: " << std::flush;

    std::string line;
    if (!std::getline(std::cin, line))
    {
        return 0;
    }

    if (line == "q" || line == "Q")
    {
        return 0;
    }

    char* end = nullptr;
    const long index = std::strtol(line.c_str(), &end, 10);
    if (end == line.c_str() || index < 1 ||
        static_cast<size_t>(index) > commands.size())
    {
        std::cerr << "无效编号: " << line << "\n";
        return 2;
    }

    const auto& cmd = commands[static_cast<size_t>(index) - 1];
    std::cout << "\n>>> " << cmd.name << " : " << cmd.description << "\n\n";
    return cmd.run();
}

const cli::Command* FindCommand(const std::string& name)
{
    for (const auto& cmd : cli::AllCommands())
    {
        if (cmd.name == name)
        {
            return &cmd;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    SetConsoleOutputCP(65001); // 控制台输出切换为 UTF-8

    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty())
    {
        return RunMenu();
    }

    const std::string& first = args[0];

    if (first == "-h" || first == "--help")
    {
        PrintUsage(argv[0]);
        return 0;
    }

    if (first == "-l" || first == "--list")
    {
        for (const auto& cmd : cli::AllCommands())
        {
            std::cout << cli::LabelName(cmd.label) << "\t" << cmd.name << "\t"
                      << cmd.description << "\n";
        }
        return 0;
    }

    if (first == "--label")
    {
        if (args.size() < 2)
        {
            std::cerr << "--label 需要参数（unit | media | gui）\n";
            return 2;
        }
        cli::Label wanted{};
        if (!ParseLabel(args[1], wanted))
        {
            std::cerr << "未知类别: " << args[1] << "\n";
            return 2;
        }
        for (const auto& cmd : cli::AllCommands())
        {
            if (cmd.label == wanted)
            {
                std::cout << cmd.name << "\n";
            }
        }
        return 0;
    }

    const cli::Command* cmd = FindCommand(first);
    if (cmd == nullptr)
    {
        std::cerr << "未知命令: " << first << "\n\n";
        PrintUsage(argv[0]);
        return 2;
    }

    if (cmd->label == cli::Label::Media && !cli::MediaDataAvailable())
    {
        std::cerr << "命令 " << cmd->name << " 需要 data/ 目录，未找到\n";
        return 3;
    }

    return cmd->run();
}
