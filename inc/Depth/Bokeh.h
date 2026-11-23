#ifndef INC_DEPTH_BOKEH_H
#define INC_DEPTH_BOKEH_H

#include <algorithm>
#include <cmath>

#include "OpenCVCommon.h"

// 深度图 + 虚化模块。
// 定位：读懂原理 + 调用 OpenCV。按深度分层模拟大光圈虚化（人像模式）。
// 实现：聚焦深度附近清晰，远离聚焦深度的区域按深度差异做高斯模糊（差异越大越模糊）。
namespace Depth {

inline cv::Mat ApplyBokeh(const cv::Mat& image, const cv::Mat& depth, double focusDepth, double blurStrength)
{
    if (image.empty() || depth.empty() || image.size() != depth.size()) {
        return {};
    }
    if (image.channels() != 3 || depth.type() != CV_64F) {
        return {};
    }

    // 深度差异 → 模糊权重 w ∈ [0,1]：差异 0 → w=0（清晰），差异大 → w→1（全模糊）
    // 用指数衰减：w = 1 - exp(-|depth - focus| / blurStrength)
    cv::Mat weight(depth.size(), CV_32F);
    for (int y = 0; y < depth.rows; ++y) {
        for (int x = 0; x < depth.cols; ++x) {
            const double d = depth.at<double>(y, x);
            const double diff = std::abs(d - focusDepth);
            double w;
            if (blurStrength <= 0) {
                w = 0.0;
            } else {
                w = 1.0 - std::exp(-diff / blurStrength);
            }
            weight.at<float>(y, x) = static_cast<float>(std::min(1.0, std::max(0.0, w)));
        }
    }

    // 高斯模糊的图（模糊程度固定，这里用一个较大核）
    cv::Mat blurred;
    cv::GaussianBlur(image, blurred, cv::Size(21, 21), 8.0);

    // 按权重混合：result = (1-w)*image + w*blurred
    cv::Mat imgF;
    cv::Mat blurF;
    image.convertTo(imgF, CV_32FC3);
    blurred.convertTo(blurF, CV_32FC3);

    cv::Mat resultF(depth.size(), CV_32FC3);
    for (int y = 0; y < depth.rows; ++y) {
        for (int x = 0; x < depth.cols; ++x) {
            const float w = weight.at<float>(y, x);
            resultF.at<cv::Vec3f>(y, x) = (1.0f - w) * imgF.at<cv::Vec3f>(y, x) + w * blurF.at<cv::Vec3f>(y, x);
        }
    }

    cv::Mat result;
    resultF.convertTo(result, image.type());
    return result;
}

} // namespace Depth

#endif // INC_DEPTH_BOKEH_H
