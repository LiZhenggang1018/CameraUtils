# 第 11 章 SfM & SLAM 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，实现第 11 章 SfM & SLAM 的四个模块（相机标定 / 对极几何 / 三角化 / PnP），BA 仅笔记。工程完全空白，全新。

**Architecture:** 章节目录 `inc/SfM/` + `src/SfM/` + `tests/SfM/`，`namespace SfM`。测试用合成数据 + 已知真值
精确断言；demo 数值输出为主。

**Tech Stack:** C++20, OpenCV 4.10（calib3d）, MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码。
- 目录：第 11 章 → `inc/SfM/` + `src/SfM/` + `tests/SfM/`。
- 命名：`namespace SfM`，类型/函数大驼峰，变量小驼峰。
- include guard：`INC_SFM_<NAME>_H`；测试 guard `TESTS_SFM_<NAME>_H`。
- 测试 = `Label::UNIT`（合成真值精确断言）；demo = `Label::UNIT` 或 `Label::GUI`（对极线可视化用 GUI）。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：空 vector / 空 Mat → 各函数安全返回，不崩溃。
2. **点数量边界**：标定/对极/三角化需足够点（标定 ≥ 若干图×棋盘角点，E/F 需 ≥8 点，三角化需 ≥2 视图），不足时降级。
3. **矩阵维度**：E/F 是 3×3，K 是 3×3，R/t 是 3×3/3×1，断言类型与维度。
4. **三角化深度符号**：三角化出的 3D 点应在相机前方（z>0），断言非退化。
5. **PnP 退化**：共面 3D 点会导致 PnP 退化，需用非共面点。

---

## 模块结构与接口约定（锁定）

统一 `namespace SfM`：

### 模块 A：相机标定（`Calibration`）
- 头文件 `inc/SfM/Calibration.h`
- 接口：
  - `bool CalibrateCamera(const std::vector<std::vector<cv::Point3f>>& objectPoints, const std::vector<std::vector<cv::Point2f>>& imagePoints, cv::Size imageSize, cv::Mat& K, cv::Mat& distCoeffs)`
- 测试 `tests/SfM/CalibrationTest.h`（`calibration`，UNIT）
- 笔记 `inc/SfM/CalibrationNotes.md`

### 模块 B：对极几何（`Epipolar`）
- 头文件 `inc/SfM/Epipolar.h`
- 接口：
  - `cv::Mat ComputeFundamental(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2)`
  - `cv::Mat ComputeEssential(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2, const cv::Mat& K)`
- 测试 `tests/SfM/EpipolarTest.h`（`epipolar`，UNIT）
- 笔记 `inc/SfM/EpipolarNotes.md`

### 模块 C：三角化（`Triangulation`）
- 头文件 `inc/SfM/Triangulation.h`
- 接口：
  - `cv::Mat TriangulatePoints(const cv::Mat& P1, const cv::Mat& P2, const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2)`
- 测试 `tests/SfM/TriangulationTest.h`（`triangulation`，UNIT）
- 笔记 `inc/SfM/TriangulationNotes.md`

### 模块 D：PnP（`Pnp`）
- 头文件 `inc/SfM/Pnp.h`
- 接口：
  - `bool SolvePnp(const std::vector<cv::Point3f>& objectPoints, const std::vector<cv::Point2f>& imagePoints, const cv::Mat& K, const cv::Mat& distCoeffs, cv::Mat& R, cv::Mat& t)`
- 测试 `tests/SfM/PnpTest.h`（`pnp`，UNIT）
- 笔记 `inc/SfM/PnpNotes.md`

### BA（仅笔记）
- `inc/SfM/BundleAdjustmentNotes.md`

---

### Task 1: 建 SfM 骨架

- [ ] **Step 1: 建 inc/SfM、src/SfM、tests/SfM 三目录**
- [ ] **Step 2: 更新 src/CMakeLists.txt**（加 `inc/SfM` include + GLOB `src/SfM/*.cpp`）
- [ ] **Step 3: 构建验证**
- [ ] **Step 4: Commit（逐文件）**

---

### Task 2: 相机标定模块

- [ ] **Step 1: 写失败测试 `CalibrationTest.h`**

  合成数据：已知内参 K（如 fx=fy=800, cx=320, cy=240）生成棋盘角点的多视角投影，
  `CalibrateCamera` 恢复的 K 接近真值（fx/fy/cx/cy 误差 < 阈值）。

- [ ] **Step 2: RED → GREEN**（`cv::calibrateCamera`）
- [ ] **Step 3: 写 `CalibrationNotes.md`**
- [ ] **Step 4: 注册命令 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 3: 对极几何模块

- [ ] **Step 1: 写失败测试 `EpipolarTest.h`**

  合成数据：已知 E（从已知 R/t 构造）生成两视图对应点，`ComputeEssential` 恢复的 E 接近真值；
  `ComputeFundamental` 满足 x'ᵀ F x = 0（对极约束）。

- [ ] **Step 2: RED → GREEN**（`findEssentialMat` / `findFundamentalMat`）
- [ ] **Step 3: 写 `EpipolarNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 4: 三角化模块

- [ ] **Step 1: 写失败测试 `TriangulationTest.h`**

  合成数据：已知投影矩阵 P1/P2 与 3D 点，投影到两视图得 2D 点，`TriangulatePoints` 重建的 3D 点
  接近真值（归一化后误差 < 阈值），且深度 z > 0。

- [ ] **Step 2: RED → GREEN**（`triangulatePoints`）
- [ ] **Step 3: 写 `TriangulationNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 5: PnP 模块

- [ ] **Step 1: 写失败测试 `PnpTest.h`**

  合成数据：已知 R/t 与非共面 3D 点，投影得 2D 点，`SolvePnp` 恢复的 R/t 接近真值（重投影误差小）。

- [ ] **Step 2: RED → GREEN**（`solvePnP`）
- [ ] **Step 3: 写 `PnpNotes.md`**
- [ ] **Step 4: 注册 + 构建 + 单测**
- [ ] **Step 5: Commit（逐文件）**

---

### Task 6: BA 笔记 + 章节 README + 全量验证

- [ ] **Step 1: 写 `BundleAdjustmentNotes.md`**（BA 原理，仅笔记）
- [ ] **Step 2: 写 `inc/SfM/README.md`**（章节 README）
- [ ] **Step 3: 全量构建 + ctest + 编码校验**
- [ ] **Step 4: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：四块 + BA 笔记，均有对应任务。
- 合成真值：每块用已知内参/位姿生成数据，精确断言（比前几章的非空断言更严格）。
- Review Focus 5 条：空输入、点数量边界、矩阵维度、三角化深度、PnP 退化，均有对应测试。
