# AGENTS.md

本文件记录本仓库的固定工作约定，供 AI 助手在本项目中工作时遵循。

## 环境前提

- 操作系统：Windows 11；终端：VSCode 集成终端（PowerShell 5.1）。
- **当前运行环境必然处于公司内网**，这是既定前提，无需再确认。
- `git` 已配置内网代理 `http.proxy=http://10.110.188.25:80`，**必须保留，不要取消或修改**。
  在非公司网络下推送失败属于预期外情况，正常情况下不会发生。

## 构建

Ninja + MSVC 需要在能识别 `cl.exe` 的环境中运行，先加载 `vcvars64.bat`：

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake --preset x64-debug
cmake --build --preset x64-debug
```

已配置过之后，日常增量构建可直接：

```bat
call "...\vcvars64.bat"
ninja -C out\build\x64-debug
```

注意：PowerShell 中直接写含空格的路径必须用调用运算符并正确加引号，复杂命令建议写成 `.bat` 脚本再执行。

### 两套工具链并存（重要）

本机装有两个 MSVC 工具集，两份构建产物各自独立，**告警与错误不一定互通**：

| | 助手使用 | 开发者使用 |
|---|---|---|
| 配置目录 | `out/build/x64-debug` | `out/build/x64-release` |
| 工具集 | `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.52.37025` | `D:\Program Files\visual_studio\VC\Tools\MSVC\14.44.35207`（VS 2022 Enterprise 17.14） |
| cl 版本 | 19.52 | 19.44 |
| CMake | 4.2.3（`D:\Program Files\CMake`） | 3.31.6（VS 自带） |

因此：**助手编译通过不等于开发者 VS 2022 编译通过**，反之亦然。报告构建/测试结果时须说明用的是哪套配置，
不要声称已验证开发者那侧的构建。

### 排查构建参数时的坑

`out/build/<preset>/build.ninja` 里所有目标共用同一套 `FLAGS` 语法，第三方依赖（`_deps/`，如 exiv2）
的编译单元排在文件前面。用 `[regex]::Match` 或 `grep -m1` 取"第一个 FLAGS"会**误取到第三方目标**。

正确做法是按目标前缀区分：本工程目标（`src/CMakeFiles/`、`tests/CMakeFiles/`）才代表项目配置。
实测结论：本工程统一为 `-std:c++20`（来自根 `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD 20`），
第三方 `_deps` 目标为 `-std:c++17`，两者不应混淆。

同理，本工程 Debug 目标为 `/Ob0 /Od /RTC1 -MDd`（标准 Debug），不存在 `/Ox` 优化；
看到 `/Ox` 说明取到的是第三方目标。

## 测试

```bat
ctest --test-dir out/build/x64-debug -L unit --output-on-failure
ctest --test-dir out/build/x64-debug -L media --output-on-failure
```

单元测试聚合在 `camera_utils_tests` 中：

```bat
camera_utils_tests.exe --list
camera_utils_tests.exe magic-enum
```

## Git 提交与推送策略

### 提交

- **单个文件改动：直接提交。**
- **多个文件改动：由于网络限制，逐个文件依次提交**（每个文件单独 `git add` + `commit`），
  避免一次性大提交导致的推送失败与重试成本。
- 提交信息使用中文，沿用仓库既有的 Conventional Commits 前缀风格：
  `feat(scope):` / `fix(scope):` / `refactor(scope):` / `build:` / `ci:` / `docs:`。
- 提交信息中不要包含密钥、令牌或内网地址等敏感内容。
- 不要在未明确要求时改动 `.git/config`、hooks 或使用 `--force`。

### 推送

- 推送方式固定为：`git push origin main`。
- **公司网络与代理是既定前提，直接推送，不要就网络环境或代理配置向用户提问。**
- 多文件场景下按上面的策略逐次提交、逐次推送。
- 推送后确认同步状态：`git rev-list --left-right --count origin/main...main` 应输出 `0 0`。

## 编码规范（必须遵守）

编写或修改 C++ 代码前必须先阅读以下规范文档（存放于本地 `references/` 目录，该目录不入库）：

- `references/OpenHarmony-cpp-coding-style-guide.md` —— 命名、格式、注释、头文件、类、函数、C++ 特性
- `references/OpenHarmony-c-cpp-secure-coding-guide.md` —— C/C++ 安全编码（内存、资源、格式化字符串、路径校验等）
- `references/OpenHarmony-compile-rule.md` —— 编译选项规范

**执行要求**

- 规范中标记为「规则」的条目为强制项，「建议」条目尽量遵守。
- 格式由 `.clang-format` 落地，已按规范配置（K&R 大括号、行宽 120、4 空格缩进、`public:` 与 `class` 对齐）。
  提交前务必执行：`clang-format -i --style=file <文件>`。
- 命名由 `.clang-tidy` 的 `readability-identifier-naming` 落地：类型/函数用大驼峰，
  变量用小驼峰，类成员变量带 `_` 后缀，全局变量带 `g_` 前缀，常量/枚举值/宏用全大写。
- **约束范围：新增代码与本次修改的代码片段。存量代码不在本次范围内**，避免产生无关的大规模改动。
- 注释：新增注释用英文；存量中文注释保持原样，不要主动翻译或改写。
- 禁止在头文件中使用 `using namespace`（规范 6.1.1），新增代码一律用完整限定名（如 `cv::Mat`）。

## 编码约定

- 源码与文档统一使用 **UTF-8**（带 BOM 的文件保持原样，如已有 `inc` 下若干头文件）。
- 换行符保持文件原有风格（仓库 `core.autocrlf=true`）。
- 修复乱码时，先定位真实字节再改，禁止凭显示结果猜测。

### Windows 编码陷阱（重要）

PowerShell 5.1 控制台与管道按 GBK 解码，直接查看或管道传递中文会显示为乱码，
**这只是显示层问题，不代表文件损坏**。校验文件真实内容时使用以下方式：

- 用 `read` 工具读取文件；
- 或用 .NET 直接按 UTF-8 解码字节：
  `[System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($path))`
- 校验已提交内容用 `git cat-file blob HEAD:<path>`，并通过 `ProcessStartInfo`
  重定向 `BaseStream` 取原始字节，避免经 PowerShell 管道转码。

不要把 `.ps1` 脚本中的中文字面量直接写入无 BOM 的脚本文件（会被按 GBK 解析而报错），
需要中文内容时改为从独立 UTF-8 文本文件读取。
