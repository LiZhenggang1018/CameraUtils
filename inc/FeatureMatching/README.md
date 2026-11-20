# inc/FeatureMatching —— 第 7 章 Feature Detection & Matching（特征检测与匹配）

## 概述

本目录对应 Szeliski《计算机视觉：算法与应用》第 7 章，聚焦特征检测、描述与匹配。按主题分三个
self-contained 模块，每个模块内聚「学习笔记 + 调参 demo + 断言测试 + 可复用代码」。

## 模块总览

| 模块 | 头文件 | 学习笔记 | 单元测试命令 | GUI demo 命令 |
|---|---|---|---|---|
| 关键点检测 | `KeypointDetector.h` | `KeypointDetectorNotes.md` | `keypoint-detect` | `keypoint-detect-demo` |
| 局部特征 | `LocalFeature.h` | `LocalFeatureNotes.md` | `local-feature` | `local-feature-demo` |
| 匹配 + RANSAC | `FeatureMatcher.h` | `FeatureMatcherNotes.md` | `feature-match` | `feature-match-demo` |

## 函数接口

所有函数位于 `namespace FeatureMatching`。

### 关键点检测（`KeypointDetector.h`）

```cpp
std::vector<cv::KeyPoint> HarrisCorners(const cv::Mat& gray, int blockSize, int ksize, double k, double threshold);
std::vector<cv::KeyPoint> ShiTomasiCorners(const cv::Mat& gray, int maxCorners, double qualityLevel, double minDistance);
std::vector<cv::KeyPoint> FastCorners(const cv::Mat& gray, int threshold, bool nonmaxSuppression);
```

### 局部特征（`LocalFeature.h`）

```cpp
cv::Ptr<cv::SIFT> CreateSift(int nfeatures);
cv::Ptr<cv::ORB> CreateOrb(int nfeatures);
cv::Ptr<cv::AKAZE> CreateAkaze();
void DetectAndCompute(const cv::Ptr<cv::Feature2D>& detector, const cv::Mat& image,
                      std::vector<cv::KeyPoint>& keypoints, cv::Mat& descriptors);
```

### 匹配 + RANSAC（`FeatureMatcher.h`）

```cpp
std::vector<cv::DMatch> MatchBF(const cv::Mat& desc1, const cv::Mat& desc2, int normType);
std::vector<cv::DMatch> MatchFLANN(const cv::Mat& desc1, const cv::Mat& desc2);
std::vector<cv::DMatch> RatioTestFilter(const std::vector<std::vector<cv::DMatch>>& knnMatches, double ratio);
std::vector<cv::DMatch> CrossCheckFilter(const std::vector<cv::DMatch>& m1, const std::vector<cv::DMatch>& m2);
cv::Mat RansacHomography(const std::vector<cv::KeyPoint>& kp1, const std::vector<cv::KeyPoint>& kp2,
                         const std::vector<cv::DMatch>& matches, std::vector<cv::DMatch>& inliers);
```

## 说明

- **描述子范数**：SIFT 是 `CV_32F`（L2 / FLANN），ORB / AKAZE 是 `CV_8U`（Hamming / BFMatcher）。
- **SURF**：因本机 OpenCV 无 `xfeatures2d` contrib 模块，仅在 `LocalFeatureNotes.md` 中讲述原理，不提供代码。
- 测试用「合成图 + 已知几何变换」自动断言（进 ctest/CI）；demo 用真实图 + 合成图（`Label::GUI`）。
- 素材放在 `data/FeatureMatching/<Module>/`（不入库），demo 找不到素材时自动生成合成图。

## 学习定位

本目录是「读懂原理 + 熟练调用 OpenCV」定位：实现内部调用 OpenCV 对应 API，封装成统一语义，
便于被上层 demo / 第 8 章「图像拼接」大 demo 复用。
