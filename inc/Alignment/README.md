# inc/Alignment —— 第 8 章 Alignment & Stitching（对齐与拼接）

## 概述

本目录对应 Szeliski《计算机视觉：算法与应用》第 8 章，聚焦图像对齐与拼接。按主题分四个
self-contained 模块 + 全景拼接核心，每个模块内聚「学习笔记 + 调参 demo + 断言测试 + 可复用代码」。

## 模块总览

| 模块 | 头文件 | 学习笔记 | 单元测试命令 | GUI demo 命令 |
|---|---|---|---|---|
| 单应与 warping | `Homography.h` | `HomographyNotes.md` | `homography` | `homography-demo` |
| 图像融合 | `Blending.h` | `BlendingNotes.md` | `blending` | `blending-demo` |
| 曝光补偿 | `ExposureCompensation.h` | `ExposureCompensationNotes.md` | `exposure-comp` | `exposure-comp-demo` |
| 渐晕校正 | `Vignetting.h` | `VignettingNotes.md` | `vignetting` | `vignetting-demo` |
| 全景拼接 | `Panorama.h` | — | `panorama` | `panorama-demo` |

## 函数接口

所有函数位于 `namespace Alignment`。

### 单应与 warping（`Homography.h`）

```cpp
cv::Mat ComputeHomography(const cv::Mat& img1, const cv::Mat& img2, std::vector<cv::DMatch>& inliers);
cv::Mat WarpImage(const cv::Mat& src, const cv::Mat& h, cv::Size canvasSize);
```

### 图像融合（`Blending.h`）

```cpp
cv::Mat MultiBandBlend(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask1,
                       const cv::Mat& mask2, int levels);
```

### 曝光补偿（`ExposureCompensation.h`）

```cpp
std::vector<double> EstimateGains(const std::vector<cv::Mat>& images);
cv::Mat ApplyGain(const cv::Mat& image, double gain);
```

### 渐晕校正（`Vignetting.h`）

```cpp
cv::Mat CorrectVignetting(const cv::Mat& image, double strength);
```

### 全景拼接（`Panorama.h`）

```cpp
cv::Mat StitchPanorama(const std::vector<cv::Mat>& images);
```

## 说明

- **复用**：配准复用第 7 章 `FeatureMatching`（`ComputeHomography` 内部用 ORB + BFMatch + ratio + RANSAC），
  融合复用第 3 章 `ImageProcessing::BuildLaplacianPyramid`。
- **对照参考**：`ImageRegistrator`（`inc/LivePhotoSmoothPlay/`）实现了等价配准流程，可作对照。
- **单应方向**：`ComputeHomography` 返回 img1→img2 方向（`points2 ≈ H · points1`），warp 时注意方向。
- 测试用「合成图 + 已知变换」自动断言（进 ctest/CI）；demo 用真实图 + 合成图（`Label::GUI`）。
- 素材放在 `data/Alignment/<Module>/`（不入库），demo 找不到素材时自动生成合成图。

## 学习定位

本目录是「读懂原理 + 熟练调用 OpenCV」定位：复用第 3/7 章模块，组合成完整的图像拼接流程，
落点是「多图全景拼接」大 demo（串联 3+7+8）。
