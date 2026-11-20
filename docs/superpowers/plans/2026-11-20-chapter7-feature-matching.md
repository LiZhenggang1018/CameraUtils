# 第 7 章 Feature Detection & Matching 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，实现第 7 章 Feature Detection & Matching 的三个模块（关键点检测 / 局部特征 / 匹配+RANSAC），每个模块内聚「学习笔记 + 调参 demo + 断言测试 + 可复用代码」。同时删除旧 `inc/ImageReg/` 目录。

**Architecture:** 复用第 3 章确立的 self-contained 模块骨架。三块共用 `inc/FeatureMatching/`
（接口）+ `src/FeatureMatching/`（实现）+ `tests/FeatureMatching/`（测试与 demo），统一
`namespace FeatureMatching`。测试用「合成图 + 已知几何变换」自动断言；demo 用真实图 + 合成图。

**Tech Stack:** C++20, OpenCV 4.10（features2d / imgproc / imgcodecs / highgui）, nlohmann::json,
MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码，围绕同一份代码。
- 代码定位 = 可复用库（`inc/` 接口 + `src/` 实现）；demo 严格放 `tests/`。
- 目录按章节：第 7 章 → `inc/FeatureMatching/` + `src/FeatureMatching/` + `tests/FeatureMatching/`。
- 命名：`namespace FeatureMatching`，类型/函数大驼峰，变量小驼峰，类成员 `_` 后缀。
- include guard：`INC_FEATUREMATCHING_<NAME>_H`；测试 guard `TESTS_FEATUREMATCHING_<NAME>_H`。
- 调参 demo = 交互式 `imshow`（`Label::GUI`，不进 ctest）；测试 = `Label::UNIT`。
- json 配置 = 仅参数有意义的模块配（参考 `HdrConfig.h` 的 `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`）。
- 数据目录 = `data/FeatureMatching/<Module>/`（本地，不入库）。
- 可运行算法：SIFT/ORB/AKAZE（主库 features2d）+ Harris/GFTT/FAST（主库）。SURF 仅笔记讲述。
- 格式：提交前 `clang-format -i --style=file <file>`。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`（每次领先 1 个）。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：`cv::Mat` 为空时，检测/匹配函数安全返回空结果，不崩溃。
2. **关键点数量边界**：合成图上特征点数量可能为 0 或极少（如平坦图），函数须返回空 vector 而非崩溃。
3. **描述子范数匹配**：ORB/AKAZE 用 Hamming（`NORM_HAMMING`），SIFT 用 L2（`NORM_L2`），
   BFMatcher 的 normType 必须与描述子类型一致，否则匹配错误或崩溃。
4. **RANSAC 内点不足**：内点少于 4 个时 `findHomography` 返回空 Mat，调用方须检查而非直接 `warp`。
5. **ratio test 边界**：knnMatch 返回结果不足 2 个时（k=2 但只有一个匹配），访问 `[1]` 会越界。

---

## 模块结构与接口约定（锁定，后续任务引用）

统一 `namespace FeatureMatching`，三个模块：

### 模块 A：关键点检测（`KeypointDetector`）
- 头文件 `inc/FeatureMatching/KeypointDetector.h`
- 接口：
  - `std::vector<cv::KeyPoint> HarrisCorners(const cv::Mat& gray, int blockSize, int ksize, double k, double threshold)`
  - `std::vector<cv::KeyPoint> ShiTomasiCorners(const cv::Mat& gray, int maxCorners, double qualityLevel, double minDistance)`
  - `std::vector<cv::KeyPoint> FastCorners(const cv::Mat& gray, int threshold, bool nonmaxSuppression)`
- 测试 `tests/FeatureMatching/KeypointDetectorTest.h`（命令 `keypoint-detect`，UNIT）
- demo `tests/FeatureMatching/KeypointDetectorDemo.h`（命令 `keypoint-detect-demo`，GUI）

### 模块 B：局部特征（`LocalFeature`）
- 头文件 `inc/FeatureMatching/LocalFeature.h`
- 接口（统一返回 `cv::Ptr<cv::Feature2D>` 或封装结构）：
  - `cv::Ptr<cv::SIFT> CreateSift(int nfeatures)`
  - `cv::Ptr<cv::ORB> CreateOrb(int nfeatures)`
  - `cv::Ptr<cv::AKAZE> CreateAkaze()`
  - `void DetectAndCompute(const cv::Ptr<cv::Feature2D>&, const cv::Mat&, std::vector<cv::KeyPoint>&, cv::Mat&)`
- 测试 `tests/FeatureMatching/LocalFeatureTest.h`（命令 `local-feature`，UNIT）
- demo `tests/FeatureMatching/LocalFeatureDemo.h`（命令 `local-feature-demo`，GUI）
- 笔记 `inc/FeatureMatching/LocalFeatureNotes.md`（含 SURF 原理讲述）

### 模块 C：匹配 + RANSAC（`FeatureMatcher`）
- 头文件 `inc/FeatureMatching/FeatureMatcher.h`
- 接口：
  - `std::vector<cv::DMatch> MatchBF(const cv::Mat& desc1, const cv::Mat& desc2, int normType)`
  - `std::vector<cv::DMatch> MatchFLANN(const cv::Mat& desc1, const cv::Mat& desc2)`
  - `std::vector<cv::DMatch> RatioTestFilter(const std::vector<std::vector<cv::DMatch>>& knnMatches, double ratio)`
  - `std::vector<cv::DMatch> CrossCheckFilter(const std::vector<cv::DMatch>& m1, const std::vector<cv::DMatch>& m2)`
  - `cv::Mat RansacHomography(const std::vector<cv::KeyPoint>& kp1, const std::vector<cv::KeyPoint>& kp2, const std::vector<cv::DMatch>& matches, std::vector<cv::DMatch>& inliers)`
- 测试 `tests/FeatureMatching/FeatureMatcherTest.h`（命令 `feature-match`，UNIT）
- demo `tests/FeatureMatching/FeatureMatcherDemo.h`（命令 `feature-match-demo`，GUI）

---

### Task 1: 删除 ImageReg 目录 + 建 FeatureMatching 骨架

**Files:**
- Delete: `inc/ImageReg/HarrisDetect.h`、`inc/ImageReg/`（整目录）
- Modify: `src/CMakeLists.txt`（删 `inc/ImageReg` include、加 `inc/FeatureMatching` include + GLOB `src/FeatureMatching/*.cpp`）
- Modify: `tests/cli/CommandTable.h`（删 `#include "HarrisDetect.h"` 与 `harris` GUI 命令）
- Create: `inc/FeatureMatching/`（目录）、`src/FeatureMatching/`（目录）、`tests/FeatureMatching/`（目录）

**Interfaces:**
- Consumes: 现有 `CommandTable.h` 里的 `harris` 命令注册与 `HarrisDetect.h` include。
- Produces: 三个空章节目录 + `src/CMakeLists.txt` 已注册新 include/GLOB；`harris` 命令与 `HarrisDetect.h` 移除。

- [ ] **Step 1: 删 HarrisDetect.h 与 ImageReg 目录**

  删除 `inc/ImageReg/HarrisDetect.h`，随后删除空的 `inc/ImageReg/` 目录。

- [ ] **Step 2: 更新 src/CMakeLists.txt**

  删掉 `target_include_directories` 里的 `${CMAKE_SOURCE_DIR}/inc/ImageReg`；新增
  `${CMAKE_SOURCE_DIR}/inc/FeatureMatching`；在 `file(GLOB ...)` 新增 `src/FeatureMatching/*.cpp`。

- [ ] **Step 3: 更新 CommandTable.h**

  删 `#include "HarrisDetect.h"` 与 `.name = "harris"` 的 GUI 命令块。

- [ ] **Step 4: 建目录**

  `New-Item -ItemType Directory inc/FeatureMatching`, `src/FeatureMatching`, `tests/FeatureMatching`。

- [ ] **Step 5: 构建验证**

  Run: `ninja -C out\build\x64-debug camera_utils_tests`
  Expected: 编译成功（`harris` 命令已移除，其余命令不受影响）。

- [ ] **Step 6: Commit（逐文件）**

  ```bash
  git add inc/ImageReg/HarrisDetect.h && git commit -m "refactor(FeatureMatching): 删除旧 HarrisDetect GUI demo" && git push origin main
  git add tests/cli/CommandTable.h && git commit -m "refactor(FeatureMatching): 移除 harris 命令注册" && git push origin main
  git add src/CMakeLists.txt && git commit -m "build(FeatureMatching): 注册 FeatureMatching 目录与移除 ImageReg include" && git push origin main
  ```

---

### Task 2: 关键点检测模块（Harris / Shi-Tomasi / FAST）

**Files:**
- Create: `inc/FeatureMatching/KeypointDetector.h`
- Create: `inc/FeatureMatching/KeypointDetectorNotes.md`
- Create: `tests/FeatureMatching/KeypointDetectorTest.h`
- Create: `tests/FeatureMatching/KeypointDetectorDemo.h`
- Modify: `tests/cli/CommandTable.h`（`keypoint-detect` UNIT + `keypoint-detect-demo` GUI）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 加 `keypoint-detect`）

**Interfaces:**（见模块 A 接口约定）

- [ ] **Step 1: 写失败测试 `KeypointDetectorTest.h`**

  断言（合成图 + 已知角点）：
  1. 构造含明显角点的合成图（如黑白棋盘格 `cv::Mat`，或黑底白矩形），`HarrisCorners` 返回非空、
     且角点集中在角点附近（数量 > 0）。
  2. 平坦图（全灰）→ `HarrisCorners`/`ShiTomasiCorners`/`FastCorners` 返回空或极少（安全，不崩）。
  3. `ShiTomasiCorners` 的 `maxCorners` 参数生效（返回数量 ≤ maxCorners）。
  4. `FastCorners` 非极大值抑制开关影响结果（关闭时返回更多）。
  5. 空输入返回空 vector。

- [ ] **Step 2: 运行确认失败 → 实现 `KeypointDetector.h` → 运行确认通过**

  实现用 `cv::cornerHarris` / `cv::goodFeaturesToTrack` / `cv::FAST`，空输入提前返回。

- [ ] **Step 3: 写 demo + 笔记，注册命令**

  `KeypointDetectorDemo()`：读图/合成图，叠加绘制 Harris/ShiTomasi/FAST 关键点，按键切换算法与阈值。
  `KeypointDetectorNotes.md`：Harris/GFTT/FAST 原理、背景、局限、改良、工程使用、硬件诉求。

- [ ] **Step 4: 构建 + 单测验证**

  Run: `ninja ... && camera_utils_tests keypoint-detect` → `[PASS]`。

- [ ] **Step 5: Commit（逐文件）**

  按 `inc/FeatureMatching/KeypointDetector.h` → `tests/FeatureMatching/KeypointDetectorTest.h` →
  `tests/cli/CommandTable.h` + `tests/CMakeLists.txt` → `tests/FeatureMatching/KeypointDetectorDemo.h` →
  `inc/FeatureMatching/KeypointDetectorNotes.md` 顺序，每个文件 add+commit+push。

---

### Task 3: 局部特征模块（SIFT / ORB / AKAZE，SURF 仅笔记）

**Files:**
- Create: `inc/FeatureMatching/LocalFeature.h`
- Create: `inc/FeatureMatching/LocalFeatureNotes.md`
- Create: `tests/FeatureMatching/LocalFeatureTest.h`
- Create: `tests/FeatureMatching/LocalFeatureDemo.h`
- Modify: `tests/cli/CommandTable.h`（`local-feature` UNIT + `local-feature-demo` GUI）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 加 `local-feature`）

**Interfaces:**（见模块 B 接口约定）

- [ ] **Step 1: 写失败测试 `LocalFeatureTest.h`**

  断言（合成图 + 已知变换）：
  1. `CreateSift`/`CreateOrb`/`CreateAkaze` 返回非空 `Ptr`。
  2. `DetectAndCompute` 在纹理图上返回非空 keypoints + descriptors，且 descriptors 行数 == keypoints 数。
  3. SIFT 描述子是 `CV_32F`（float），ORB/AKAZE 是 `CV_8U`（uchar）——类型断言。
  4. 对同一图「旋转/缩放」后，SIFT/ORB 匹配仍有较高正确率（用 BFMatcher + ratio test 交叉验证，
     内点数量超过阈值）。
  5. 平坦图返回空 keypoints（安全）。

- [ ] **Step 2: 运行确认失败 → 实现 `LocalFeature.h` → 运行确认通过**

  `CreateSift` 用 `cv::SIFT::create`，`CreateOrb` 用 `cv::ORB::create`，`CreateAkaze` 用 `cv::AKAZE::create`。

- [ ] **Step 3: 写 demo + 笔记，注册命令**

  `LocalFeatureDemo()`：读图/合成图，分别用 SIFT/ORB/AKAZE 检测并绘制关键点，按键切换。
  `LocalFeatureNotes.md`：SIFT/SURF/ORB/AKAZE 原理与对比（SURF 重点讲原理与 SIFT 的异同，
  标注「本机无 xfeatures2d，SURF 仅理论」）。

- [ ] **Step 4: 构建 + 单测验证**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 4: 匹配 + RANSAC 模块

**Files:**
- Create: `inc/FeatureMatching/FeatureMatcher.h`
- Create: `inc/FeatureMatching/FeatureMatcherNotes.md`
- Create: `tests/FeatureMatching/FeatureMatcherTest.h`
- Create: `tests/FeatureMatching/FeatureMatcherDemo.h`
- Modify: `tests/cli/CommandTable.h`（`feature-match` UNIT + `feature-match-demo` GUI）
- Modify: `tests/CMakeLists.txt`（`_unit_tests` 加 `feature-match`）

**Interfaces:**（见模块 C 接口约定）

- [ ] **Step 1: 写失败测试 `FeatureMatcherTest.h`**

  断言（合成图 + 已知单应变换）：
  1. 对同一图做已知单应变换（旋转+平移），`MatchBF` + `RatioTestFilter` 后能匹配（非空）。
  2. `RansacHomography` 估计出的单应矩阵接近真值（把图 1 关键点经 H 投影到图 2，重投影误差 < 阈值）。
  3. RANSAC 内点数量 ≥ 4 且占比合理（内点/匹配 > 某阈值）。
  4. 空描述子输入安全返回空。
  5. 内点 < 4 时 `RansacHomography` 返回空 Mat（不崩）。

- [ ] **Step 2: 运行确认失败 → 实现 `FeatureMatcher.h` → 运行确认通过**

  `MatchBF` 用 `cv::BFMatcher(normType)`，`MatchFLANN` 用 `cv::FlannBasedMatcher`，
  `RansacHomography` 用 `cv::findHomography(..., cv::RANSAC, ...)`，内点 < 4 返回空 Mat。

- [ ] **Step 3: 写 demo + 笔记，注册命令**

  `FeatureMatcherDemo()`：两张图（真实或合成变换图），绘制匹配线 + RANSAC 内点，按键调 ratio/阈值。
  `FeatureMatcherNotes.md`：BF/FLANN、ratio test、cross-check、RANSAC/LMEDS 原理与工程用途。

- [ ] **Step 4: 构建 + 单测验证**

- [ ] **Step 5: Commit（逐文件）**

---

### Task 5: 章节 README + 全量验证

**Files:**
- Create: `inc/FeatureMatching/README.md`（第 7 章 API 说明）
- 无其它新增

**Interfaces:**
- Consumes: Task 1~4 全部产物。

- [ ] **Step 1: 写章节 README**

  列三块模块、函数签名、命令名、如何构建运行、素材位置。

- [ ] **Step 2: 全量构建**

  Run: `ninja -C out\build\x64-debug`
  Expected: 无 error。

- [ ] **Step 3: 全量单测**

  Run: `ctest --test-dir out/build/x64-debug -L unit --output-on-failure`
  Expected: 100% 通过（含新增 `keypoint-detect` / `local-feature` / `feature-match`）。

- [ ] **Step 4: 编码校验**

  Run: `powershell -ExecutionPolicy Bypass -File scripts/check-encoding.ps1`
  Expected: 退出码 0。

- [ ] **Step 5: 确认同步状态**

  Run: `git rev-list --left-right --count origin/main...main`
  Expected: `0 0`。

- [ ] **Step 6: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：三块模块 + ImageReg 删除 + 目录骨架，均有对应任务。
- SURF：仅笔记（Task 3 笔记），不写代码——符合「本机无 xfeatures2d」约束。
- 类型一致：SIFT=CV_32F/L2、ORB=CV_8U/Hamming、AKAZE=CV_8U/Hamming，Task 3/4 中 BFMatcher normType 与之匹配。
- Review Focus 5 条：空输入（Task 2/3/4 测试）、关键点数量边界（Task 2/3）、描述子范数（Task 4）、
  RANSAC 内点不足（Task 4 测试）、ratio test 边界（Task 4 测试）。
- 复用 `ImageRegistrator.h` 的经验：它已实现 ORB/SIFT/AKAZE + RANSAC 单应，可作为 Task 3/4 的
  OpenCV 调用参考（但不直接依赖，第 7 章自成模块）。
