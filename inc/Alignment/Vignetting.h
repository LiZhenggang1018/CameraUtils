#ifndef INC_ALIGNMENT_VIGNETTING_H
#define INC_ALIGNMENT_VIGNETTING_H

#include "OpenCVCommon.h"

// 渐晕校正模块。
// 定位：读懂原理 + 调用 OpenCV，基于径向衰减模型校正镜头暗角。
// 模型：I' = I / (1 - strength·r²)，r 为归一化半径（0 中心 ~ 1 角落）。
// strength 控制衰减强度（0 = 无衰减，越大约暗角越重）。
namespace Alignment {

inline cv::Mat CorrectVignetting(const cv::Mat& image, double strength)
{
    if (image.empty()) {
        return {};
    }
    if (strength <= 0.0) {
        return image.clone();
    }

    const int rows = image.rows;
    const int cols = image.cols;
    const double cx = (cols - 1) / 2.0;
    const double cy = (rows - 1) / 2.0;
    const double maxR2 = cx * cx + cy * cy;
    if (maxR2 <= 0.0) {
        return image.clone(); // 退化 1×1 图，无暗角可校
    }

    // 预计算校正系数图（乘性增益）
    cv::Mat gainMap(rows, cols, CV_64F);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            const double r2 = (dx * dx + dy * dy) / maxR2;
            gainMap.at<double>(y, x) = 1.0 / std::max(0.1, 1.0 - strength * r2);
        }
    }

    cv::Mat imgF;
    image.convertTo(imgF, CV_32F);
    cv::Mat correctedF;
    cv::multiply(imgF, gainMap, correctedF, 1.0, CV_32F);

    cv::Mat out;
    correctedF.convertTo(out, image.type());
    return out;
}

} // namespace Alignment

#endif // INC_ALIGNMENT_VIGNETTING_H
