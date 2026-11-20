# inc/ImageProcessing —— 第 3 章 Image Processing（图像处理）

## 概述

本目录对应 Szeliski《计算机视觉：算法与应用》第 3 章，聚焦图像处理基础算子。按主题分三个
self-contained 模块 + 一个简单展示，每个模块内聚「学习笔记 + 调参 demo + 断言测试 + 可复用代码」。

## 模块总览

| 模块 | 头文件 | 学习笔记 | 单元测试命令 | GUI demo 命令 |
|---|---|---|---|---|
| 线性滤波 | `LinearFilter.h` | `LinearFilterNotes.md` | `linear-filter` | `linear-filter-demo` |
| 非线性滤波 | `NonLinearFilter.h` | `NonLinearFilterNotes.md` | `nonlinear-filter` | `nonlinear-filter-demo` |
| 金字塔 | `Pyramid.h` | `PyramidNotes.md` | `pyramid` | `pyramid-demo` |
| 傅里叶（简单展示） | — | — | — | `fourier-demo` |
| Sobel（手写实现） | `SobelDetect.h` | — | `sobel` | — |

## 函数接口

所有函数位于 `namespace ImageProcessing`（Sobel 位于 `namespace Sobel`），输入 `CV_8UC1`/`CV_8UC3`，
空输入安全返回空结果。

### 线性滤波（`LinearFilter.h`）

```cpp
cv::Mat GaussianBlur(const cv::Mat& src, int kernelSize, double sigmaX);
cv::Mat MeanBlur(const cv::Mat& src, int kernelSize);
cv::Mat Sharpen(const cv::Mat& src, double amount);      // unsharp 掩膜
cv::Mat SobelX(const cv::Mat& src);                      // CV_16S
cv::Mat SobelY(const cv::Mat& src);                      // CV_16S
```

### 非线性滤波（`NonLinearFilter.h`）

```cpp
cv::Mat MedianBlur(const cv::Mat& src, int kernelSize);
cv::Mat BilateralFilter(const cv::Mat& src, int diameter, double sigmaColor, double sigmaSpace);
```

### 金字塔（`Pyramid.h`）

```cpp
std::vector<cv::Mat> BuildGaussianPyramid(const cv::Mat& src, int levels);      // CV_8U 层
std::vector<cv::Mat> BuildLaplacianPyramid(const cv::Mat& src, int levels);     // CV_32F 层（残差）
cv::Mat ReconstructFromLaplacian(const std::vector<cv::Mat>& pyr);              // CV_8U
```

## 调参 demo

- demo 为交互式 `imshow` 窗口（`Label::GUI`，不进 ctest/CI）。
- 关键参数通过 `configs/*.json` 序列化（`LinearFilterConfig.h` / `NonLinearFilterConfig.h`），
  金字塔与傅里叶无复杂参数，用代码内常量。
- 运行：`camera_utils_tests <demo-command>`（或 `CameraUtils <demo-command>`）。

## 素材

输入/输出图像、中间结果放在 `data/ImageProcessing/<Module>/`（该目录不入库，`.gitignore` 已忽略
`data/`）。demo 找不到素材时会自动生成合成图。

## 学习定位

本目录是「读懂原理 + 熟练调用 OpenCV」定位：实现内部调用 OpenCV 对应 API，封装成统一语义，
便于被上层 demo / 后续章节（拼接、EIS、HDR、深度）复用。
