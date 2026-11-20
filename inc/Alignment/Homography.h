#ifndef INC_ALIGNMENT_HOMOGRAPHY_H
#define INC_ALIGNMENT_HOMOGRAPHY_H

#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/FeatureMatcher.h"
#include "FeatureMatching/LocalFeature.h"

// 单应与 warping 模块。
// 定位：读懂原理 + 调用 OpenCV，复用第 7 章 FeatureMatching 组装配准流程。
namespace Alignment {

// 计算两图间的单应矩阵（复用第 7 章 ORB 检测 + BFMatch + ratio test + RANSAC）。
// 方向：img1 → img2（points2 ≈ H * points1）。内点通过 inliers 返回。
inline cv::Mat ComputeHomography(const cv::Mat& img1, const cv::Mat& img2, std::vector<cv::DMatch>& inliers)
{
    inliers.clear();
    if (img1.empty() || img2.empty()) {
        return {};
    }

    std::vector<cv::KeyPoint> kp1;
    std::vector<cv::KeyPoint> kp2;
    cv::Mat desc1;
    cv::Mat desc2;
    FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(1000), img1, kp1, desc1);
    FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(1000), img2, kp2, desc2);
    if (kp1.empty() || kp2.empty()) {
        return {};
    }

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(desc1, desc2, knn, 2);
    const auto good = FeatureMatching::RatioTestFilter(knn, 0.75);
    if (good.empty()) {
        return {};
    }

    return FeatureMatching::RansacHomography(kp1, kp2, good, inliers);
}

// 将 src 经单应 h 变换到指定画布尺寸（warpPerspective）。
inline cv::Mat WarpImage(const cv::Mat& src, const cv::Mat& h, cv::Size canvasSize)
{
    if (src.empty() || h.empty()) {
        return {};
    }
    cv::Mat dst;
    cv::warpPerspective(src, dst, h, canvasSize);
    return dst;
}

} // namespace Alignment

#endif // INC_ALIGNMENT_HOMOGRAPHY_H
