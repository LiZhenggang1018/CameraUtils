# 设计文档：按章节学习《计算机视觉：算法与应用》

日期：2026-11-20

## 意图与为什么（intent & why）

用当前这个 C++/OpenCV 工程（CameraUtils）作为练习场，按 Richard Szeliski
《计算机视觉：算法与应用》第 2 版的章节顺序，系统学习手机影像领域的图像算法。

学习目标（用户已明确）：**读懂原理 + 熟练调用 OpenCV**，不要求从零手写核心算法。
定位是工作相关（安卓相机图像算法工程师，拍照/视频算法）。

## 目标、非目标、反目标

### 目标

- 每章按主题分成若干 **self-contained 模块**，每个模块内聚三样东西，围绕同一份代码：
  1. **学习笔记**：算法原理、背景、解决的问题、效果、局限、后续改良（更新算法如何在此
     基础上改进）、实际工程使用情况、对硬件的额外诉求。
  2. **调参 demo**：能看到算法作用在图像上的效果，关键参数可调、调参结果可见。关键效果
     参数通过 `configs/` 下的 json 配置序列化/反序列化（沿用工程现有
     `NLOHMANN_DEFINE_TYPE_INTRUSIVE` 模式，参考 `InterpolationConfig.h` / `HdrConfig.h`）。
  3. **带断言的脚本测试**：用 `CU_CHECK` / `CU_CHECK_EQ` 验证工程最终运行情况。
- 算法封装成**可复用库代码**：接口放 `inc/`、实现放 `src/`，demo 与测试都去调用它
  （参考现有 `OpticalFlow/` 范式）。
- **模块粒度**：不是每个具体算法一个目录，而是每章按主题分块、每块一个模块。一个模块
  内可含多个相关算法（如「线性滤波」块含高斯/均值/锐化/Sobel 多个函数），它们共享同一
  个目录、同套笔记/测试/demo 骨架。

### 非目标（本次不做）

- 不手写核心算法（除非某算法的最小实现本身有助于"读懂原理"，由 builder 按例判断）。
- 不引入 GPU / 深度学习框架（无 GPU 环境）。
- 不追求覆盖书里每一小节，只聚焦与手机影像相关的章节。

### 反目标（即使技术上能跑也算失败）

- 只产出一堆 `imshow` 可视化 demo、没有任何笔记和断言测试。
- 笔记、demo、测试各写一份、互不围绕同一份代码（违背 self-contained 要求）。
- 目录结构混乱到后续章节无法沿用同一套约定。

## 约束

- Windows 11 + PowerShell 5.1；MSVC + Ninja 构建（遵循 `AGENTS.md` 的两套工具链约定）。
- OpenCV 必须含 contrib（工程既有约束，`inc/OpenCVCommon.h` 依赖 optflow/ximgproc）。
- 编码规范遵循 OpenHarmony C++ 规范 + `.clang-format` + `.clang-tidy`（`AGENTS.md`）。
- 提交策略：多文件逐文件提交 + `git push origin main`（`AGENTS.md`）。
- 本章节学习不改变现有构建/CI 结构（不引入新第三方依赖）。

## 范围（Scope）：章节推进顺序

按书中顺序，从前往后，循序渐进。**不考虑**第 1、2、4、5、6、13、14、15 章。
**要做的章节**及与工作的对应关系：

| 章 | 主题 | 对应工作 | 节点大 demo（串联前序章节） |
|---|---|---|---|
| 3 | Image Processing | 多帧降噪、锐化地基 | — |
| 7 | Feature Detection & Matching | 图像配准、多摄对齐 | — |
| 8 | Alignment & Stitching | 图像配准、多摄对齐 | **图像拼接**（3+7+8） |
| 9 | Motion Estimation | EIS 防抖、光流插帧 | **视频 EIS 防抖**（3+7+8+9） |
| 10 | Computational Photography | 多帧 HDR、融合降噪 | **多帧融合（HDR + 降噪）**（3+7+8+10） |
| 11 | SfM & SLAM | 多摄对齐、标定 | — |
| 12 | Depth Estimation | 深度估计、双摄虚化 | **双摄虚化 / 人像模式**（7+8+10+11+12） |

### 节点大 demo 的规则

当学到某个节点、已有知识足够时，做一个贴近工程应用的完整示例，把前面章节的知识串起来。
四个已确认的大 demo，均对齐手机影像领域的核心功能：

1. **图像拼接**（第 8 章落点）—— 串联 3+7+8，对应全景拍摄。
2. **视频 EIS 防抖**（第 9 章落点）—— 串联 3+7+8+9，对应视频防抖。
3. **多帧融合（HDR + 降噪）**（第 10 章落点）—— 串联 3+7+8+10，对应手持超级夜景
   （HDR 解决动态范围、MFNR 多帧降噪解决噪声，两者同属"多帧对齐 + 融合"）。
4. **双摄虚化 / 人像模式**（第 11~12 章落点）—— 串联 7+8+10+11+12，对应双摄深度估计 +
   人像虚化，作为深度估计章节的收尾产出。

## 第一个里程碑：第 3 章 Image Processing

聚焦三大块（每块一个 self-contained 模块）+ 一块简单展示：

- **线性滤波**（模块）：卷积、模糊（均值/高斯）、锐化、Sobel/导数。
- **非线性滤波**（模块）：中值、双边（保边降噪）。
- **金字塔**（模块）：高斯/拉普拉斯金字塔、多尺度（后续拼接/HDR/光流地基）。
- **傅里叶变换**（仅简单展示，不按完整模块做）：频谱可视化、高低频概念。

**实施策略**：先做一个「模板模块」（线性滤波）完整打通「笔记 + 调参 demo + 断言测试 +
config 序列化 + data 目录」全链路并验证结构，再滚动复制到非线性滤波、金字塔两块。
傅里叶变换作为第 3 章的收尾「简单展示」，不套用完整模块骨架。

## 做法（Approaches）

已与用户确认采用 **做法 B**：按「章节」重排目录，学习笔记独立成文件。

### 目录组织约定（做法 B 的具体形态）

- 代码目录名对齐章节：
  - 第 3 章 → `inc/ImageProcessing/` + `src/ImageProcessing/`
  - 第 7 章 → `inc/FeatureMatching/` + `src/FeatureMatching/`（`inc/ImageReg/` 整体删除）
  - 第 8 章 → `inc/Alignment/`
  - 第 9 章 → `inc/OpticalFlow/`（沿用现有）
  - 第 10 章 → 命名对齐为计算摄影（`HdrMerge` 目录重命名为计算摄影语义的目录名）
  - 第 11 章 → `inc/SfM/`
  - 第 12 章 → `inc/Depth/`
- 每个模块（每章按主题分块）的目录形如：
  ```
  inc/<Chapter>/
    <Module>Notes.md      # 学习笔记（独立文件，非 README）
    <Module>.h            # 可复用接口（header-only 或接口声明）
  src/<Chapter>/
    <Module>.cpp          # 实现（若有状态/较重逻辑）
  tests/
    <Module>Test.h        # 断言测试
  configs/
    <Module>Config.json   # 调参 demo 的关键参数配置（仅参数有意义的模块）
    <Module>Config_README.md
  data/<Chapter>/<Module>/   # 输入/输出图像视频 + 中间结果（本地，不入库）
  ```
- `README.md`（API 说明）与 `<Module>Notes.md`（学习笔记）**分开**：README 讲"怎么用"，
  Notes 讲"原理/背景/局限/改良/硬件诉求"。每章一个 `README.md`，每个模块一个 `<Module>Notes.md`。

### 输入/输出数据目录约定

很多算法需要输入输出图像/视频文件。在 `data/` 下按算法模块新建文件夹存放输入、输出及中间
生成结果：

```
data/<Chapter>/<Algo>/
```

`data/` 已在 `.gitignore` 中忽略（不入库），是纯本地行为，与现有 `data/HdrMerge/`、
`data/OpticalFlow/` 一致。demo 与测试读取 `data/` 下素材时，沿用现有 `EnvRootPath()` +
相对路径的约定（参考 `OpticalFlowDataPathImpl()`）。

### 调参 demo 的呈现方式（用户已拍板）

- 调参 demo 用**交互式 `imshow` GUI 窗口**呈现，用户手动调参、实时看画面变化
  （参考现有 `harris` / `optical-flow` 命令）。
- demo 在 `tests/cli/CommandTable.h` 中标注为 `Label::GUI`（不进 ctest/CI）。
- demo 代码放 `tests/`（沿用 `*Test.h` 惯例），**严格与 `inc/`+`src/` 的可复用 API 分离**，
  不在 `inc/` 里塞 GUI demo。

### json 配置粒度（用户已拍板）

- **只有参数有意义的算法**才配 `NLOHMANN_DEFINE_TYPE_INTRUSIVE` struct + JSON + README
  （参考 `InterpolationConfig.h` / `HdrConfig.h`）。
- 近无参的算法（如中值滤波≈一个核大小、傅里叶展示）用代码内常量，不单独配 json。

### 一次性重构成本（本次里程碑内完成）

- 新建 `inc/ImageProcessing/` 章节目录，把第 3 章相关算法归入。
- 现有 `SobelDetect.h` 已放错在 `inc/ImageReg/`，随本次一并归入第 3 章目录。
- 更新 `src/CMakeLists.txt` 的 include 目录（新增章节目录），更新 `tests/cli/CommandTable.h`
  的 include 路径。
- **`HarrisDetect.h` 的迁移推迟到第 7 章开工时再做**（本里程碑不碰它，避免出现只装一个
  GUI demo 头文件的空章节目录）。

### 第 10 章目录命名

第 10 章（Computational Photography）目录命名对齐为计算摄影语义，现有 `inc/HdrMerge/`
  与 `src/HdrMerge/` 相关代码在推进到第 10 章时重命名归入新目录（具体英文名 builder 定，
  与 `OpticalFlow`/`ImageProcessing` 命名风格一致）。

## 第二个里程碑：第 7 章 Feature Detection & Matching

拆三块（每块一个 self-contained 模块），对应工作「图像配准、多摄对齐」，是第 8 章「图像拼接」
大 demo 的地基。

| 块 | 内容 |
|---|---|
| 关键点检测 | Harris、Shi-Tomasi(GFTT)、FAST（可选补充 MSER） |
| 局部特征 | SIFT、ORB、AKAZE（可运行）+ SURF（仅笔记讲述，不写代码） |
| 匹配 + RANSAC | BruteForce + FLANN、Lowe's ratio test、cross-check、RANSAC 单应估计（可选 LMEDS） |

**技术选型**：SIFT / ORB / AKAZE 可运行（OpenCV 主库 features2d，工程已配）；SURF 因
本机 OpenCV 无 `xfeatures2d` contrib 模块，**仅在学习笔记里讲原理/背景/局限（与 SIFT 对比），
不写代码**。

**测试与 demo 输入**：
- 测试用「合成图 + 已知几何变换」自动断言（能进 ctest，验证关键点数量/匹配正确率/内点数量）。
- demo 用真实图 + 合成图看效果（`data/FeatureMatching/` 素材，找不到时生成合成图）。

**清理**：删除 `inc/ImageReg/HarrisDetect.h`（旧 GUI 形态，不符合 self-contained 约定），连同
`src/CMakeLists.txt` 里对 `inc/ImageReg` 的 include 引用一并删除。Harris 在「关键点检测」块里
重新做成可复用库 API + 独立 demo/测试。

## 留给 builder 的部分

- 每个算法的具体 OpenCV 调用与参数选择（在"读懂原理 + 调 OpenCV"目标下，由 builder 从书 +
  OpenCV 文档落地）。
- json 配置的字段设计（沿用 `InterpolationConfig.h` 的序列化宏模式）。
- 断言测试的具体真值来源（合成图 + OpenCV 参考实现交叉验证）。
- 笔记文档的具体措辞与结构（遵循用户列的 8 个要点）。
- 各章节目录的最终英文命名（保持与现有 `OpticalFlow`/`HdrMerge` 命名风格一致）。

## 已由用户拍板、不可再改的部分

- 学习目标 = 读懂原理 + 熟练调用 OpenCV（不手写核心）。
- 一个算法 = self-contained 模块（笔记 + 调参 demo + 断言测试 + 可复用代码）。
- 代码定位 = 可复用库代码（`inc/` + `src/`，参考 `OpticalFlow/`）。
- 目录组织 = 做法 B（按章节重排 + 笔记独立成文件）。
- 推进顺序 = 严格按书中章节顺序 3→7→8→9→10→11→12。
- 节点大 demo = 图像拼接（8）、视频 EIS 防抖（9）、多帧融合 HDR+降噪（10）、双摄虚化（11~12）。
- 第 3 章范围 = 线性滤波 + 非线性滤波 + 金字塔（完整），傅里叶（简单展示）。
- 第 7 章范围 = 关键点检测（Harris/GFTT/FAST）+ 局部特征（SIFT/ORB/AKAZE 可运行，
  SURF 仅笔记）+ 匹配+RANSAC（BruteForce/FLANN/ratio test/cross-check/RANSAC 单应）。
- 第 7 章目录 = `FeatureMatching`，删除 `inc/ImageReg/`。
- 调参 demo = 交互式 `imshow` GUI 窗口（`Label::GUI`，不进 CI）。
- json 配置 = 仅参数有意义的算法才配，近无参用代码内常量。
- 数据目录 = `data/<Chapter>/<Algo>/` 放输入输出与中间结果（本地，不入库）。
- 第 10 章目录命名对齐计算摄影语义（`HdrMerge` 届时重命名归入）。
