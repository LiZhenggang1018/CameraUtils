# 第 12 章 Depth Estimation 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，实现第 12 章 Depth Estimation 的三个模块（极线校正 / 双目立体匹配 / 深度图+虚化），最终做「双摄虚化 / 人像模式」大 demo。工程完全空白，全新。

**Architecture:** 章节目录 `inc/Depth/` + `src/Depth/` + `tests/Depth/`，`namespace Depth`。测试用合成图 + 已知深度
真值精确断言；demo 用合成图 + 可选真实手机左右照片（未标定校正链路）。

**Tech Stack:** C++20, OpenCV 4.10（calib3d/ximgproc）, MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码。
- 目录：第 12 章 → `inc/Depth/` + `src/Depth/` + `tests/Depth/`。
- 命名：`namespace Depth`，类型/函数大驼峰，变量小驼峰。
- include guard：`INC_DEPTH_<NAME>_H`；测试 guard `TESTS_DEPTH_<NAME>_H`。
- 测试 = `Label::UNIT`（合成真值精确断言）；demo = `Label::UNIT`（数值）或 `Label::GUI`（虚化可视化）。
- 复用第 11 章 `SfM::ComputeFundamental`（未标定校正的输入）。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：空 Mat / 空点集 → 各函数安全返回，不崩溃。
2. **视差范围**：StereoBM/SGBM 有视差范围 numDisparities（须 16 的倍数），参数需校验。
3. **视差 → 深度除零**：视差为 0 处深度无穷大，需防护（掩码或 clamp）。
4. **未标定校正失败**：特征不足/退化时 `stereoRectifyUncalibrated` 失败，需降级。
5. **虚化边界**：深度图驱动虚化时，前景/背景边界需按深度分层，深度噪声会导致边界闪烁。

---

## 模块结构与接口约定（锁定）

统一 `namespace Depth`：

### 模块 A：极线校正（`Rectification`）
- 头文件 `inc/Depth/Rectification.h`
- 接口：
  - `bool RectifyUncalibrated(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2, cv::Size imageSize, cv::Mat& H1, cv::Mat& H2)`
    （内部用 `SfM::ComputeFundamental` 求 F，再 `stereoRectifyUncalibrated` 求校正单应 H1/H2）
- 测试 `tests/Depth/RectificationTest.h`（`rectification`，UNIT）
- 笔记 `inc/Depth/RectificationNotes.md`

### 模块 B：双目立体匹配（`StereoMatching`）
- 头文件 `inc/Depth/StereoMatching.h`
- 接口：
  - `cv::Mat ComputeDisparity(const cv::Mat& left, const cv::Mat& right, int numDisparities, int blockSize)`（`StereoSGBM`）
  - `cv::Mat DisparityToDepth(const cv::Mat& disparity, double focalLength, double baseline)`（视差→深度，除零防护）
- 测试 `tests/Depth/StereoMatchingTest.h`（`stereo-matching`，UNIT）
- 笔记 `inc/Depth/StereoMatchingNotes.md`

### 模块 C：深度图 + 虚化（`Bokeh`）
- 头文件 `inc/Depth/Bokeh.h`
- 接口：
  - `cv::Mat ApplyBokeh(const cv::Mat& image, const cv::Mat& depth, double focusDepth, double blurStrength)`
    （按深度分层：聚焦深度附近清晰，远离聚焦深度的区域按深度差异模糊，模拟大光圈虚化）
- 测试 `tests/Depth/BokehTest.h`（`bokeh`，UNIT）
- demo `tests/Depth/BokehDemo.h`（`bokeh-demo`，GUI）
- 笔记 `inc/Depth/BokehNotes.md`

---

### Task 1: 建 Depth 骨架

- [ ] **Step 1: 建 inc/Depth、src/Depth、tests/Depth 三目录**
- [ ] **Step 2: 更新 src/CMakeLists.txt**（加 `inc/Depth` include + GLOB `src/Depth/*.cpp`）
- [ ] **Step 3: 构建验证**
- [ ] **Step 4: Commit（逐文件）**

---

### Task 2: 极线校正模块

- [ ] **Step 1: 写失败测试 `RectificationTest.h`**

  合成数据：已知 F 的两视图对应点，`RectifyUncalibrated` 返回 H1/H2，校正后对应点 y 坐标对齐
  （对极线水平，y 差 ≈ 0）。

- [ ] **Step 2: RED → GREEN**（`stereoRectifyUncalibrated`）
- [ ] **Step 3: 写 `RectificationNotes.md`**
- [ ] **Step 4: 注册命令 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 3: 双目立体匹配模块

- [ ] **Step 1: 写失败测试 `StereoMatchingTest.h`**

  合成数据：已知视差（前景/背景不同深度的合成左右图），`ComputeDisparity` 返回非空视差图，
  前景视差 > 背景视差；`DisparityToDepth` 深度与视差成反比，视差 0 处深度防护。

- [ ] **Step 2: RED → GREEN**（`StereoSGBM` + 视差转深度）
- [ ] **Step 3: 写 `StereoMatchingNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 4: 深度图 + 虚化模块

- [ ] **Step 1: 写失败测试 `BokehTest.h`**

  合成数据：已知深度图（前景近、背景远），`ApplyBokeh` 聚焦前景时背景模糊、前景清晰。

- [ ] **Step 2: RED → GREEN**（深度分层 + 高斯模糊合成虚化）
- [ ] **Step 3: 写 `BokehNotes.md`**
- [ ] **Step 4: 写 `BokehDemo.h`（可视化虚化）+ 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 5: 双摄虚化大 demo + 章节 README + 全量验证

- [ ] **Step 1: 写 `tests/Depth/PortraitDemo.h`**（完整链路：真实手机左右照片 → 未标定校正 → SGBM 视差 → 深度 → 虚化）
- [ ] **Step 2: 写 `inc/Depth/README.md`**（章节 README）
- [ ] **Step 3: 全量构建 + ctest + 编码校验**
- [ ] **Step 4: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：三块 + 双摄虚化大 demo，均有对应任务。
- 复用：极线校正复用 `SfM::ComputeFundamental`（第 11 章）。
- 合成真值：视差/深度有已知真值，精确断言（比前几章非空断言更严格）。
- Review Focus 5 条：空输入、视差范围（numDisparities 16 倍数）、视差除零、未标定校正退化、虚化边界，均有对应测试。
