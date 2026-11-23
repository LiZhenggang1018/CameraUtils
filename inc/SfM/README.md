# inc/SfM —— 第 11 章 SfM & SLAM（运动恢复结构）

## 概述

本目录对应 Szeliski《计算机视觉：算法与应用》第 11 章，聚焦从图像恢复相机位姿与 3D 结构。工程
完全空白，全新。按主题分四个 self-contained 模块 + Bundle Adjustment 仅笔记。

## 模块总览

| 模块 | 头文件 | 学习笔记 | 单元测试命令 |
|---|---|---|---|
| 相机标定 | `Calibration.h` | `CalibrationNotes.md` | `calibration` |
| 对极几何 | `Epipolar.h` | `EpipolarNotes.md` | `epipolar` |
| 三角化 | `Triangulation.h` | `TriangulationNotes.md` | `triangulation` |
| PnP | `Pnp.h` | `PnpNotes.md` | `pnp` |
| Bundle Adjustment（仅笔记） | — | `BundleAdjustmentNotes.md` | — |

## 函数接口

所有函数位于 `namespace SfM`。

### 相机标定（`Calibration.h`）

```cpp
bool CalibrateCamera(const std::vector<std::vector<cv::Point3f>>& objectPoints,
                     const std::vector<std::vector<cv::Point2f>>& imagePoints,
                     cv::Size imageSize, cv::Mat& K, cv::Mat& distCoeffs);
```

### 对极几何（`Epipolar.h`）

```cpp
cv::Mat ComputeFundamental(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2);
cv::Mat ComputeEssential(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2, const cv::Mat& K);
```

### 三角化（`Triangulation.h`）

```cpp
cv::Mat TriangulatePoints(const cv::Mat& P1, const cv::Mat& P2,
                          const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2);
```

### PnP（`Pnp.h`）

```cpp
bool SolvePnp(const std::vector<cv::Point3f>& objectPoints, const std::vector<cv::Point2f>& imagePoints,
              const cv::Mat& K, const cv::Mat& distCoeffs, cv::Mat& R, cv::Mat& t);
```

## 说明

- **测试用合成数据 + 已知真值精确断言**（能进 ctest）：标定恢复内参、对极约束误差、三角化重建误差、
  PnP 重投影误差，均有数值真值。
- **demo 以数值输出为主**：本目录以精确断言为主，无 imshow demo（几何结果以矩阵/数值呈现）。
- **Bundle Adjustment**：仅笔记讲原理（需 g2o/Ceres 等图优化库，超出 OpenCV 范围）。
- 依赖 `calib3d` 模块（`<opencv2/opencv.hpp>` 已聚合）。

## 学习定位

本目录是「读懂原理 + 熟练调用 OpenCV」定位：标定/对极/三角化/PnP 是几何视觉的基石，
为第 12 章 Depth Estimation（双摄虚化）做铺垫。
