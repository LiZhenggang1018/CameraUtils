#ifndef INC_SFM_CALIBRATION_H
#define INC_SFM_CALIBRATION_H

#include <vector>

#include "OpenCVCommon.h"

// 相机标定模块。
// 定位：读懂原理 + 调用 OpenCV。用多视角棋盘角点标定内参 K 与畸变系数。
namespace SfM {

// 张正友标定：输入多视角的 objectPoints（棋盘 3D 角点）与 imagePoints（对应 2D 角点），
// 输出内参 K（3×3）与畸变系数 distCoeffs。成功返回 true。
inline bool CalibrateCamera(const std::vector<std::vector<cv::Point3f>>& objectPoints,
                            const std::vector<std::vector<cv::Point2f>>& imagePoints, cv::Size imageSize, cv::Mat& K,
                            cv::Mat& distCoeffs)
{
    if (objectPoints.empty() || imagePoints.empty() || objectPoints.size() != imagePoints.size()) {
        return false;
    }
    if (objectPoints.size() < 2 || imageSize.width <= 0 || imageSize.height <= 0) {
        return false;
    }

    std::vector<cv::Mat> rvecs;
    std::vector<cv::Mat> tvecs;
    try {
        const double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize, K, distCoeffs, rvecs, tvecs);
        (void)rms;
    } catch (const cv::Exception&) {
        return false;
    }

    return !K.empty() && K.rows == 3 && K.cols == 3;
}

} // namespace SfM

#endif // INC_SFM_CALIBRATION_H
