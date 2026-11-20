#ifndef INC_FEATUREMATCHING_KEYPOINTDETECTOR_H
#define INC_FEATUREMATCHING_KEYPOINTDETECTOR_H

#include <vector>

#include "OpenCVCommon.h"

// 关键点检测模块（Harris / Shi-Tomasi / FAST）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
// 输入 CV_8UC1 灰度图；空输入安全返回空 vector。
namespace FeatureMatching {

// Harris 角点：blockSize 邻域、ksize Sobel 核、k 响应系数、threshold 角点响应阈值。
inline std::vector<cv::KeyPoint> HarrisCorners(const cv::Mat& gray, int blockSize, int ksize, double k,
                                               double threshold)
{
    if (gray.empty()) {
        return {};
    }
    cv::Mat dst;
    cv::cornerHarris(gray, dst, blockSize, ksize, k);

    cv::Mat dstNorm;
    cv::normalize(dst, dstNorm, 0, 255, cv::NORM_MINMAX, CV_8U);

    std::vector<cv::KeyPoint> keypoints;
    for (int y = 0; y < dstNorm.rows; ++y) {
        for (int x = 0; x < dstNorm.cols; ++x) {
            const auto response = dst.at<float>(y, x);
            if (dstNorm.at<std::uint8_t>(y, x) >= threshold) {
                keypoints.emplace_back(cv::Point2f(static_cast<float>(x), static_cast<float>(y)), 3.0f, -1.0f,
                                       response);
            }
        }
    }
    return keypoints;
}

// Shi-Tomasi / GFTT 角点：maxCorners 最大数量、qualityLevel 质量阈值、minDistance 最小间距。
inline std::vector<cv::KeyPoint> ShiTomasiCorners(const cv::Mat& gray, int maxCorners, double qualityLevel,
                                                  double minDistance)
{
    if (gray.empty()) {
        return {};
    }
    std::vector<cv::Point2f> corners;
    cv::goodFeaturesToTrack(gray, corners, maxCorners, qualityLevel, minDistance);

    std::vector<cv::KeyPoint> keypoints;
    keypoints.reserve(corners.size());
    for (const auto& c : corners) {
        keypoints.emplace_back(c, 3.0f);
    }
    return keypoints;
}

// FAST 角点：threshold 强度阈值、nonmaxSuppression 是否做非极大值抑制。
inline std::vector<cv::KeyPoint> FastCorners(const cv::Mat& gray, int threshold, bool nonmaxSuppression)
{
    if (gray.empty()) {
        return {};
    }
    std::vector<cv::KeyPoint> keypoints;
    cv::FAST(gray, keypoints, threshold, nonmaxSuppression);
    return keypoints;
}

} // namespace FeatureMatching

#endif // INC_FEATUREMATCHING_KEYPOINTDETECTOR_H
