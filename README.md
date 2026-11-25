# CameraUtils

安卓相机领域硬件抽象层（HAL）的通用工具类与代码库，同时作为 Szeliski《计算机视觉：算法与应用》的学习实践工程。

项目以「学习笔记 + 可复用代码 + 断言测试 + 调参 demo」四合一的方式，按章节归档了图像处理、特征匹配、光流、HDR、SfM、深度估计等计算机视觉算法。

## 模块总览

| 目录 | 章节 / 主题 | 说明 |
|---|---|---|
| `inc/CommonUtils/` | 公共工具类 | 跨章节复用的通用工具（线程池、工厂、日志、JSON、OpenCV/FFmpeg/exiv2 封装等） |
| `inc/CoreUtils.h` | 纯标准库聚合头 | 汇总 CommonUtils 下大部分纯 C++ 工具 |
| `inc/ImageProcessing/` | 第 3 章 图像处理 | 线性/非线性滤波、金字塔、Sobel |
| `inc/FeatureMatching/` | 第 7 章 特征检测与匹配 | 关键点、局部特征、匹配 + RANSAC |
| `inc/Alignment/` | 第 8 章 对齐与拼接 | 单应、融合、曝光补偿、渐晕、全景 |
| `inc/OpticalFlow/` | 第 9 章 运动估计 | 稀疏/稠密光流、视频插帧、EIS |
| `inc/ComputationalPhotography/` | 第 10 章 计算摄影 | HDR 融合、MFNR 多帧降噪 |
| `inc/SfM/` | 第 11 章 运动恢复结构 | 标定、对极几何、三角化、PnP |
| `inc/Depth/` | 第 12 章 深度估计 | 极线校正、双目匹配、深度虚化 |
| `inc/LivePhotoSmoothPlay/` | LivePhoto 平滑播放 | 解码 → 配准 → FOV 渐变 → 重编码 |

## 运行环境

Windows 11 + Visual Studio 2022（本机 x64 Debug / Release），CMake + Ninja 构建。

## 编程语言

C++20

## 构建依赖

通过 CMake + CPM 下载第三方依赖：OpenCV（含 contrib 模块 optflow / ximgproc）、FFmpeg、stdexec、nlohmann_json、magic_enum、exiv2。

## 构建

**日常增量构建统一走 `scripts\build-debug.bat`**（内部先 `chcp 65001` 规避 Ninja 中文头文件依赖追踪 bug，再 `vcvars64` 再 `cmake --build`）：

```bat
scripts\build-debug.bat                          :: 构建默认目标
scripts\build-debug.bat camera_utils_tests       :: 构建指定目标
```

完整配置（首次或改了 CMakeLists/CMakePresets 时）：

```powershell
# Debug（Ninja + MSVC 18 BuildTools）
cmake --preset x64-debug
cmake --build --preset x64-debug

# Release（Visual Studio 2022 自带工具链）
cmake --preset x64-release
cmake --build --preset x64-release
```

> Ninja 与 MSVC 需在环境中可用：先执行对应的 `vcvars64.bat`，或直接从 Visual Studio 开发者命令行启动。
> 若不走 `build-debug.bat`，务必先 `chcp 65001`（否则改了 `.h` 不触发重编译，详见 AGENTS.md）。
> OpenCVConfig.cmake 不识别 MSVC 19.5x 时，`CMakeLists.txt` 已自动兜底设置 `OpenCV_RUNTIME=vc17`。

### CMake Presets

| Preset | 配置 | 说明 |
|---|---|---|
| `x64-debug` | Debug（BuildTools 14.52） | 日常开发默认 |
| `x64-release` | Release（VS2022 14.44） | 开发者侧 / clang-tidy 配对环境 |
| `x64-hardened` | Release + `/guard:cf /sdl` + `/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA` | CFG + SDL + ASLR/DEP 加固 |
| `x64-asan` | Debug + `/fsanitize=address` | AddressSanitizer |
| `x64-warnings` | Debug + 严格告警 | `/W4` + 遮蔽/转换/虚析构/未定义宏等严格告警项 |
| `x86-debug` / `x86-release` | x86 Debug / Release | — |

> 本机有两套 MSVC 工具集并存，`x64-debug` 与 `x64-release` 的产物、告警互不相通。构建/测试结果请注明用的是哪套配置，详见 `AGENTS.md`。

### 构建选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `CAMERA_UTILS_ENABLE_VAS_CONCURRENCY` | `ON` | 编译依赖 stdexec 的 VAS 并发示例，并注册 `vas-concurrency` 测试用例 |

开关通过 `VAS_ENABLE_STDEXEC` 宏传递给 `tests/VasConcurrencyDemo.h`。改动该选项时必须显式传入 `-D`，因为 CMake 的 `option()` 默认值不会覆盖已有构建目录中 `CMakeCache.txt` 的旧值：

```powershell
cmake -S . -B out/build/x64-release -DCAMERA_UTILS_ENABLE_VAS_CONCURRENCY=OFF
```

警告级别、`/permissive-`、`/utf-8` 与 Release 优化选项统一由 INTERFACE 目标 `CameraUtils::build_options`（根 `CMakeLists.txt`）提供，`camera_utils_core`、`CameraUtils`、`camera_utils_tests` 共用，无需各自重复声明。

## 运行

```powershell
# 列出全部命令
CameraUtils.exe --list

# 查看帮助（按 unit / media / gui 分组）
CameraUtils.exe --help

# 执行单个命令
CameraUtils.exe hdr-merge-ghost

# 无参数运行 → 交互式菜单
CameraUtils.exe
```

命令分三类：

| 类别 | 说明 |
|---|---|
| `unit` | 纯内存运算，无外部依赖 |
| `media` | 需要 `data/` 目录下的媒体素材 |
| `gui` | 需要图形界面（`imshow` / `waitKey`） |

## 测试

```powershell
# 只跑单元测试（无需素材）
ctest --preset x64-debug-unit

# 跑依赖素材的测试（data/ 存在时才会被注册）
ctest --preset x64-debug-media
```

等价的长命令（无 testPresets 时）：

```powershell
ctest --test-dir out/build/x64-debug -L unit --output-on-failure
ctest --test-dir out/build/x64-debug -L media --output-on-failure
```

测试由独立可执行文件 `camera_utils_tests` 驱动，每个用例单独注册到 CTest：

```powershell
camera_utils_tests.exe --list
camera_utils_tests.exe magic-enum
```

退出码约定：`0` 通过、`非 0` 失败、`77` 跳过（素材缺失）。

> `gui` 类命令不注册进 ctest，用 `CameraUtils.exe <command>` 手动运行。

## 常用命令速查

| 场景 | 命令 |
|---|---|
| 增量构建 | `scripts\build-debug.bat` |
| 构建指定目标 | `scripts\build-debug.bat camera_utils_tests` |
| 首次配置（Debug） | `cmake --preset x64-debug` |
| 跑单元测试 | `ctest --preset x64-debug-unit` |
| 跑素材测试 | `ctest --preset x64-debug-media` |
| 列出全部测试/命令 | `out\build\x64-debug\tests\camera_utils_tests.exe --list` |
| 跑单个测试 | `out\build\x64-debug\tests\camera_utils_tests.exe magic-enum` |
| 运行 GUI demo | `out\build\x64-debug\src\CameraUtils.exe sift-handwritten-demo` |
| 数值报告 demo | `out\build\x64-debug\src\CameraUtils.exe sift-handwritten-report` |
| clang-format 格式化 | `D:\Program Files\visual_studio\VC\Tools\Llvm\x64\bin\clang-format.exe -i --style=file <文件>` |
| 乱码检测 | `powershell -ExecutionPolicy Bypass -File scripts/check-encoding.ps1` |
| 静态扫描 | `powershell -ExecutionPolicy Bypass -File scripts/scan-bugs.ps1 -Step all` |
| 启用 pre-commit hook | `git config core.hooksPath githooks` |

## 目录结构

```
CameraUtils/
├── inc/                            接口层（头文件，绝大多数 header-only）
│   ├── CoreUtils.h                 纯标准库工具聚合头（不含第三方依赖）
│   ├── CommonUtils/                公共工具类（跨章节复用）
│   │   ├── CheckUtils.h            断言/检查工具
│   │   ├── ThreadPool.h            线程池
│   │   ├── GeneralFactory.h        通用工厂模式
│   │   ├── Singleton.h / Event*.h / SyncAgency.h  单例/事件/同步
│   │   ├── JsonUtils.h             JSON 序列化（nlohmann）
│   │   ├── OpenCVCommon.h / FFMpegCommon.h / Exiv2Common.h  第三方库封装
│   │   └── ...                     其余见 inc/CommonUtils/README.md
│   ├── ImageProcessing/            第 3 章 图像处理（README.md 见章节对照表）
│   ├── FeatureMatching/            第 7 章 特征检测与匹配
│   ├── Alignment/                  第 8 章 对齐与拼接
│   ├── OpticalFlow/                第 9 章 光流/插帧/EIS（接口层）
│   ├── ComputationalPhotography/   第 10 章 计算摄影（HDR/MFNR）
│   ├── SfM/                        第 11 章 运动恢复结构
│   ├── Depth/                      第 12 章 深度估计
│   └── LivePhotoSmoothPlay/        LivePhoto 平滑播放（接口层）
├── src/                            实现层（只有 OpticalFlow / LivePhotoSmoothPlay 有 .cpp）
│   ├── main.cpp                    CLI 入口（子命令 + 菜单）
│   ├── CMakeLists.txt              camera_utils_core（静态库）+ CameraUtils（可执行）目标定义
│   ├── OpticalFlow/                FrameInterpolator / LkTracker / FfmpegVideoWriter 等 .cpp
│   │   ├── README.md               实现总览
│   │   └── IMPLEMENTATION_DETAILS.md  逐文件实现细节
│   └── LivePhotoSmoothPlay/        ImageRegistrator.cpp / YUVImageReader.cpp
│       ├── README.md               实现总览
│       └── IMPLEMENTATION_DETAILS.md  逐文件实现细节
├── tests/                          测试与 demo
│   ├── cli/CommandTable.h          命令表（CLI 与测试共用）
│   ├── cli/TestAssert.h            断言宏（Release 下同样生效）
│   ├── TestMain.cpp                测试驱动入口
│   ├── CMakeLists.txt              ctest 注册（unit / media 分组）
│   ├── CommonUtils/                公共工具类测试用例
│   ├── ImageProcessing/            第 3 章测试与 demo
│   ├── FeatureMatching/            第 7 章测试与 demo
│   ├── Alignment/                  第 8 章测试与 demo
│   ├── OpticalFlow/                第 9 章测试与 demo
│   ├── ComputationalPhotography/   第 10 章测试与 demo
│   ├── SfM/                        第 11 章测试与 demo
│   ├── Depth/                      第 12 章测试与 demo
│   ├── LivePhotoSmoothPlay/        LivePhoto 平滑播放测试
│   └── CppRuntimeCheck/            C++ 运行时检查 demo
├── cmake/                          CPM.cmake（依赖管理）+ tools.cmake
├── configs/                        运行时 JSON 配置（HdrConfig / InterpolationConfig / Shapes 等）
├── docs/                           CI / 学习 / 工具文档
├── scripts/                        check-encoding.ps1（乱码检测）/ scan-bugs.ps1（静态扫描）
├── .github/                        workflows（ci-windows / style / tidy / scan）+ setup-opencv action
├── CMakeLists.txt                  根构建脚本（依赖解析 + build_options）
├── CMakePresets.json               构建预设
├── .clang-format / .clang-tidy / .cmake-format  代码格式 / 静态分析 / CMake 格式配置
├── .editorconfig / .gitattributes  编码与行尾规范
├── Show-Tree.ps1                   目录树打印脚本
├── data/                           媒体素材（不入库，.gitignore 忽略）
└── references/                     编码规范文档（不入库）
```

### 章节对照表

| 章节 | 头文件（接口） | 实现（.cpp） | 测试（tests/） | header-only | 命名空间 | 详细文档 |
|---|---|---|---|---|---|---|
| 3 图像处理 | `inc/ImageProcessing/` | — | `ImageProcessing/` | 是 | `ImageProcessing` / `Sobel` | `inc/ImageProcessing/README.md` |
| 7 特征匹配 | `inc/FeatureMatching/` | — | `FeatureMatching/` | 是 | `FeatureMatching` | `inc/FeatureMatching/README.md` |
| 8 对齐拼接 | `inc/Alignment/` | — | `Alignment/` | 是 | `Alignment` | `inc/Alignment/README.md` |
| 9 运动估计 | `inc/OpticalFlow/` | `src/OpticalFlow/` | `OpticalFlow/` | 否 | `OpticalFlow` | `inc/OpticalFlow/README.md`、`src/OpticalFlow/README.md` |
| 10 计算摄影 | `inc/ComputationalPhotography/` | — | `ComputationalPhotography/` | 是 | `ComputationalPhotography` | `inc/ComputationalPhotography/README.md` |
| 11 SfM | `inc/SfM/` | — | `SfM/` | 是 | `SfM` | `inc/SfM/README.md` |
| 12 深度估计 | `inc/Depth/` | — | `Depth/` | 是 | `Depth` | `inc/Depth/README.md` |
| LivePhoto 平滑播放 | `inc/LivePhotoSmoothPlay/` | `src/LivePhotoSmoothPlay/` | `LivePhotoSmoothPlay/` | 部分（仅 2 个 .cpp） | 全局 | `inc/LivePhotoSmoothPlay/README.md`、`src/LivePhotoSmoothPlay/README.md` |

> **约定**：可复用算法按章节目录归档（`inc/<章节>/` + `src/<章节>/`）；公共工具类放 `inc/CommonUtils/`；测试统一放 `tests/<章节>/`，不散落在 `tests/` 根目录。详见 `AGENTS.md`。

## 个人备注

1. 手动下载 rapids-cmake，解压到 `D:/ThirdPartyLib/rapids-cmake`，解决内网 CMake 下载 rapids-cmake 时 SSL 验证失败问题。
2. 为节省 opencv-contrib 每次清理后重新构建的耗时，在本地 `D:\ThirdPartyLib\OpenCvForCameraUtils\install` 安装了 OpenCV-contrib 的 debug 与 release。
