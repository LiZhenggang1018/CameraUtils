项目名称：\
CameraUtils

项目简介：\
Utils code for Android camera HAL use / 安卓相机领域 硬件抽象层HAL的一些通用工具类+代码。\
包含 4 个模块：纯标准库工具层（CoreUtils）、光流/配准（OpticalFlow）、LivePhoto 平滑播放（LivePhotoSmoothPlay）、HDR 融合（HdrMerge）。

运行环境：\
windows11 OS + visual studio 2022 本地计算机 x64Debug/Release,

编程语言：\
C++20

构建依赖：\
cmake 构建，通过 CPM 下载 3 方依赖库：opencv(含contrib模块) ffmpeg stdexec nlohmann_json MagicEnum exiv2

个人备注：\
1. 手动下载rapids-cmake，解压到如下路径D:/ThirdPartyLib/rapids-cmake，解决内网CMake 在下载 rapids-cmake 时 SSL 验证失败问题：
2. 为了节省opencv-contrib每次清理后重新构建的耗时操作，在本地路径D:\ThirdPartyLib\OpenCvForCameraUtils\install 安装了OpenCV-contrib的debug与release

## 构建

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

> Ninja 与 MSVC 需在环境中可用：先执行对应的 `vcvars64.bat`，或直接从 Visual Studio 开发者命令行启动。
> OpenCVConfig.cmake 不识别 MSVC 19.5x 时，`CMakeLists.txt` 已自动兜底设置 `OpenCV_RUNTIME=vc17`。

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
ctest --test-dir out/build/x64-debug -L unit --output-on-failure

# 跑依赖素材的测试（data/ 存在时才会被注册）
ctest --test-dir out/build/x64-debug -L media --output-on-failure
```

测试由独立可执行文件 `camera_utils_tests` 驱动，每个用例单独注册到 CTest：

```powershell
camera_utils_tests.exe --list
camera_utils_tests.exe magic-enum
```

退出码约定：`0` 通过、`非 0` 失败、`77` 跳过（素材缺失）。

## 目录结构

```
inc/
  CoreUtils.h             纯标准库工具层聚合头
  Utils.h                 兼容聚合头（已标记 @deprecated）
  OpticalFlow/*.h         光流/配准
  LivePhotoSmoothPlay/*.h LivePhoto 平滑播放
  HdrMerge/*.h            HDR 融合
src/
  main.cpp                CLI 入口（子命令 + 菜单）
  OpticalFlow/*.cpp       → camera_utils_core
  LivePhotoSmoothPlay/*.cpp → camera_utils_core
tests/
  cli/CommandTable.h      命令表（CLI 与测试共用）
  cli/TestAssert.h        断言宏（Release 下同样生效）
  TestMain.cpp            测试驱动入口
  *.h                     各模块用例
configs/                  运行时 JSON 配置
data/                     媒体素材（不入库）
```
