#ifndef INC_FEATUREMATCHING_FEATUREMATCHER_H
#define INC_FEATUREMATCHING_FEATUREMATCHER_H

#include <vector>

#include "OpenCVCommon.h"

// 匹配 + RANSAC 模块（BF / FLANN / ratio test / cross-check / RANSAC 单应）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
namespace FeatureMatching {

// 暴力匹配：normType 须与描述子类型一致（SIFT=L2，ORB/AKAZE=HAMMING）。
inline std::vector<cv::DMatch> MatchBF(const cv::Mat& desc1, const cv::Mat& desc2, int normType)
{
    if (desc1.empty() || desc2.empty()) {
        return {};
    }
    cv::BFMatcher matcher(normType);
    std::vector<cv::DMatch> matches;
    matcher.match(desc1, desc2, matches);
    return matches;
}

// FLANN 近似最近邻匹配：仅适用于 float 描述子（如 SIFT）。
// 二进制描述子（CV_8U）会抛异常，此处前置校验并安全返回空。
inline std::vector<cv::DMatch> MatchFLANN(const cv::Mat& desc1, const cv::Mat& desc2)
{
    if (desc1.empty() || desc2.empty() || desc1.type() != CV_32F || desc2.type() != CV_32F) {
        return {};
    }
    cv::FlannBasedMatcher matcher;
    std::vector<cv::DMatch> matches;
    matcher.match(desc1, desc2, matches);
    return matches;
}

// Lowe's ratio test：保留最近邻距离远小于次近邻的匹配（ratio 通常 0.7~0.8）。
inline std::vector<cv::DMatch> RatioTestFilter(const std::vector<std::vector<cv::DMatch>>& knnMatches, double ratio)
{
    std::vector<cv::DMatch> good;
    for (const auto& knn : knnMatches) {
        if (knn.size() < 2) {
            continue;
        }
        if (knn[0].distance < ratio * knn[1].distance) {
            good.push_back(knn[0]);
        }
    }
    return good;
}

// 交叉验证：双向匹配都指向对方才保留。
inline std::vector<cv::DMatch> CrossCheckFilter(const std::vector<cv::DMatch>& m1, const std::vector<cv::DMatch>& m2)
{
    std::vector<cv::DMatch> good;
    for (const auto& a : m1) {
        for (const auto& b : m2) {
            if (a.queryIdx == b.trainIdx && a.trainIdx == b.queryIdx) {
                good.push_back(a);
                break;
            }
        }
    }
    return good;
}

// RANSAC 单应估计：内点 < 4 时返回空 Mat（调用方须检查）。
inline cv::Mat RansacHomography(const std::vector<cv::KeyPoint>& kp1, const std::vector<cv::KeyPoint>& kp2,
                                const std::vector<cv::DMatch>& matches, std::vector<cv::DMatch>& inliers)
{
    inliers.clear();
    if (matches.size() < 4) {
        return {};
    }

    std::vector<cv::Point2f> pts1;
    std::vector<cv::Point2f> pts2;
    pts1.reserve(matches.size());
    pts2.reserve(matches.size());
    for (const auto& m : matches) {
        pts1.push_back(kp1[m.queryIdx].pt);
        pts2.push_back(kp2[m.trainIdx].pt);
    }

    std::vector<uchar> mask;
    const cv::Mat h = cv::findHomography(pts1, pts2, cv::RANSAC, 3.0, mask);

    if (h.empty()) {
        return {};
    }

    for (std::size_t i = 0; i < matches.size(); ++i) {
        if (mask[i]) {
            inliers.push_back(matches[i]);
        }
    }
    return h;
}

} // namespace FeatureMatching

#endif // INC_FEATUREMATCHING_FEATUREMATCHER_H
