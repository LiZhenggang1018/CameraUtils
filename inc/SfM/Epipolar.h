#ifndef INC_SFM_EPIPOLAR_H
#define INC_SFM_EPIPOLAR_H

#include <vector>

#include "OpenCVCommon.h"

// 对极几何模块。
// 定位：读懂原理 + 调用 OpenCV。本质矩阵 E、基础矩阵 F。
namespace SfM {

// 基础矩阵 F（x'ᵀ F x = 0），用 8 点法。点不足 8 或退化返回空。
inline cv::Mat ComputeFundamental(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2)
{
    if (pts1.size() < 8 || pts1.size() != pts2.size()) {
        return {};
    }
    cv::Mat mask;
    cv::Mat F;
    try {
        F = cv::findFundamentalMat(pts1, pts2, cv::FM_8POINT, 1.0, 0.99, mask);
    } catch (const cv::Exception&) {
        return {};
    }
    if (F.empty() || F.rows != 3 || F.cols != 3) {
        return {};
    }
    return F;
}

// 本质矩阵 E（x'ᵀ E x = 0，x 为归一化坐标）。用 findEssentialMat（RANSAC + 五点法）。
inline cv::Mat ComputeEssential(const std::vector<cv::Point2f>& pts1, const std::vector<cv::Point2f>& pts2,
                                const cv::Mat& K)
{
    if (pts1.size() < 5 || pts1.size() != pts2.size() || K.empty()) {
        return {};
    }
    if (K.rows != 3 || K.cols != 3) {
        return {};
    }
    cv::Mat mask;
    cv::Mat E;
    try {
        E = cv::findEssentialMat(pts1, pts2, K, cv::RANSAC, 0.999, 1.0, mask);
    } catch (const cv::Exception&) {
        return {};
    }
    if (E.empty() || E.rows != 3 || E.cols != 3) {
        return {};
    }
    return E;
}

} // namespace SfM

#endif // INC_SFM_EPIPOLAR_H
