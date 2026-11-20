# 第 8 章 Alignment & Stitching 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，实现第 8 章 Alignment & Stitching 的四个模块（单应+warping / 图像融合 / 曝光补偿 / 渐晕校正），最终做「多图全景拼接」大 demo。

**Architecture:** 四个模块统一 `inc/Alignment/`（接口）+ `src/Alignment/`（实现）+ `tests/Alignment/`（测试与 demo），
`namespace Alignment`。配准复用第 7 章 `FeatureMatching` 三块（`RansacHomography` 等），融合复用第 3 章
`ImageProcessing::BuildLaplacianPyramid`。`ImageRegistrator` 作对照参考。

**Tech Stack:** C++20, OpenCV 4.10, nlohmann::json, MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码。
- 代码定位 = 可复用库（`inc/Alignment/` 接口 + `src/Alignment/` 实现）；demo 严格放 `tests/Alignment/`。
- 命名：`namespace Alignment`，类型/函数大驼峰，变量小驼峰，类成员 `_` 后缀。
- include guard：`INC_ALIGNMENT_<NAME>_H`；测试 guard `TESTS_ALIGNMENT_<NAME>_H`。
- 调参 demo = `Label::GUI`（不进 ctest）；测试 = `Label::UNIT`。
- json 配置 = 仅参数有意义的模块配。
- 数据目录 = `data/Alignment/<Module>/`（本地，不入库）。
- 复用既有模块：`FeatureMatching::{MatchBF, RatioTestFilter, RansacHomography}`、
  `FeatureMatching::{CreateOrb, DetectAndCompute}`、`ImageProcessing::BuildLaplacianPyramid`。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：`cv::Mat` 空 → 各函数安全返回，不崩溃。
2. **单应求逆/方向**：`findHomography` 返回的方向约定（src→dst），warp 时必须方向正确，否则图 warped 反。
3. **warp 边界扩展**：`warpPerspective` 输出画布尺寸需容纳变换后的图像，否则内容被裁剪。
4. **融合掩码**：多波段 blending 时两张图的权重掩码和必须为 1（重叠区），否则亮度异常。
5. **曝光补偿除零**：增益估计时分母为 0（纯黑重叠区）需防护。
6. **渐晕模型参数**：径向衰减模型参数需合理，避免除零或负半径。

---

## 模块结构与接口约定（锁定）

### 模块 A：单应与 warping（`Homography`）
- 头文件 `inc/Alignment/Homography.h`
- 接口（`namespace Alignment`）：
  - `cv::Mat ComputeHomography(const cv::Mat& img1, const cv::Mat& img2, std::vector<cv::DMatch>& inliers)`
    （内部复用第 7 章 ORB 检测 + BFMatch + ratio test + RansacHomography）
  - `cv::Mat WarpImage(const cv::Mat& src, const cv::Mat& h, cv::Size canvasSize)`
    （`warpPerspective`，返回指定画布尺寸的 warped 图）
- 测试 `tests/Alignment/HomographyTest.h`（`homography`，UNIT）
- demo `tests/Alignment/HomographyDemo.h`（`homography-demo`，GUI）
- 笔记 `inc/Alignment/HomographyNotes.md`

### 模块 B：图像融合（`Blending`）
- 头文件 `inc/Alignment/Blending.h`
- 接口：
  - `cv::Mat MultiBandBlend(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask1, const cv::Mat& mask2, int levels)`
    （拉普拉斯金字塔融合，复用 `ImageProcessing::BuildLaplacianPyramid`）
- 测试 `tests/Alignment/BlendingTest.h`（`blending`，UNIT）
- demo `tests/Alignment/BlendingDemo.h`（`blending-demo`，GUI）
- 笔记 `inc/Alignment/BlendingNotes.md`

### 模块 C：曝光补偿（`ExposureCompensation`）
- 头文件 `inc/Alignment/ExposureCompensation.h`
- 接口：
  - `std::vector<double> EstimateGains(const std::vector<cv::Mat>& images)`（基于重叠区的增益估计）
  - `cv::Mat ApplyGain(const cv::Mat& image, double gain)`
- 测试 `tests/Alignment/ExposureCompensationTest.h`（`exposure-comp`，UNIT）
- demo `tests/Alignment/ExposureCompensationDemo.h`（`exposure-comp-demo`，GUI）
- 笔记 `inc/Alignment/ExposureCompensationNotes.md`

### 模块 D：渐晕校正（`Vignetting`）
- 头文件 `inc/Alignment/Vignetting.h`
- 接口：
  - `cv::Mat CorrectVignetting(const cv::Mat& image, double strength)`
    （径向衰减模型：`I' = I / (1 + a·r²)` 或乘性校正）
- 测试 `tests/Alignment/VignettingTest.h`（`vignetting`，UNIT）
- demo `tests/Alignment/VignettingDemo.h`（`vignetting-demo`，GUI）
- 笔记 `inc/Alignment/VignettingNotes.md`

---

### Task 1: 建 Alignment 骨架

**Files:**
- Create: `inc/Alignment/`、`src/Alignment/`、`tests/Alignment/`（目录）
- Modify: `src/CMakeLists.txt`（加 `inc/Alignment` include + GLOB `src/Alignment/*.cpp`）

- [ ] **Step 1: 建三目录**
- [ ] **Step 2: 更新 src/CMakeLists.txt**
- [ ] **Step 3: 构建验证**
- [ ] **Step 4: Commit（逐文件）**

---

### Task 2: 单应与 warping 模块

- [ ] **Step 1: 写失败测试 `HomographyTest.h`**

  断言：
  1. 对同一图做已知单应（旋转+平移），`ComputeHomography` 返回非空 H，内点数 ≥ 4。
  2. H 重投影误差：把 img1 关键点经 H 投影到 img2，误差 < 阈值。
  3. `WarpImage` 输出画布尺寸正确、非空。
  4. 空输入安全返回空。

- [ ] **Step 2: 运行确认失败 → 实现 → 运行确认通过**

  `ComputeHomography` 内部：`DetectAndCompute(CreateOrb)` × 2 → `MatchBF(NORM_HAMMING)` → knnMatch → `RatioTestFilter` → `RansacHomography`。
  `WarpImage`：`cv::warpPerspective`。

- [ ] **Step 3: demo + 笔记 + 注册命令**

- [ ] **Step 4: 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 3: 图像融合（多波段 blending）模块

- [ ] **Step 1: 写失败测试 `BlendingTest.h`**

  断言：
  1. `MultiBandBlend` 对「同一张图拆成两半 + 重叠区」融合，结果与原因接近（重叠区无接缝）。
  2. mask 和 = 1 的重叠区，融合亮度正常（不偏亮/偏暗）。
  3. 空输入安全返回空。

- [ ] **Step 2: RED→GREEN**（拉普拉斯金字塔融合）

- [ ] **Step 3: demo + 笔记 + 注册**

- [ ] **Step 4: 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 4: 曝光补偿模块

- [ ] **Step 1: 写失败测试 `ExposureCompensationTest.h`**

  断言：
  1. 两张亮度不同的图（同一场景不同曝光），`EstimateGains` 估计的增益使两图均值趋近。
  2. `ApplyGain` 对常数图乘以 gain 后亮度正确缩放。
  3. 除零防护：全黑图不崩溃。

- [ ] **Step 2: RED→GREEN**

- [ ] **Step 3: demo + 笔记 + 注册**

- [ ] **Step 4: 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 5: 渐晕校正模块

- [ ] **Step 1: 写失败测试 `VignettingTest.h`**

  断言：
  1. 构造已知径向衰减图（中心亮、边缘暗），`CorrectVignetting` 校正后边缘亮度提升。
  2. 中心区域基本不变（增益≈1）。
  3. 空输入安全。

- [ ] **Step 2: RED→GREEN**

- [ ] **Step 3: demo + 笔记 + 注册**

- [ ] **Step 4: 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 6: 多图全景拼接大 demo

**Files:**
- Create: `inc/Alignment/Panorama.h`（拼接核心：配准 + 累计单应 + 融合 + 曝光补偿 + 渐晕校正的组合）
- Create: `tests/Alignment/PanoramaTest.h`（`panorama`，UNIT，合成多图拼接断言）
- Create: `tests/Alignment/PanoramaDemo.h`（`panorama-demo`，GUI，真实多图拼接）
- Create: `inc/Alignment/README.md`（章节 README）

- [ ] **Step 1: 写失败测试 `PanoramaTest.h`**

  断言：
  1. 对一组「已知拼接关系的合成图」（同一张大图切成重叠的若干张），拼接结果覆盖完整场景（尺寸合理）。
  2. 拼接结果非空，重叠区无明显接缝（可选：与真值大图对比 MSE < 阈值）。

- [ ] **Step 2: RED→GREEN**（实现 Panorama 组合流程）

- [ ] **Step 3: PanoramaDemo（真实多图）+ README**

- [ ] **Step 4: 全量验证（构建 + ctest + 编码校验）**

- [ ] **Step 5: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：四模块 + 全景大 demo，均有对应任务。
- 复用：配准复用 `FeatureMatching`，融合复用 `ImageProcessing::BuildLaplacianPyramid`，`ImageRegistrator` 作对照。
- 类型/方向：`findHomography` 方向约定在 Homography 模块笔记与注释中明确（Review Focus #2）。
- Review Focus 6 条：空输入（各模块测试）、单应方向（Task 2）、warp 画布（Task 2）、融合掩码（Task 3）、
  曝光除零（Task 4）、渐晕参数（Task 5）。
