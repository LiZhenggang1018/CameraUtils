# 运行时检查工具使用指南（ASan / _CRTDBG / Application Verifier / PageHeap）

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
| `pageheap-overflow` | `x64-Debug` + gflags 开 PageHeap | 堆越界即时崩溃 |
| `ubsan-undefined` | 需 clang-cl | 有符号整数溢出 |

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

## 4. PageHeap（页堆，堆越界即时检测）

**查什么**：堆缓冲区溢出（在越界写入发生的**那一刻**崩溃，而不是事后）。

**使用**（命令行，`gflags.exe` 来自 Windows Debugging Tools）：

```bat
:: 为 camera_utils_tests.exe 开启 PageHeap（full 模式）
gflags /p /enable camera_utils_tests.exe /full

:: 运行测试，越界时立即崩溃
out\build\x64-debug\tests\camera_utils_tests.exe <test-name>

:: 排查完关闭
gflags /p /disable camera_utils_tests.exe
```

**注意**：
- PageHeap 会把每次分配对齐到页边界，越界读写会触发访问违例，定位极准，但内存开销巨大。
- 与 Application Verifier 的 Heaps 检查有重叠，二选一即可；PageHeap 更激进，适合「怀疑某个越界但 ASan 没抓到」。

## 推荐排查流程

1. **日常 Debug**：跑一遍 `ctest -L unit`，靠 `_CRTDBG` 抓泄漏（零成本）。
2. **怀疑内存越界**：用 `x64-asan` 跑相关测试，ASan 报具体错误。
3. **怀疑 Windows 资源/句柄/临界区**：Application Verifier 的 Basics 检查。
4. **怀疑堆越界但 ASan 未命中**：gflags 开 PageHeap 复现。

## 已集成的运行检查总结

| 工具 | 状态 | 查什么 |
|---|---|---|
| ASan（`x64-asan` preset） | ✅ 已集成 | 越界、use-after-free、泄漏 |
| _CRTDBG（`TestMain.cpp`） | ✅ 已集成 | 内存泄漏 |
| /RTC1 | ✅ Debug 默认 | 未初始化变量、栈帧 |
| Application Verifier | 📖 本文档说明 | 句柄/堆/临界区 |
| PageHeap（gflags） | 📖 本文档说明 | 堆越界即时检测 |

> UBSan（未定义行为）与 TSan（线程竞争）需 clang-cl 编译器，本机 MSVC 不支持，且 Windows 上
> TSan 不稳定，暂不集成（见 `AGENTS.md` 或向维护者咨询）。
