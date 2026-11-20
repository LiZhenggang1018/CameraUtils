# 第 9 章 Motion Estimation 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，完成第 9 章 Motion Estimation：
把已有光流代码「学习化」（补笔记 + 断言测试），并做「视频 EIS 防抖」大 demo（全局运动估计双路线 + 轨迹平滑 + 稳定化裁剪）。

**Architecture:** 光流部分沿用 `inc/OpticalFlow/` + `src/OpticalFlow/`（生产代码），补 `*Notes.md` + 断言测试；
EIS 防抖新增 `inc/OpticalFlow/Eis.h`（稳定化核心）。测试统一 `tests/OpticalFlow/`。

**Tech Stack:** C++20, OpenCV 4.10（video/optflow/ximgproc）, FFmpeg, nlohmann::json, MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码。
- 代码定位：光流生产代码已有（`.h`+`.cpp`），本章补笔记 + 测试；EIS 新增 `Eis.h`。
- 目录：第 9 章 → `inc/OpticalFlow/` + `src/OpticalFlow/` + `tests/OpticalFlow/`。
- `tests/CommonUtils/OpticalFlowTest.h` 移到 `tests/OpticalFlow/`（按章节归档）。
- 命名：类型/函数大驼峰，变量小驼峰，类成员 `_` 后缀。
- include guard：`INC_OPTICALFLOW_<NAME>_H`；测试 guard `TESTS_OPTICALFLOW_<NAME>_H`。
- 调参 demo = `Label::GUI`；测试 = `Label::UNIT`。
- 数据目录 = `data/OpticalFlow/<Module>/`（本地，不入库）。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：空 `cv::Mat` / 空帧 → 各函数安全返回，不崩溃。
2. **光流类型**：LK 输出 `Point2f`、稠密输出 `CV_32FC2` 光流场（`dx,dy` 两通道），断言类型正确。
3. **EIS 轨迹平滑**：运动轨迹平滑后不能产生跳变；窗口裁剪不能越界（裁剪尺寸 < 原图）。
4. **EIS 全局运动退化**：纹理弱/纯色帧 `findTransformECC` 可能失败，需降级到恒等变换。
5. **插帧时间参数**：`t ∈ [0,1]`，越界需钳制。

---

## 模块结构与接口约定（锁定）

### 光流学习化（3 块，补笔记 + 断言测试）

#### 模块 A：稀疏光流（LK）
- 笔记 `inc/OpticalFlow/LkTrackerNotes.md`
- 测试 `tests/OpticalFlow/LkTrackerTest.h`（`lk-tracker`，UNIT）
- 断言：`LkTracker` 在平移两帧上能跟踪点、`Track` 后 `TrackedCount() > 0`、FB 过滤生效、空帧安全。

#### 模块 B：稠密光流
- 笔记 `inc/OpticalFlow/DenseFlowNotes.md`
- 测试 `tests/OpticalFlow/DenseFlowTest.h`（`dense-flow`，UNIT）
- 断言：`DenseFlowFarneback::Compute` 返回 `CV_32FC2`、平移两帧的光流场均指向平移方向、
  DIS/DeepFlow 同接口、空帧安全。

#### 模块 C：视频插帧
- 笔记 `inc/OpticalFlow/FrameInterpolatorNotes.md`
- 测试 `tests/OpticalFlow/FrameInterpolatorTest.h`（`frame-interp`，UNIT）
- 断言：`Interpolate(prev, curr, 0.5)` 返回中间帧、尺寸正确、t 越界钳制、空帧安全。

### EIS 大 demo（新）

#### 模块 D：EIS 防抖核心
- 头文件 `inc/OpticalFlow/Eis.h`（`namespace OpticalFlow`，或独立类）
- 接口：
  - `cv::Mat EstimateGlobalMotionOptical(const cv::Mat& prev, const cv::Mat& curr)`（光流法：`findTransformECC` 或光流场拟合仿射）
  - `cv::Mat EstimateGlobalMotionFeature(const cv::Mat& prev, const cv::Mat& curr)`（特征法：复用 `Alignment::ComputeHomography`）
  - `cv::Mat Stabilize(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& motions)`（轨迹平滑 + 稳定化裁剪）
- 测试 `tests/OpticalFlow/EisTest.h`（`eis`，UNIT）
- demo `tests/OpticalFlow/EisDemo.h`（`eis-demo`，GUI）
- 笔记 `inc/OpticalFlow/EisNotes.md`

---

### Task 1: OpticalFlowTest.h 归档 + 建测试目录

**Files:**
- Move: `tests/CommonUtils/OpticalFlowTest.h` → `tests/OpticalFlow/OpticalFlowTest.h`
- Create: `tests/OpticalFlow/`（目录）
- Modify: `tests/cli/CommandTable.h`（include 路径改为 `OpticalFlow/OpticalFlowTest.h`）
- Modify: `tests/CMakeLists.txt`（若 OpticalFlowTest 有注册的测试命令，确认路径）

- [ ] **Step 1: 建 tests/OpticalFlow 目录 + 移动文件**
- [ ] **Step 2: 更新 CommandTable.h include 路径**
- [ ] **Step 3: 构建验证**
- [ ] **Step 4: Commit（逐文件）**

---

### Task 2: 稀疏光流（LK）学习化

- [ ] **Step 1: 写失败测试 `LkTrackerTest.h`**（平移合成图 + 已知点跟踪 + FB 过滤 + 空帧安全）
- [ ] **Step 2: RED → GREEN**（LkTracker 已存在，测试通过即 GREEN；若发现 bug 修复）
- [ ] **Step 3: 写 `LkTrackerNotes.md`**
- [ ] **Step 4: 注册命令 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 3: 稠密光流学习化

- [ ] **Step 1: 写失败测试 `DenseFlowTest.h`**（CV_32FC2 类型 + 平移方向 + 三后端 + 空帧安全）
- [ ] **Step 2: RED → GREEN**
- [ ] **Step 3: 写 `DenseFlowNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 4: 视频插帧学习化

- [ ] **Step 1: 写失败测试 `FrameInterpolatorTest.h`**（中间帧 + 尺寸 + t 钳制 + 空帧安全）
- [ ] **Step 2: RED → GREEN**
- [ ] **Step 3: 写 `FrameInterpolatorNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 5: EIS 防抖核心

- [ ] **Step 1: 写失败测试 `EisTest.h`**

  断言：
  1. `EstimateGlobalMotionOptical` 对「平移两帧」返回接近平移的仿射矩阵（非空、3×3 或 2×3）。
  2. `EstimateGlobalMotionFeature` 同样返回有效变换。
  3. 两条路线的运动估计在平移场景下结果接近（对比）。
  4. `Stabilize` 输出帧尺寸 <= 原帧（裁剪不越界）、非空。
  5. 空输入安全。

- [ ] **Step 2: RED → GREEN**（实现 `Eis.h` 两个估计函数 + `Stabilize`）
- [ ] **Step 3: 写 `EisNotes.md`**
- [ ] **Step 4: 写 `EisDemo.h`（真实视频防抖）+ 注册命令 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 6: 章节 README + 全量验证

- [ ] **Step 1: 更新/补 `inc/OpticalFlow/README.md`**（加入学习化笔记索引 + EIS）
- [ ] **Step 2: 全量构建**
- [ ] **Step 3: 全量单测**（ctest -L unit）
- [ ] **Step 4: 编码校验**（check-encoding.ps1）
- [ ] **Step 5: 确认同步状态 `0 0`**
- [ ] **Step 6: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：光流学习化 3 块 + EIS 大 demo，均有对应任务。
- 复用：EIS 特征法复用 `Alignment::ComputeHomography`（第 8 章），光流法用 `findTransformECC`。
- 已有生产代码（LkTracker/DenseFlow/FrameInterpolator）不重写，只补笔记 + 测试（学习化）。
- Review Focus 5 条：空输入（各模块测试）、光流类型（Task 3）、EIS 轨迹平滑/裁剪越界（Task 5）、
  全局运动退化（Task 5）、插帧 t 钳制（Task 4）。
