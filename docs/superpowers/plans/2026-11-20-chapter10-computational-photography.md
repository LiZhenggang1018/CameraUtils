# 第 10 章 Computational Photography 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CameraUtils 工程中按「读懂原理 + 熟练调用 OpenCV」的目标，完成第 10 章 Computational Photography：
把已有 `HdrMerge` 代码学习化（补笔记 + 断言测试）并迁移目录/命名空间到 `ComputationalPhotography`，
新增 MFNR 多帧降噪模块，最终做「多帧融合（HDR + 降噪）」大 demo。

**Architecture:** 章节目录 `inc/ComputationalPhotography/` + `tests/ComputationalPhotography/`（header-only，无 src）。
`namespace HdrMerge` → `namespace ComputationalPhotography`。MFNR 新增 `Mfnr.h`。

**Tech Stack:** C++20, OpenCV 4.10（photo/ximgproc）, Exiv2, nlohmann::json, MSVC 19.52 + Ninja, CTest + CU_CHECK。

**Spec:** `docs/superpowers/specs/2026-11-20-cv-algorithm-learning-design.md`

## Global Constraints

- 学习目标 = 读懂原理 + 熟练调用 OpenCV，不手写核心算法。
- 一个模块 = 笔记 + 调参 demo + 断言测试 + 可复用代码。
- 目录：第 10 章 → `inc/ComputationalPhotography/` + `tests/ComputationalPhotography/`。
- `namespace HdrMerge` → `namespace ComputationalPhotography`。
- `data/HdrMerge/` 路径字符串保持不变（本地素材不入库）。
- 命名：类型/函数大驼峰，变量小驼峰。
- include guard：`INC_COMPUTATIONALPHOTOGRAPHY_<NAME>_H`；测试 guard `TESTS_COMPUTATIONALPHOTOGRAPHY_<NAME>_H`。
- 调参 demo = `Label::GUI`；测试 = `Label::UNIT`。
- 提交：按 AGENTS.md 推送根因规则，逐文件 `add → commit → push origin main`。
- 编码：新文件 UTF-8 无 BOM + LF；提交前跑 `scripts/check-encoding.ps1`。
- 不改 `.git/config`、hooks、构建/CI 结构，不引入新第三方依赖。

## Review Focus

1. **空输入**：空 `cv::Mat` / 空图像向量 → 各函数安全返回，不崩溃。
2. **曝光次数**：HDR 需要 ≥2 张不同曝光；MFNR 需要 ≥2 张同曝光；单张输入需降级。
3. **对齐失败**：配准失败时需降级（用原图），不能崩溃。
4. **色调映射**：`TonemapHdr` 输入 HDR 32F，输出 LDR 8U，类型需正确。
5. **MFNR 对齐**：同曝光多帧需先对齐再平均，否则运动模糊。

---

## 模块结构与接口约定（锁定）

### 迁移与学习化（HdrMerge）

#### Task 1: 目录 + 命名空间迁移

**Files:**
- Move: `inc/HdrMerge/HdrMerge.h` → `inc/ComputationalPhotography/HdrMerge.h`
- Move: `inc/HdrMerge/HdrConfig.h` → `inc/ComputationalPhotography/HdrConfig.h`
- Move: `tests/HdrMerge/HdrMergeTest.h` → `tests/ComputationalPhotography/HdrMergeTest.h`
- Modify: `src/CMakeLists.txt`（include 目录 `inc/HdrMerge` → `inc/ComputationalPhotography`）
- Modify: `tests/cli/CommandTable.h`（include 路径）
- Modify: `inc/ComputationalPhotography/HdrMerge.h`（`namespace HdrMerge` → `namespace ComputationalPhotography`，2 处；内部 `#include "HdrMerge/HdrConfig.h"` → `#include "ComputationalPhotography/HdrConfig.h"`）
- Modify: `tests/ComputationalPhotography/HdrMergeTest.h`（`HdrMerge::` → `ComputationalPhotography::`，4 处；include 路径）

- [ ] **Step 1: git mv 移动 3 个文件**
- [ ] **Step 2: 改 namespace 与 include 路径**（用 read/edit，禁 PowerShell 文本命令）
- [ ] **Step 3: 更新 src/CMakeLists.txt + CommandTable.h**
- [ ] **Step 4: 构建验证**
- [ ] **Step 5: Commit（逐文件）**

#### Task 2: HdrMerge 学习化（笔记 + 断言测试）

- [ ] **Step 1: 写失败测试 `tests/ComputationalPhotography/HdrMergeTest.h` 的补充断言**

  新增纯内存可测的断言（不依赖 data/ 素材）：
  1. `MergeMertens` 对多张不同亮度合成图返回非空 LDR 结果。
  2. `TonemapHdr` 对合成 HDR 32F 返回 LDR 8U。
  3. `UnsharpMask` 锐化不改变平坦区。
  4. 空输入安全。

- [ ] **Step 2: RED → GREEN**（HdrMerge 已存在，测试通过即 GREEN）

- [ ] **Step 3: 写 `inc/ComputationalPhotography/HdrMergeNotes.md`**

- [ ] **Step 4: 注册命令 + 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

#### Task 3: MFNR 多帧降噪模块

**Files:**
- Create: `inc/ComputationalPhotography/Mfnr.h`
- Create: `inc/ComputationalPhotography/MfnrNotes.md`
- Create: `tests/ComputationalPhotography/MfnrTest.h`
- Modify: `tests/cli/CommandTable.h`（`mfnr` UNIT）
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
```cpp
namespace ComputationalPhotography {
cv::Mat MfnrDenoise(const std::vector<cv::Mat>& frames, int strength);  // 对齐 + 时域平均 + 可选保边
}
```

- [ ] **Step 1: 写失败测试 `MfnrTest.h`**

  断言（合成图 + 高斯噪声）：
  1. 多张同图加独立噪声，`MfnrDenoise` 输出噪声方差 < 单张（降噪有效）。
  2. 输出尺寸正确。
  3. 单张输入降级（返回原图或空，不崩）。
  4. 空输入安全。

- [ ] **Step 2: RED → GREEN**（实现 MFNR：对齐 + 时域平均，用 `fastNlMeansDenoising` 或简单时域均值）

- [ ] **Step 3: 写 `MfnrNotes.md`**

- [ ] **Step 4: 注册 + 构建 + 单测**

- [ ] **Step 5: Commit（逐文件）**

---

#### Task 4: 多帧融合大 demo + 章节 README

- [ ] **Step 1: 写 `tests/ComputationalPhotography/FusionDemo.h`**（HDR + MFNR 串联演示，GUI）
- [ ] **Step 2: 写 `inc/ComputationalPhotography/README.md`**（章节 README，索引 HDR/MFNR）
- [ ] **Step 3: 注册 demo 命令 + 全量验证（构建 + ctest + 编码校验）**
- [ ] **Step 4: Commit（逐文件）**

---

## Self-Review 备注

- Spec 覆盖：HdrMerge 学习化 + 迁移 + MFNR 新增 + 大 demo，均有对应任务。
- 迁移：目录 + namespace 都改 `ComputationalPhotography`（方案 B），改动点已精确定位（HdrMerge.h 2 处 namespace + 1 处 include，HdrMergeTest.h 4 处调用 + 1 处 include）。
- MFNR：同曝光多帧时域平均降噪，是工程空白的新增量。
- Review Focus 5 条：空输入、曝光/帧数边界（Task 2/3）、对齐失败降级（Task 2）、色调映射类型（Task 2）、MFNR 对齐（Task 3）。
