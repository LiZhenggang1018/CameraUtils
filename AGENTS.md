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

### clang-tidy 与 MSVC 工具集必须配对（重要）

MSVC STL 会在 `yvals_core.h` 里硬性校验 Clang 版本，版本不足时报 `STL1000`
（错误位置落在 STL 头里，看起来像代码问题，实则工具链不匹配）：

| MSVC 工具集 | `yvals_core.h` 要求的最低 Clang |
|---|---|
| 14.44（`out/build/x64-release`） | **19** |
| 14.52（`out/build/x64-debug`） | **22** |

本机只有 `D:\Program Files\visual_studio\VC\Tools\Llvm\x64\bin\clang-tidy.exe`（LLVM 19.1.5），
所以**跑 clang-tidy 必须配合 x64-release 的 14.44 环境**：

```bat
call "D:\Program Files\visual_studio\VC\Auxiliary\Build\vcvars64.bat"
"D:\Program Files\visual_studio\VC\Tools\Llvm\x64\bin\clang-tidy.exe" -p out/build/x64-release <文件>
```

用 14.52 的 vcvars 去跑会得到满屏 `STL1000` 与 magic_enum 断言错误，**这不是代码缺陷**。

另一个坑：clang-cl 同时定义 `__clang__` 与 `_MSC_VER`，magic_enum v0.9.8 的宏判断按
`__clang__` 优先分支取偏移，而 clang-cl 下 `__PRETTY_FUNCTION__` 实为 MSVC 格式
（`sizeof` 为 42，与 `__FUNCTION__` 的 25 不同），导致枚举反射范围为空、触发
`is_reflected_v` 断言。`tidy.yml` 仅对 `src/main.cpp` 追加
`-DMAGIC_ENUM_NO_CHECK_REFLECTED_ENUM` 绕过该断言，**该宏绝不可用于真实构建**。

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

## CI 约定

- 工作流只有 `ci-windows.yml`（构建 + 单测）、`style.yml`（clang-format）、`tidy.yml`（clang-tidy）。
  曾有一个 `ci.yml`，用 `cmake -S . -B build` 且未准备 MSVC 环境与 OpenCV，已删除，勿再引入。
- **OpenCV 必须含 contrib**：`inc/OpenCVCommon.h` 依赖 `optflow.hpp` 与 `ximgproc.hpp`，而
  `CMakeLists.txt` 用 `optflow.hpp` 是否存在来决定走预编译还是源码构建。官方
  `opencv-<ver>-windows.exe` 不含 contrib，用它会被判定拒绝并退化为源码构建（等于白下载）。
  两个 workflow 统一通过 `.github/actions/setup-opencv` 获取，该 action 构建并缓存
  `C:/opencv-contrib`，`config` 输入区分 Debug/Release，缓存 key 含 config。
- `setup-opencv` 的 `modules` 默认值必须与 `CMakeLists.txt` 的
  `CAMERA_UTILS_OPENCV_MODULES` + `CAMERA_UTILS_OPENCV_CONTRIB_MODULES` 保持一致，
  否则缓存命中的树无法满足链接。
- `.github/` 下的文件统一使用 **CRLF** 行尾。

### CI 测试归类（重要，防「假通过」）

CI 的 `ci-windows.yml` 只跑 `ctest -L unit`。因此 **unit 组只应放真正的、纯内存、带断言的测试**：

- **demo（`*Demo.h`，`Label::GUI`）不进 ctest**：GUI demo 本就不注册，无需处理。
- **数值输出 demo（如 `sfm-demo`）不算单元测试**：它们无断言、纯打印，必须留在
  `CommandTable.h` 供手动运行，但**不能列入 `tests/CMakeLists.txt` 的 `_unit_tests`**（否则「假通过」）。
- **依赖 `data/` 素材的用例必须归 `Label::MEDIA`**（如 `exif` 读 `data/HdrMerge/normal.jpg`）：
  归 UNIT 会因 CI 无 `data/` 而「假通过」（无断言时 `FailureCount()==0` 返回 0）；归 MEDIA 后
  CI 会走 `RunOne` 的 `[SKIP] return 77` 正确跳过，并列入 `_media_tests`。
- **media 组只在本地 `data/` 存在时注册**（`tests/CMakeLists.txt` 的 `if(EXISTS data)`），CI 无
  素材时整组不注册，不会失败。

### CI 环境风险（非阻塞，但需留意）

- **MSVC 版本不一致**：CI 用 `windows-2022` runner 自带的 MSVC，与本机两套工具集（开发者 14.44 /
  助手 14.52）都可能不同。本地编译通过 ≠ CI 通过，这是固有风险。
- **Debug 无优化下的性能**：`dense-flow`（DeepFlow 后端）与 `mfnr`（内含 ImageRegistrator 配准）
  在 Debug（`/Ob0 /Od`）下偏慢，`unit` 组超时 120s，若 CI 机器较慢可能超时；本地实测 43 个 unit
  总耗时约 44s。若单独用例超时，需调大该用例 TIMEOUT 或减小合成图尺寸。

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

### 推送的根因规则（必须遵守）

**`git push` 推送任意提交时，会连带远端没有的所有祖先提交一起打包发送。**

- 本地相对远端领先 N 个提交时，即使只推第 1 个提交（`git push origin <最早sha>:main`），
  git 实际仍会把 N 个提交的内容全部打包上传 → 单次 POST 过大 → 被网关 403 拒绝。
- **因此「领先 N 个时逐个 push」是无效的**，必须先消除领先，再逐个提交+推送。

**正确做法（每个提交必须保证 push 前本地仅领先 1 个）：**

1. 若已有多个未推提交堆积，先 `git reset --soft origin/main` 把改动退回暂存区
   （工作区文件不丢），再 `git reset` 清空暂存区，让改动回到工作区。
2. 按「先小文件、后大文件」的顺序，对**每个文件**依次执行：
   `git add <单个文件>` → `git commit -m "..."` → `git push origin main`。
3. 每一步 push 时本地都只领先 1 个提交，pack 最小，可稳定通过网关。

**约束：**

- 一次 `git add` 只能 add 单个文件，禁止一次 add 多个文件后再 commit/push。
- 一次 `git commit` 只提交单个文件（多文件改动必须拆成多个单文件提交）。
- 每个单文件提交后立即 `git push origin main`，不要攒多个提交再推。
- **每次更新完 `AGENTS.md` 后，也要按单文件规则及时提交并推送**（`git add AGENTS.md` → `git commit` → `git push origin main`），不要攒着。

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

## 目录结构约定（按章节归档）

- **可复用算法库**：`inc/<章节目录>/`（接口）+ `src/<章节目录>/`（实现），章节目录名对齐书中章节
  （如第 3 章 → `ImageProcessing`）。
- **公共工具类**：`inc/CommonUtils/`（不含 OpenCV/FFmpeg/exiv2 的通用工具，如
  `CheckUtils.h`、`ThreadPool.h`、`GeneralFactory.h` 等）+ `tests/CommonUtils/`（对应测试用例）。
  `CoreUtils.h` 作为纯标准库聚合头保留在 `inc/` 根目录。
- **测试用例**：`tests/<章节目录>/`，每章的测试用例（`*Test.h` / `*Demo.h`）统一放在
  该章对应子目录下，**不散落在 `tests/` 根目录**。已在用的章节目录：
  `tests/ImageProcessing/`、`tests/FeatureMatching/`、`tests/HdrMerge/`、`tests/LivePhotoSmoothPlay/`。
- 通用测试框架（`tests/cli/`）保留在 `tests/` 根目录。
- 测试文件 include guard 用 `TESTS_<章节目录>_<文件名>_H` 形式
  （如 `TESTS_IMAGEPROCESSING_SOBELTEST_H`），`CommandTable.h` 里用
  `#include "<章节目录>/<文件名>.h"` 引用。

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

**禁止用 PowerShell 文本命令修改含中文的 UTF-8 文件（会导致真实损坏，不是显示问题）。**
具体：`Get-Content` + `-replace`/`Set-Content`、`Out-File`、`Set-Content` 等按 GBK 读入
UTF-8 文件后写回，会把中文永久破坏（曾因此损坏 `tests/ImageProcessing/` 下 8 个文件的注释）。
改文本一律用 `read`/`edit` 工具，或用 C# 脚本显式指定 `Encoding.UTF8` 读写。

### 编码统一约定（防乱码）

- **新文件一律 UTF-8 无 BOM + LF 换行**（`.editorconfig` 已声明 `charset = utf-8` + `end_of_line = lf`）。
- **换行符**：源码/文档/配置统一 LF（`.gitattributes` 已声明 `eol=lf`）；`.github/` 下与 `*.ps1` 保持 CRLF。
- **存量文件**：保持原有风格，不强制迁移（避免大规模无关 diff）。
- **提交前校验**：运行 `powershell -ExecutionPolicy Bypass -File scripts/check-encoding.ps1`，
  脚本通过检测 `U+FFFD` 替换字符与私用区字符 `U+E000-U+F8FF`（GBK 错转 UTF-8 的典型特征）来发现乱码；
  退出码非 0 表示有乱码，须用 `read`/`edit` 修复后再提交。
