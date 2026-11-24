# GitHub Actions CI 检查全景

本工程在 GitHub Actions 上共 4 个 workflow + 1 个复用 composite action。
本文档介绍每个 workflow 的触发方式、运行环境与实际执行内容。

## 一、4 个 workflow 总览

| Workflow | 文件 | 触发 | runner | 检查内容 | 硬门禁 |
|---|---|---|---|---|---|
| Windows CI | `ci-windows.yml` | 每 3 天 + 手动 | windows-2022 | 构建 + unit 单测 | 测试失败 |
| Style | `style.yml` | push/PR | windows-latest | clang-format 格式 | 格式不符 |
| Tidy | `tidy.yml` | push/PR | windows-latest | clang-tidy 静态分析 | `clang-analyzer-*` |
| Scheduled Bug Scan | `scan.yml` | 每 2 天 + 手动 | windows-2022 | ASan + hardened + 静态 | 任一 finding |

## 二、逐 workflow 详解

### 1. ci-windows.yml — 构建 + 单元测试

- **触发**：`schedule: '0 11 */3 * *'`（每 3 天）+ `workflow_dispatch`（可选 `ref`、`build_target`）
- **环境**：`windows-2022`

步骤：

1. `actions/checkout@v5`
2. `TheMrMilchmann/setup-msvc-dev@v4`（arch x64，等价 `vcvarsall.bat x64`）
3. `actions/cache@v5` 缓存 CPM 包
4. `.github/actions/setup-opencv`（含 contrib，`config: Debug`）
5. `cmake --preset x64-debug`
6. `cmake --build --preset x64-debug --target camera_utils_tests`
7. `ctest --test-dir out/build/x64-debug -C Debug -L unit --output-on-failure`

> 只跑 `-L unit`，不跑 media（CI 无 `data/`）。

### 2. style.yml — clang-format 格式检查

- **触发**：`push` / `pull_request` 到 main
- **环境**：`windows-latest`

步骤：

1. checkout
2. 递归查找 `C:\Program Files\Microsoft Visual Studio` 下的 `clang-format.exe`
3. 遍历 `inc`、`src`、`tests` 下 `.h/.cpp`，逐个 `clang-format --dry-run --Werror`

### 3. tidy.yml — clang-tidy 静态分析

- **触发**：`push` / `pull_request` 到 main
- **环境**：`windows-latest`

步骤：

1. checkout
2. `setup-msvc-dev@v4`
3. `setup-opencv`（`config: Release`）
4. `cmake --preset x64-release`（生成 `compile_commands.json`）
5. 定位 `clang-tidy.exe`
6. **版本校验**：clang-tidy 主版本 vs MSVC STL 最低要求（14.44→Clang 19，14.50+→Clang 22），不匹配报 STL1000
7. 遍历 `src/`、`tests/` 全部翻译单元（排除 `_deps/`、`.cpm-cache/`），
   逐个跑 `clang-tidy --warnings-as-errors=clang-analyzer-*`

> 只有 `clang-analyzer-*` 静态分析器告警会 fail；`bugprone`/`style` 类先报告、不阻断。

### 4. scan.yml — 定时 bug 扫描

- **触发**：`schedule: '0 18 */2 * *'`（每 2 天）+ `workflow_dispatch`
- **环境**：3 个并行 job，均 `windows-2022`

| Job | 内容 | OpenCV config | 脚本 |
|---|---|---|---|
| `static-analysis` | clang-tidy `clang-analyzer-*` 全量 | Release | `scan-bugs.ps1 -Step static` |
| `asan` | 构建 `x64-asan` + 跑 unit 单测 | Debug | `scan-bugs.ps1 -Step asan` |
| `hardened` | 构建 `x64-hardened`（`/guard:cf`+`/sdl`+`/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA`）+ 单测 | Release | `scan-bugs.ps1 -Step hardened` |

每个 job 步骤：checkout → setup-msvc-dev → cache CPM → setup-opencv → 跑扫描。

## 三、复用 composite action：setup-opencv

所有需要 OpenCV 的 workflow 都调用 `.github/actions/setup-opencv`：

1. `actions/cache@v5` 按 `version + config + action.yml 哈希 + OS` 缓存 `C:/opencv-contrib`
2. 缓存未命中时 clone `opencv` + `opencv_contrib`，用 `BUILD_LIST` 只编译所需模块：
   `core,imgproc,highgui,imgcodecs,video,videoio,features2d,calib3d,flann,photo,optflow,ximgproc`
3. 导出 `OPENCV_ROOT`，校验 `optflow.hpp` 存在（确认含 contrib）

> 模块清单必须与 `CMakeLists.txt` 的 `CAMERA_UTILS_OPENCV_MODULES` +
> `CAMERA_UTILS_OPENCV_CONTRIB_MODULES` 保持一致（详见 `AGENTS.md`）。

## 四、CI 环境与本地差异（关键）

| 维度 | 本地 | CI |
|---|---|---|
| MSVC 工具集 | 14.44（开发者）/ 14.52（助手） | windows runner 自带（版本不固定） |
| OpenCV | 预编译（world 打包 DLL） | 源码构建（分模块 DLL） |
| clang-tidy 版本 | LLVM 19（配 14.44） | VS 自带（需版本校验） |

**编译器路径（重要坑）**：`CMakePresets.json` 硬编码了本地 cl.exe 绝对路径（根治 STL1001）：
- `x64-debug` / `x64-asan` / `x64-hardened` / `x64-warnings` → `C:/Program Files (x86)/.../14.52.37025/.../cl.exe`
- `x64-release` → `D:/Program Files/visual_studio/.../14.44.35207/.../cl.exe`

CI runner（windows-2022）上这些路径**不存在**（runner 实际是
`C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\`），
直接 `cmake --preset` 会报 `is not a full path to an existing compiler tool`。

**修复方式**：所有 CI workflow 的 configure 步骤都用 `setup-msvc-dev` 提供的
`VCToolsInstallDir` 推导 runner 上的 cl.exe，再用 `-D` 覆盖 preset 的硬编码值：

```powershell
$cl = (Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe') -replace '\\', '/'
cmake --preset x64-debug "-DCMAKE_CXX_COMPILER=$cl" "-DCMAKE_C_COMPILER=$cl"
```

`scan.yml` 通过脚本 `scripts/scan-bugs.ps1` 的 `Get-CMakeCompilerArgs` 自动做同样的事，
`ci-windows.yml` 和 `tidy.yml` 则在 workflow 内联这段逻辑。三处逻辑保持一致。
