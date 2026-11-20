# 第 3 章 Image Processing 里程碑实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，实现第 3 章 Image Processing 的三大模块（线性滤波 / 非线性滤波 / 金字塔）与傅里叶简单展示，每个模块内聚「学习笔记 + 调参 demo + 断言测试 + 可复用代码」。

**Architecture:** 采用「模板模块先行、滚动复制」策略。先完整落地「线性滤波」模块，验证
「笔记 + 交互式 demo + 断言测试 + config 序列化 + data 目录」的整套结构约定，再复制到
「非线性滤波」「金字塔」两块；傅里叶变换做轻量展示。可复用算法代码放 `inc/ImageProcessing/`
（接口）+ `src/ImageProcessing/`（实现），demo 放 `tests/`，配置放 `configs/`，素材放
`data/ImageProcessing/<module>/`（不入库）。

**Tech Stack:** C++20, OpenCV 4.10（imgproc/imgcodecs/highgui）, nlohmann::json（配置序列化）,
MSVC 19.52 + Ninja（构建）, CTest（断言测试）, CU_CHECK（断言宏）。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心（Sobel 已手写属例外，本里程碑不再新增手写核心）。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码，围绕同一份代码。
- 代码定位 = 可复用库（`inc/` 接口 + `src/` 实现，参考 `OpticalFlow/`）；demo 严格放 `tests/`，不放 `inc/`。
- 目录按章节：第 3 章 → `inc/ImageProcessing/` + `src/ImageProcessing/`。
- 调参 demo = 交互式 `imshow` GUI 窗口，`CommandTable.h` 标 `Label::GUI`，不进 ctest。
- json 配置 = 仅参数有意义的模块配 `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`（参考 `HdrConfig.h`），近无参用代码内常量。
- 数据目录 = `data/ImageProcessing/<module>/`（本地，不入库，`.gitignore` 已忽略 `data/`）。
- 命名规则（.clang-tidy readability-identifier-naming）：类型/函数大驼峰，变量/参数小驼峰，类成员带 `_` 后缀，常量全大写。
- 格式：提交前 `clang-format -i --style=file <file>`。
- 提交：多文件逐文件提交，`git push origin main`，提交信息中文 + Conventional Commits 前缀。
- 本里程碑范围约束：不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：`cv::Mat` 为空时，滤波函数不应崩溃或 UB，应安全返回空 Mat 或由 `CV_Assert` 明确报错。
2. **通道数与类型**：滤波函数若接收灰度 `CV_8UC1`，遇到 `CV_8UC3` 彩色图应明确行为（支持三通道逐通道处理，还是仅支持单通道）。
3. **核尺寸奇数**：模糊/中值等核尺寸为偶数时 OpenCV 会报错，模块应显式约束或文档说明。
4. **金字塔层数越界**：金字塔层数过大导致 `buildPyramid` 尺寸不足时（已有 `OpticalFlow` README 记录的坑），应限制层数或断言。
5. **config 文件缺失/损坏**：`LoadFromFile` 应回退默认值并生成默认 json（沿用 `HdrConfig.h` 模式），不得崩溃。

---

## 模块结构与接口约定（锁定，后续任务引用）

每个模块遵循同一骨架。以「线性滤波」模块 `LinearFilter` 为模板，接口命名如下（其余模块对应替换 `LinearFilter` → `NonLinearFilter` / `Pyramid`）：

- 接口头文件 `inc/ImageProcessing/LinearFilter.h`，namespace `ImageProcessing`。
- 实现 `src/ImageProcessing/LinearFilter.cpp`（若 header-only 则省略）。
- 学习笔记 `inc/ImageProcessing/LinearFilterNotes.md`。
- 断言测试 `tests/LinearFilterTest.h`，命令名 `linear-filter`（`Label::UNIT`）。
- 调参 demo `tests/LinearFilterDemo.h`，命令名 `linear-filter-demo`（`Label::GUI`）。
- 配置 `inc/ImageProcessing/LinearFilterConfig.h` + `configs/LinearFilterConfig.json`。
- 素材 `data/ImageProcessing/LinearFilter/`。

---

### Task 1: 建立第 3 章目录骨架 + Sobel 归位

**Files:**
- Create: `inc/ImageProcessing/`（目录）
- Create: `src/ImageProcessing/`（目录）
- Modify: `src/CMakeLists.txt`（include 目录新增 `inc/ImageProcessing`；GLOB 新增 `src/ImageProcessing/*.cpp`）
- Modify: `tests/cli/CommandTable.h`（更新 Sobel include 路径）
- Modify: `tests/SobelTest.h`（更新 include 路径）
- Modify: `inc/ImageReg/SobelDetect.h` → 移动为 `inc/ImageProcessing/SobelDetect.h`

**Interfaces:**
- Consumes: 无（纯骨架任务）。
- Produces: `inc/ImageProcessing/` 目录 + `src/ImageProcessing/` 目录 + `src/CMakeLists.txt` 中已注册的新 include/GLOB 路径；`SobelDetect.h` 位于 `inc/ImageProcessing/` 下。

- [ ] **Step 1: 移动 SobelDetect.h 到新章节目录**

  将 `inc/ImageReg/SobelDetect.h` 移动到 `inc/ImageProcessing/SobelDetect.h`（保留 namespace `Sobel`，仅改头文件物理位置与 include guard）。

- [ ] **Step 2: 更新引用 SobelDetect.h 的 include**

  `tests/SobelTest.h` 中 `#include "SobelDetect.h"` 改为 `#include "ImageProcessing/SobelDetect.h"`。

- [ ] **Step 3: 注册新 include 目录与 src GLOB**

  在 `src/CMakeLists.txt` 的 `target_include_directories(camera_utils_core PUBLIC ...)` 中新增一行 `${CMAKE_SOURCE_DIR}/inc/ImageProcessing`；在 `camera_utils_core` 的 `file(GLOB ...)` 中新增 `src/ImageProcessing/*.cpp`。

- [ ] **Step 4: 构建验证**

  Run: `cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && ninja -C out\build\x64-debug camera_utils_tests'`
  Expected: 编译成功，无 error。

- [ ] **Step 5: 运行 sobel 测试确认未破坏**

  Run: `cmd /c 'out\build\x64-debug\tests\camera_utils_tests.exe sobel'`
  Expected: `[PASS] sobel`

- [ ] **Step 6: Commit**

  ```bash
  git add inc/ImageProcessing/SobelDetect.h
  git commit -m "refactor(ImageProcessing): 将 SobelDetect 归入第 3 章目录"
  git add tests/SobelTest.h src/CMakeLists.txt
  git commit -m "refactor(ImageProcessing): 更新 Sobel include 路径与构建配置"
  ```

---

### Task 2: 线性滤波模块 — 可复用库代码 + 断言测试

**Files:**
- Create: `inc/ImageProcessing/LinearFilter.h`
- Create: `tests/LinearFilterTest.h`
- Modify: `tests/cli/CommandTable.h`（注册 `linear-filter` 命令，`Label::UNIT`）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 新增 `linear-filter`）

**Interfaces:**
- Consumes: Task 1 的目录骨架与 include 路径。
- Produces: `namespace ImageProcessing` 内：
  - `cv::Mat GaussianBlur(const cv::Mat& src, int kernelSize, double sigmaX)` → `CV_8UC1`/`CV_8UC3` 输出
  - `cv::Mat MeanBlur(const cv::Mat& src, int kernelSize)` → 同上
  - `cv::Mat Sharpen(const cv::Mat& src, double amount)` → 锐化（unsharp 或卷积核）
  - `cv::Mat SobelX(const cv::Mat& src)` / `cv::Mat SobelY(const cv::Mat& src)` → `CV_16S` 梯度（可复用已有 `Sobel` 命名空间或委托）

  这些函数是「读懂原理 + 调 OpenCV」定位：内部调用 `cv::GaussianBlur` / `cv::blur` /
  `cv::filter2D` / `cv::Sobel`，封装成统一语义。

- [ ] **Step 1: 写失败测试 `tests/LinearFilterTest.h`**

  断言（用 `CU_CHECK` / `CU_CHECK_EQ`）：
  1. `GaussianBlur` 对全 0 图输出仍全 0；对全 255 图输出仍全 255（保亮度/常数图不变性）。
  2. `MeanBlur` 对 3×3 全 255 图中心像素输出 255（均值不变的平凡真值）。
  3. `Sharpen` 对常数图输出仍为常数（锐化不改变平坦区）。
  4. `SobelX`/`SobelY` 尺寸与类型正确（`CV_16S`），且 `SobelX` 对垂直阶跃图中心响应为正（真值方向）。
  5. 空输入 `cv::Mat()` 传入各函数：不崩溃，返回空 Mat。

- [ ] **Step 2: 运行测试确认失败**

  Run: `cmd /c 'out\build\x64-debug\tests\camera_utils_tests.exe linear-filter'`
  Expected: 失败（`linear-filter` 命令不存在或函数未定义）。

- [ ] **Step 3: 实现 `inc/ImageProcessing/LinearFilter.h`**

  定义 `namespace ImageProcessing`，上述 5 个函数。每个函数内部调用对应 OpenCV API 并保留
  语义：空输入提前返回空 Mat；`SobelX/Y` 用 `cv::Sobel(..., CV_16S, ...)` 而非手写卷积。

- [ ] **Step 4: 运行测试确认通过**

  Run: `cmd /c 'out\build\x64-debug\tests\camera_utils_tests.exe linear-filter'`
  Expected: `[PASS] linear-filter`

- [ ] **Step 5: Commit**

  ```bash
  git add inc/ImageProcessing/LinearFilter.h
  git commit -m "feat(ImageProcessing): 线性滤波模块（高斯/均值/锐化/Sobel）"
  git add tests/LinearFilterTest.h tests/cli/CommandTable.h tests/CMakeLists.txt
  git commit -m "test(ImageProcessing): 线性滤波断言测试"
  ```

---

### Task 3: 线性滤波模块 — 调参 demo + config 序列化 + 学习笔记

**Files:**
- Create: `inc/ImageProcessing/LinearFilterConfig.h`
- Create: `configs/LinearFilterConfig.json`（首次运行时自动生成，可提交默认版）
- Create: `configs/LinearFilterConfig_README.md`
- Create: `tests/LinearFilterDemo.h`
- Create: `inc/ImageProcessing/LinearFilterNotes.md`
- Modify: `tests/cli/CommandTable.h`（注册 `linear-filter-demo`，`Label::GUI`）

**Interfaces:**
- Consumes: Task 2 的 `ImageProcessing::{GaussianBlur, MeanBlur, Sharpen, SobelX, SobelY}`。
- Produces:
  - `struct LinearFilterConfig`（含 `int kernelSize = 3; double sigma = 1.0; double sharpenAmount = 1.0; std::string filterType = "gaussian";` + `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` + `DefaultPath()` + `LoadFromFile(path)` + `SaveToFile(path)`，参考 `HdrConfig.h`）。
  - demo 入口 `inline int LinearFilterDemo()`（交互式 imshow，调参看效果）。

- [ ] **Step 1: 写 `LinearFilterConfig.h` 配置结构体**

  字段：`kernelSize`（默认 3）、`sigma`（默认 1.0）、`sharpenAmount`（默认 1.0）、
  `filterType`（默认 `"gaussian"`）。`DefaultPath()` 返回 `EnvRootPath() + "/configs/LinearFilterConfig.json"`。
  `LoadFromFile` 缺失/损坏时回退默认并 `Srlz` 生成默认 json（沿用 `HdrConfig.h` 模式）。

- [ ] **Step 2: 写 `tests/LinearFilterDemo.h` 交互式 demo**

  `LinearFilterDemo()`：加载一张素材图（`data/ImageProcessing/LinearFilter/input.jpg`，若不存在用
  合成图 `cv::Mat` 生成）；读 `LinearFilterConfig`；`namedWindow` + `imshow`，键盘按键切换
  `filterType`、调 `kernelSize`/`sigma`/`sharpenAmount`，实时显示滤波结果与参数文本。复用
  Task 2 的库函数，不在 demo 里另写滤波逻辑。

- [ ] **Step 3: 注册 demo 命令**

  `CommandTable.h` 新增 `linear-filter-demo`（`Label::GUI`，run = `LinearFilterDemo()`）。

- [ ] **Step 4: 写学习笔记 `LinearFilterNotes.md`**

  覆盖用户列的 8 要点：原理、背景、解决的问题、效果、局限、后续改良、工程使用情况、硬件诉求；
  聚焦线性滤波（卷积、高斯/均值、锐化、Sobel 导数），中文。

- [ ] **Step 5: 构建 + 运行 demo 冒烟验证**

  Run: `cmd /c 'call "...vcvars64.bat" && ninja -C out\build\x64-debug camera_utils_tests'`
  然后运行 `camera_utils_tests linear-filter` 确认 `[PASS]`（demo 为 GUI，仅编译验证，不进 ctest）。

- [ ] **Step 6: Commit**

  ```bash
  git add inc/ImageProcessing/LinearFilterConfig.h configs/LinearFilterConfig_README.md
  git commit -m "feat(ImageProcessing): 线性滤波配置序列化"
  git add tests/LinearFilterDemo.h tests/cli/CommandTable.h
  git commit -m "feat(ImageProcessing): 线性滤波交互式调参 demo"
  git add inc/ImageProcessing/LinearFilterNotes.md
  git commit -m "docs(ImageProcessing): 线性滤波学习笔记"
  ```

---

### Task 4: 非线性滤波模块（中值/双边）

**Files:**
- Create: `inc/ImageProcessing/NonLinearFilter.h`
- Create: `inc/ImageProcessing/NonLinearFilterConfig.h`
- Create: `tests/NonLinearFilterTest.h`
- Create: `tests/NonLinearFilterDemo.h`
- Create: `inc/ImageProcessing/NonLinearFilterNotes.md`
- Create: `configs/NonLinearFilterConfig_README.md`
- Modify: `tests/cli/CommandTable.h`（`nonlinear-filter` UNIT + `nonlinear-filter-demo` GUI）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 新增 `nonlinear-filter`）

**Interfaces:**
- Consumes: Task 1 骨架 + Task 3 的 config/序列化模式。
- Produces: `namespace ImageProcessing`：
  - `cv::Mat MedianBlur(const cv::Mat& src, int kernelSize)`
  - `cv::Mat BilateralFilter(const cv::Mat& src, int diameter, double sigmaColor, double sigmaSpace)`
  - `struct NonLinearFilterConfig`（`kernelSize` / `diameter` / `sigmaColor` / `sigmaSpace` / `filterType`）

- [ ] **Step 1: 写失败测试 `tests/NonLinearFilterTest.h`**

  断言：中值滤波对「单点椒盐噪声」能去除噪声（构造中心为 255、周围为 0 的 3×3，`MedianBlur` 后中心 ≈ 0）；
  双边滤波保边（对阶跃边缘，边缘两侧各自保持接近原值，不平坦化）；常数图不变；空输入安全返回。
  真值可用 `cv::medianBlur` / `cv::bilateralFilter` 交叉验证。

- [ ] **Step 2: 运行确认失败 → 实现 `NonLinearFilter.h` → 运行确认通过**

  实现 `MedianBlur`（内部 `cv::medianBlur`）、`BilateralFilter`（内部 `cv::bilateralFilter`），
  空输入提前返回。验证 `[PASS] nonlinear-filter`。

- [ ] **Step 3: 写 config + demo + 笔记，注册命令**

  参照 Task 3 的 `LinearFilterConfig` 与 `LinearFilterDemo` 模式：config 结构体 + 交互式 demo
  （按键切换中值/双边、调核尺寸/σ）+ `NonLinearFilterNotes.md`（中值滤波去椒盐、双边保边降噪）。

- [ ] **Step 4: 构建 + 单测验证**

  Run: `cmd /c '... && ninja -C out\build\x64-debug camera_utils_tests'` + `camera_utils_tests nonlinear-filter`
  Expected: 编译成功，`[PASS] nonlinear-filter`。

- [ ] **Step 5: Commit（逐文件）**

  ```bash
  git add inc/ImageProcessing/NonLinearFilter.h
  git commit -m "feat(ImageProcessing): 非线性滤波模块（中值/双边）"
  git add tests/NonLinearFilterTest.h tests/cli/CommandTable.h tests/CMakeLists.txt
  git commit -m "test(ImageProcessing): 非线性滤波断言测试"
  git add inc/ImageProcessing/NonLinearFilterConfig.h configs/NonLinearFilterConfig_README.md
  git commit -m "feat(ImageProcessing): 非线性滤波配置序列化"
  git add tests/NonLinearFilterDemo.h
  git commit -m "feat(ImageProcessing): 非线性滤波交互式调参 demo"
  git add inc/ImageProcessing/NonLinearFilterNotes.md
  git commit -m "docs(ImageProcessing): 非线性滤波学习笔记"
  ```

---

### Task 5: 金字塔模块（高斯/拉普拉斯）

**Files:**
- Create: `inc/ImageProcessing/Pyramid.h`
- Create: `tests/PyramidTest.h`
- Create: `tests/PyramidDemo.h`
- Create: `inc/ImageProcessing/PyramidNotes.md`
- Modify: `tests/cli/CommandTable.h`（`pyramid` UNIT + `pyramid-demo` GUI）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 新增 `pyramid`）

**Interfaces:**
- Consumes: Task 1 骨架。
- Produces: `namespace ImageProcessing`：
  - `std::vector<cv::Mat> BuildGaussianPyramid(const cv::Mat& src, int levels)`
  - `std::vector<cv::Mat> BuildLaplacianPyramid(const cv::Mat& src, int levels)`
  - （可选）`cv::Mat ReconstructFromLaplacian(const std::vector<cv::Mat>& pyr)`

  金字塔无复杂参数（仅 `levels`），不配 json，用代码内常量（`levels` 作为函数参数传入）。

- [ ] **Step 1: 写失败测试 `tests/PyramidTest.h`**

  断言：`BuildGaussianPyramid` 第 0 层 == 原图，后续层尺寸逐层减半；`BuildLaplacianPyramid`
  层数正确；拉普拉斯重构（若提供）与原图逐像素一致（容差内）；`levels` 过大时安全处理
  （限制到可构建的最大层数，不崩溃——覆盖 Review Focus #4）。

- [ ] **Step 2: 运行确认失败 → 实现 `Pyramid.h` → 运行确认通过**

  实现用 `cv::pyrDown` / `cv::pyrUp`（或 `cv::buildPyramid`）构建高斯/拉普拉斯金字塔，
  层数用 `min(levels, 最大可行层数)` 约束。验证 `[PASS] pyramid`。

- [ ] **Step 3: 写 demo + 笔记，注册命令**

  `PyramidDemo()`：展示金字塔各层（把不同层 resize 到同一尺寸平铺显示）。`PyramidNotes.md`：
  高斯/拉普拉斯金字塔原理、多尺度用途（后续拼接/HDR/光流地基）。

- [ ] **Step 4: 构建 + 单测验证**

  Run: `cmd /c '... && ninja -C out\build\x64-debug camera_utils_tests'` + `camera_utils_tests pyramid`
  Expected: 编译成功，`[PASS] pyramid`。

- [ ] **Step 5: Commit（逐文件）**

  ```bash
  git add inc/ImageProcessing/Pyramid.h
  git commit -m "feat(ImageProcessing): 金字塔模块（高斯/拉普拉斯）"
  git add tests/PyramidTest.h tests/cli/CommandTable.h tests/CMakeLists.txt
  git commit -m "test(ImageProcessing): 金字塔断言测试"
  git add tests/PyramidDemo.h
  git commit -m "feat(ImageProcessing): 金字塔交互式 demo"
  git add inc/ImageProcessing/PyramidNotes.md
  git commit -m "docs(ImageProcessing): 金字塔学习笔记"
  ```

---

### Task 6: 傅里叶变换简单展示 + 章节 README

**Files:**
- Create: `inc/ImageProcessing/FourierDemo.h`（或并入 `tests/FourierDemo.h`，按 GUI demo 定位放 `tests/`）
- Create: `tests/FourierDemo.h`
- Create: `inc/ImageProcessing/README.md`（第 3 章章节 README，API 说明）
- Modify: `tests/cli/CommandTable.h`（注册 `fourier-demo`，`Label::GUI`）

**Interfaces:**
- Consumes: Task 1 骨架。
- Produces: 傅里叶简单展示（频谱可视化、高低频概念），不配 json、不配断言测试（仅展示）。

- [ ] **Step 1: 写 `tests/FourierDemo.h`**

  `FourierDemo()`：读图 → `cv::dft` 计算频谱 → 幅值谱对数缩放 → `imshow` 展示幅度谱与
  高低频掩码滤波效果（低通保留轮廓、高通保留边缘）。仅展示，不做模块化 API。

- [ ] **Step 2: 注册 `fourier-demo` 命令（GUI）**

- [ ] **Step 3: 写章节 `inc/ImageProcessing/README.md`**

  API 说明（与 `<Module>Notes.md` 学习笔记区分）：列本章模块、函数签名、命令名、
  如何构建运行、素材位置。

- [ ] **Step 4: 构建验证**

  Run: `cmd /c '... && ninja -C out\build\x64-debug camera_utils_tests'`
  Expected: 编译成功。

- [ ] **Step 5: Commit（逐文件）**

  ```bash
  git add tests/FourierDemo.h tests/cli/CommandTable.h
  git commit -m "feat(ImageProcessing): 傅里叶变换简单展示 demo"
  git add inc/ImageProcessing/README.md
  git commit -m "docs(ImageProcessing): 第 3 章章节 README"
  ```

---

### Task 7: 全量验证 + 收尾

**Files:**
- 无新增/修改（验证与推送）

**Interfaces:**
- Consumes: Task 1~6 全部产物。

- [ ] **Step 1: 全量构建**

  Run: `cmd /c 'call "...vcvars64.bat" && ninja -C out\build\x64-debug'`
  Expected: 无 error。

- [ ] **Step 2: 全量单测**

  Run: `cmd /c 'ctest --test-dir out/build/x64-debug -L unit --output-on-failure'`
  Expected: `100% tests passed`（含新增 `linear-filter` / `nonlinear-filter` / `pyramid`）。

- [ ] **Step 3: 确认同步状态**

  Run: `git rev-list --left-right --count origin/main...main`
  Expected: `0 0`（推送后确认）。

---

## Self-Review 备注

- Spec 覆盖：三大模块（线性/非线性/金字塔）+ 傅里叶展示 + 目录重构，均有对应任务。
- 模板先行：Task 2~3（线性滤波）锁定结构，Task 4（非线性）、Task 5（金字塔）复用同一骨架，
  Task 6 收尾傅里叶 + README。
- 类型一致：`namespace ImageProcessing`、`LinearFilterConfig`/`NonLinearFilterConfig` 的
  `DefaultPath()`/`LoadFromFile`/`SaveToFile` 命名在 Task 3/4 间一致。
- Review Focus 5 条：空输入（Task 2/4 测试）、类型/通道（Task 2 测试）、核奇数（Task 2/4 文档）、
  金字塔层数越界（Task 5 测试）、config 缺失/损坏（Task 3/4 的 `LoadFromFile` 逻辑）。
