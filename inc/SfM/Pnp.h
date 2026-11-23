#ifndef INC_SFM_PNP_H
#define INC_SFM_PNP_H

#include <vector>

#include "OpenCVCommon.h"

// PnP 模块。
// 定位：读懂原理 + 调用 OpenCV。3D→2D 位姿估计（solvePnP）。
namespace SfM {

// 用 3D 点（objectPoints）与对应 2D 点（imagePoints）求解相机位姿 R（3×3 旋转矩阵）与 t（3×1 平移）。
// 默认用 SOLVEPNP_ITERATIVE。点不足 4 或失败返回 false。
inline bool SolvePnp(const std::vector<cv::Point3f>& objectPoints, const std::vector<cv::Point2f>& imagePoints,
                     const cv::Mat& K, const cv::Mat& distCoeffs, cv::Mat& R, cv::Mat& t)
{
    if (objectPoints.size() < 4 || objectPoints.size() != imagePoints.size() || K.empty()) {
        return false;
    }
    if (K.rows != 3 || K.cols != 3) {
        return false;
    }

    cv::Mat rvec;
    try {
        const bool ok = cv::solvePnP(objectPoints, imagePoints, K, distCoeffs, rvec, t, false, cv::SOLVEPNP_ITERATIVE);
        if (!ok) {
            return false;
        }
        cv::Rodrigues(rvec, R);
    } catch (const cv::Exception&) {
        return false;
    }

    return !R.empty() && !t.empty();
}

} // namespace SfM

#endif // INC_SFM_PNP_H
