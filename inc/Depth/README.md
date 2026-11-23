# inc/Depth —— 第 12 章 Depth Estimation（深度估计）

## 概述

本目录对应 Szeliski《计算机视觉：算法与应用》第 12 章，聚焦双目深度估计与双摄虚化。工程完全空白，
全新。按主题分三个 self-contained 模块 + 双摄虚化大 demo。

## 模块总览

| 模块 | 头文件 | 学习笔记 | 单元测试命令 | GUI demo 命令 |
|---|---|---|---|---|
| 极线校正 | `Rectification.h` | `RectificationNotes.md` | `rectification` | — |
| 双目立体匹配 | `StereoMatching.h` | `StereoMatchingNotes.md` | `stereo-matching` | — |
| 深度图 + 虚化 | `Bokeh.h` | `BokehNotes.md` | `bokeh` | `bokeh-demo` |
| 双摄虚化（大 demo） | — | — | — | `portrait-demo` |

## 函数接口

所有函数位于 `namespace Depth`。

### 极线校正（`Rectification.h`）

```cpp
bool RectifyUncalibrated(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2,
                         cv::Size imageSize, cv::Mat& H1, cv::Mat& H2);
```

### 双目立体匹配（`StereoMatching.h`）

```cpp
cv::Mat ComputeDisparity(const cv::Mat& left, const cv::Mat& right, int numDisparities, int blockSize);
cv::Mat DisparityToDepth(const cv::Mat& disparity, double focalLength, double baseline);
```

### 深度图 + 虚化（`Bokeh.h`）

```cpp
cv::Mat ApplyBokeh(const cv::Mat& image, const cv::Mat& depth, double focusDepth, double blurStrength);
```

## 说明

- **复用**：极线校正复用第 11 章 `SfM::ComputeFundamental`（未标定校正，无需内参）。
- **测试用合成图 + 已知深度真值精确断言**（能进 ctest）：校正后对极线水平、视差大小关系、
  深度反比、虚化前后景差异。
- **双摄虚化大 demo**（`portrait-demo`）：真实手机左右照片 → 特征匹配 → 未标定校正 → SGBM 视差 →
  深度 → 虚化，串联 7+8+10+11+12。
- 素材：`data/Depth/Portrait/left.jpg` + `right.jpg`（本地，不入库），缺素材时自动生成合成左右图。

## 学习定位

本目录是「读懂原理 + 熟练调用 OpenCV」定位：极线校正、双目匹配、深度虚化是双摄/人像模式的核心，
是《计算机视觉》全书学习的收官章节。
