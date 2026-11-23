# 《计算机视觉：算法与应用》学习工程总结

> 用 CameraUtils（C++/OpenCV）作为练习场，按 Richard Szeliski《计算机视觉：算法与应用》第 2 版的章节顺序，
> 系统学习手机影像（安卓相机）领域的图像算法。

## 学习目标

- **读懂原理 + 熟练调用 OpenCV**（不手写核心算法，OpenCV 是手段，理解是目的）。
- 定位是**工作相关**：安卓相机图像算法工程师，覆盖拍照/视频算法（多帧融合降噪、HDR、配准、EIS 防抖、
  光流插帧、深度估计、多摄对齐等）。

## 已完成的章节（7 章，按书中顺序）

| 章 | 目录 | 核心内容 | 单元测试命令 | 大 demo |
|---|---|---|---|---|
| 3 图像处理 | `ImageProcessing` | 线性滤波、非线性滤波、金字塔、傅里叶展示 | `sobel`/`linear-filter`/`nonlinear-filter`/`pyramid` | — |
| 7 特征检测匹配 | `FeatureMatching` | 关键点（Harris/GFTT/FAST）、局部特征（SIFT/ORB/AKAZE）、匹配+RANSAC | `keypoint-detect`/`local-feature`/`feature-match` | — |
| 8 对齐拼接 | `Alignment` | 单应+warping、多波段融合、曝光补偿、渐晕校正 | `homography`/`blending`/`exposure-comp`/`vignetting` | **图像拼接** `panorama` |
| 9 运动估计 | `OpticalFlow` | 稀疏光流（LK）、稠密光流、视频插帧、EIS 防抖 | `lk-tracker`/`dense-flow`/`frame-interp`/`eis` | **EIS 防抖** `eis-demo` |
| 10 计算摄影 | `ComputationalPhotography` | HDR 融合、MFNR 多帧降噪 | `hdr-merge-unit`/`mfnr` | **多帧融合（HDR+降噪）** `fusion-demo` |
| 11 SfM & SLAM | `SfM` | 相机标定、对极几何、三角化、PnP（BA 仅笔记） | `calibration`/`epipolar`/`triangulation`/`pnp` | `sfm-demo` |
| 12 深度估计 | `Depth` | 极线校正、双目立体匹配、深度图+虚化 | `rectification`/`stereo-matching`/`bokeh` | **双摄虚化** `portrait-demo` |

## 四个节点大 demo（串联前序章节）

| 大 demo | 串联章节 | 对应手机功能 |
|---|---|---|
| 图像拼接 | 3+7+8 | 全景拍摄 |
| 视频 EIS 防抖 | 3+7+8+9 | 视频防抖 |
| 多帧融合（HDR+降噪） | 3+7+8+10 | 超级夜景 |
| 双摄虚化 | 7+8+10+11+12 | 人像模式 |

## 统一的模块范式

每个算法都是 **self-contained 模块**，内聚四样东西：

1. **学习笔记**（`<Module>Notes.md`）：原理、背景、解决问题、效果、局限、后续改良、工程使用、硬件诉求。
2. **可复用库代码**：`inc/<章节目录>/`（接口，header-only）+ `src/<章节目录>/`（实现，如有）。
3. **断言测试**（`tests/<章节目录>/<Module>Test.h`）：用 `CU_CHECK` 断言，合成数据 + 已知真值精确验证（能进 ctest）。
4. **调参 demo**（`<Module>Demo.h`）：交互式 `imshow`（GUI）或数值输出（几何类）。

## 测试方法论

- **图像类算法**（滤波/特征/光流/融合/虚化）：合成图 + 已知变换，断言「方向正确、类型正确、边界安全」。
- **几何类算法**（标定/对极/三角化/PnP）：合成数据 + 已知真值，断言「数值精确」（误差达 1e-6 量级）。
- **边界防护**：空输入、点数量不足、除零、退化场景，全部安全降级不崩溃。

## 工程规范沉淀（学习过程中的副产品）

学习过程中建立并固化了大量工程规范（见 `AGENTS.md`）：

- **目录结构**：按章节归档（`inc/<章节目录>/` + `tests/<章节目录>/`），公共工具类归 `CommonUtils/`。
- **编码防乱码**：`.editorconfig`（UTF-8 无 BOM + LF）、`.gitattributes`、`scripts/check-encoding.ps1` 提交前校验。
- **推送规则**：单文件逐次提交（网关 pack 限制的根因），`git push origin main`。
- **编码规范**：OpenHarmony C++ 规范 + `.clang-format` + `.clang-tidy`。

## 测试统计

- 单元测试（`ctest -L unit`）：**44 个全绿**。
- media 测试（`data/` 素材）：`hdr-merge*`、`optical-flow-upsample`、`livephoto*` 等（本地素材存在时运行）。

## 如何运行

```bat
:: 构建（先加载 MSVC 环境）
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
ninja -C out\build\x64-debug

:: 全部单元测试
ctest --test-dir out/build/x64-debug -L unit --output-on-failure

:: 查看所有命令
out\build\x64-debug\tests\camera_utils_tests.exe --list

:: 运行某个 demo（GUI）
out\build\x64-debug\tests\camera_utils_tests.exe <demo-command>
```
