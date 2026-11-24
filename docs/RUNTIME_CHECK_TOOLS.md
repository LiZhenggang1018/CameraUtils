# 运行时检查工具使用指南（ASan / _CRTDBG / Application Verifier）

本项目是 C++20 + OpenCV 的图像处理工程，内存密集 + 多线程，运行时错误排查尤其重要。
本文档汇总已集成的运行时检查工具及使用方法。

## 1. ASan（地址消毒器，AddressSanitizer）

**查什么**：内存越界（读/写）、use-after-free、栈溢出、部分泄漏。

**已集成**：`CMakePresets.json` 提供了 `x64-asan` preset（基于 x64-debug + `/fsanitize=address`）。

**使用**（命令行）：

```bat
:: 必须在 x64 环境（vcvars64）下配置，否则 VS 默认 x86 环境会找不到 x64 OpenCV
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake --preset x64-asan
cmake --build --preset x64-asan --target camera_utils_tests

:: 运行（ASan 会在检测到内存错误时立即报告并崩溃）
.\out\build\x64-asan\tests\camera_utils_tests.exe <test-name>
```

**注意**：
- ASan 会显著变慢（约 2x）并增大内存占用，只用于排查，不作为日常构建。
- 语法是 `/fsanitize=address`（等号），**不是冒号** `/fsanitize:address`（冒号会被 cl 报 D9002 忽略）。
- `/fsanitize=address` 与 Debug 的 EditAndContinue（`/ZI`）冲突（报 D8016），`CMakeLists.txt` 已
  在检测到 `fsanitize` 时自动改用 `ProgramDatabase`（`/Zi`）。
- ASan 运行库 `clang_rt.asan*.dll` 已在构建期自动复制到 exe 同目录（`camera_utils_copy_runtime_dlls`），
  运行时**无需手动设 PATH**。

### VS 本地验证经验（重要）

- **launch.vs.json 启动配置在本机 VS 17.14 不被「调试目标」下拉框识别**（可能因 `.slnx` 新格式）。
  最可靠的验证方式是「VS 内置终端 + 完整路径命令」：

  ```powershell
  # VS 菜单「视图 → 终端」，粘贴执行（用绝对路径 + 调用运算符 &）
  & "C:\Users\11186484\CameraUtils\out\build\x64-asan\tests\camera_utils_tests.exe" asan-heap-overflow
  ```

  直接运行已构建好的 exe 不涉及编译，所以不受 VS 终端 x86 架构影响，一条命令出结果。
- 命令行里用相对路径运行 exe 时，PowerShell 必须加 `.\` 前缀，否则会被当成模块名解析报错。
- 已实测生效，报错形如：
  ```
  ERROR: AddressSanitizer: heap-buffer-overflow
  SUMMARY: ... CppRuntimeCheckDemo.h:29 in CppRuntimeCheck::AsanHeapOverflow
  ```

### 验证 demo（tests/CppRuntimeCheck/CppRuntimeCheckDemo.h）

| 命令 | 需配置 | 触发错误 |
|---|---|---|
| `asan-heap-overflow` | `x64-asan` | 堆越界写 |
| `asan-use-after-free` | `x64-asan` | free 后访问 |
| `asan-stack-overflow` | `x64-asan` | 栈越界写 |
| `crtdbg-leak` | `x64-Debug` | 内存泄漏 |
| `ubsan-undefined` | 需 clang-cl | 有符号整数溢出 |
| `gs-cookie-overflow` | 任意（`/GS` 默认开） | 栈越界覆盖 cookie，函数返回时报错 |
| `cfi-indirect-call` | `x64-hardened` | 演示 CFG 保护的间接调用（防御性，不报错） |

## 2. _CRTDBG（MSVC CRT 调试堆，内存泄漏检测）

**查什么**：内存泄漏（程序退出时报告未释放的堆块）。

**已集成**：`tests/TestMain.cpp` 在 `_DEBUG` 构建下自动启用：

```cpp
#ifdef _DEBUG
#include <crtdbg.h>
#endif
// main 开头
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
```

**使用**：直接跑 Debug 版测试即可，程序正常退出时若有泄漏会自动 dump 块地址和大小。

**注意**：
- 比 ASan 轻量得多（几乎无性能损耗），适合日常 Debug 快速排查泄漏。
- 只报泄漏，不报越界/use-after-free（那要 ASan）。
- 若泄漏报告干扰（如第三方库的「假泄漏」），可用 `_CrtSetBreakAlloc(块号)` 定位。

## 3. Application Verifier（Windows 原生运行时验证器）

**查什么**：句柄泄漏、堆损坏、临界区错误、异常处理错误——ASan 覆盖不到的 Windows 特有资源问题。

**使用**（GUI）：

1. 打开「Application Verifier」（Windows SDK 自带，`appverif.exe`；或 VS 的
   Tools → Application Verifier）。
2. 菜单 File → Add Application，选择 `camera_utils_tests.exe` 的完整路径。
3. 勾选 Basics 组下的 **Heaps**、**Handles**、**Locks**、**Exceptions** 检查项。
4. 保存，然后在调试器（VS）里运行 exe；一旦触发错误，Verifier 会在错误发生点**立即中断**到调试器，
   精确定位。

**注意**：
- Application Verifier 会大幅降低程序运行速度，只在排查时启用。
- 排查完记得 Remove 该 exe，否则后续运行一直受检。

## 4. 安全加固（CFG / SafeStack 的 MSVC 对应物）

Clang 的 CFI / SafeStack 本机不可用（需 clang-cl），但 MSVC 有对应物，已集成：

### 4.1 CFG（`/guard:cf`，CFI 对应物）

**防什么**：间接调用劫持（篡改函数指针/虚表 → ROP）。

**已集成**：`x64-hardened` preset（Release + `/guard:cf` + `/sdl` + `/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA` 链接加固）。

```bat
cmake --preset x64-hardened
cmake --build --preset x64-hardened
```

**注意**：CFG 是**防御性**机制，正常代码路径下不报错，只在函数指针被篡改时拦截。
验证 demo `cfi-indirect-call` 仅演示合法的间接调用（不会报错）。

### 4.2 /GS 栈 cookie（SafeStack 对应物）

**防什么**：栈缓冲区溢出覆盖返回地址 → ROP。

**已集成**：MSVC Debug/Release **默认开启**，无需额外配置。

**验证**：`gs-cookie-overflow` demo 栈越界覆盖 cookie，函数返回时 /GS 报错并终止。

### 4.3 /sdl（SDL 安全检查）

**已集成**：`x64-hardened` preset。额外启用更严格的安全检查（配合 /GS）。

### 4.4 三者的验证方式差异（重要）

CFG、/GS、/sdl 的**性质不同，验证方式也不同**：

| 工具 | 性质 | 能否「看到报错」 | 验证方式 |
|---|---|---|---|
| `/GS` | 运行时**检测** | ✅ 能 | 跑 `gs-cookie-overflow`，函数返回时报错/挂起 |
| CFG（`/guard:cf`） | 运行时**防御** | ❌ 不能（正常代码不报错） | 跑 `cfi-indirect-call` 看「间接调用正常执行、没被误伤」 |
| `/sdl` | **编译期**检查 | ❌ 无运行时效果 | `x64-hardened` 构建通过 = 代码合规 |

**关键理解**：
- ASan、_CRTDBG、/GS 是「**检测型**」——能主动报出 bug，demo 能看到报错。
- CFG、/sdl 是「**防御型/编译期**」——正常代码下「静默」，正确性体现在「不误伤 + 编译通过」，
  **无法用 demo 造出「报错」**（触发 CFG 报错需要构造「篡改函数指针指向非法地址」的攻击场景）。

**VS 里验证（视图 → 终端，用绝对路径 + 调用运算符 `&`）**：

```powershell
# /GS：x64-debug 跑，会挂起/报错
& "C:\Users\11186484\CameraUtils\out\build\x64-debug\tests\camera_utils_tests.exe" gs-cookie-overflow

# CFG：x64-hardened 跑，正常输出「间接调用执行」
& "C:\Users\11186484\CameraUtils\out\build\x64-hardened\tests\camera_utils_tests.exe" cfi-indirect-call
```

---

## 5. 推荐排查流程

1. **日常 Debug**：跑一遍 `ctest -L unit`，靠 `_CRTDBG` 抓泄漏（零成本）。
2. **怀疑内存越界**：用 `x64-asan` 跑相关测试，ASan 报具体错误。
3. **怀疑 Windows 资源/句柄/临界区**：Application Verifier 的 Basics 检查。

## 6. 一键自动扫描（scripts/scan-bugs.ps1）

将静态分析 + 运行时检查编排成一个脚本，本地和 GitHub CI 通用：

```bat
:: 全部扫描（static + asan + hardened）
powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1

:: 单步扫描
powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1 -Step static
powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1 -Step asan
powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1 -Step hardened
```

| 步骤 | 工具 | 判定 |
|---|---|---|
| static | clang-tidy `clang-analyzer-*` | 有分析器告警 = bug（硬门禁） |
| asan | AddressSanitizer（`x64-asan`） | 跑 `ctest -L unit`，ASan 报错 = bug |
| hardened | `/guard:cf` + `/sdl` + `/GS` + `/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA`（`x64-hardened`） | 构建通过 + 单测通过 = 合规 |

**设计要点**：
- **ASan 关闭 `detect_container_overflow`**：MSVC STL 容器容量注解在合法 vector 扩容时会误报
  （实测在 OpenCV ORB 代码内触发），已通过 `ASAN_OPTIONS=detect_container_overflow=0` 关闭，
  保留 heap-buffer-overflow / use-after-free 等高价值检查。
- **_CRTDBG 泄漏不纳入自动门禁**：OpenCV 进程级惰性单例/缓存导致泄漏报告全线误报
  （几乎每个测试都报），故泄漏检查保持手动 demo（`crtdbg-leak`）验证，不进自动扫描。

### GitHub CI 定时扫描（.github/workflows/scan.yml）

每 2 天（cron `0 18 */2 * *`）+ 手动 `workflow_dispatch` 触发，3 个并行 job 分别跑
static / asan / hardened。CI 上 preset 硬编码的本地编译器路径不存在，脚本用
`setup-msvc-dev` 的 `VCToolsInstallDir` 推导 cl.exe 路径覆盖 `CMAKE_CXX_COMPILER`。

## 已集成的运行检查总结

| 工具 | 状态 | 查什么 |
|---|---|---|
| ASan（`x64-asan` preset） | ✅ 已集成 | 越界、use-after-free、泄漏 |
| _CRTDBG（`TestMain.cpp`） | ✅ 已集成 | 内存泄漏 |
| /RTC1 | ✅ Debug 默认 | 未初始化变量、栈帧 |
| CFG（`x64-hardened`，`/guard:cf`） | ✅ 已集成 | 间接调用劫持（防御） |
| /GS 栈 cookie | ✅ 默认开启 | 栈溢出覆盖返回地址（防御） |
| /sdl（`x64-hardened`） | ✅ 已集成 | SDL 安全检查 |
| ASLR/DEP 链接加固（`x64-hardened`，`/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA`） | ✅ 已集成 | 地址随机化 + 数据执行保护 |
| Application Verifier | 📖 本文档说明 | 句柄/堆/临界区 |

> UBSan（未定义行为）与 TSan（线程竞争）需 clang-cl 编译器，本机 MSVC 不支持，且 Windows 上
> TSan 不稳定，暂不集成（见 `AGENTS.md` 或向维护者咨询）。
