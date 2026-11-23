#ifndef INC_DEPTH_STEREOMATCHING_H
#define INC_DEPTH_STEREOMATCHING_H

#include <algorithm>
#include <cmath>

#include "OpenCVCommon.h"

// 双目立体匹配模块。
// 定位：读懂原理 + 调用 OpenCV。视差图（StereoSGBM）+ 视差转深度。
namespace Depth {

// 用 StereoSGBM 计算视差图。输入 CV_8UC1 左右灰度图，输出 CV_16S 视差（值 = 实际视差 × 16）。
// numDisparities 会规整为 16 的倍数（且 ≥16）。空输入返回空。
inline cv::Mat ComputeDisparity(const cv::Mat& left, const cv::Mat& right, int numDisparities, int blockSize)
{
    if (left.empty() || right.empty() || left.size() != right.size()) {
        return {};
    }

    // numDisparities 须为 16 的倍数且 ≥16
    int ndisp = std::max(16, numDisparities);
    ndisp = ((ndisp + 15) / 16) * 16;
    if (blockSize < 1) {
        blockSize = 5;
    }
    if (blockSize % 2 == 0) {
        ++blockSize;
    }

    cv::Ptr<cv::StereoSGBM> sgbm = cv::StereoSGBM::create(0, ndisp, blockSize);
    sgbm->setP1(8 * blockSize * blockSize);
    sgbm->setP2(32 * blockSize * blockSize);
    sgbm->setUniquenessRatio(10);
    sgbm->setDisp12MaxDiff(1);

    cv::Mat disp;
    try {
        sgbm->compute(left, right, disp);
    } catch (const cv::Exception&) {
        return {};
    }
    return disp;
}

// 视差 → 深度：depth = focalLength * baseline / disparity。
// 视差 ≤ 0 处深度设为 0（无效），避免除零。空输入返回空。
inline cv::Mat DisparityToDepth(const cv::Mat& disparity, double focalLength, double baseline)
{
    if (disparity.empty() || focalLength <= 0 || baseline <= 0) {
        return {};
    }

    cv::Mat depth(disparity.size(), CV_64F);
    for (int y = 0; y < disparity.rows; ++y) {
        for (int x = 0; x < disparity.cols; ++x) {
            const double disp = static_cast<double>(disparity.at<short>(y, x)) / 16.0;
            if (disp <= 0) {
                depth.at<double>(y, x) = 0.0;
            } else {
                depth.at<double>(y, x) = focalLength * baseline / disp;
            }
        }
    }
    return depth;
}

} // namespace Depth

#endif // INC_DEPTH_STEREOMATCHING_H
