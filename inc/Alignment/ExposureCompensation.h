#ifndef INC_ALIGNMENT_EXPOSURECOMPENSATION_H
#define INC_ALIGNMENT_EXPOSURECOMPENSATION_H

#include <vector>

#include "OpenCVCommon.h"

// 曝光补偿模块。
// 定位：读懂原理 + 调用 OpenCV，基于重叠区的增益估计与亮度对齐。
// 增益估计：以第一张图为基准，计算各图均值与其比例（增益），用于拼接前拉平亮度。
namespace Alignment {

// 估计每张图的增益（第一张为基准 gain=1），使各图亮度对齐。
// 用整图全通道均值比：gain[i] = mean(images[0]) / mean(images[i])，全黑图防护返回 1.0。
inline std::vector<double> EstimateGains(const std::vector<cv::Mat>& images)
{
    if (images.empty()) {
        return {};
    }

    std::vector<double> gains;
    gains.reserve(images.size());

    // 全通道均值（灰度=单通道，彩色=BGR 三通道平均）
    auto WholeMean = [](const cv::Mat& img) {
        if (img.empty()) {
            return 0.0;
        }
        const cv::Scalar m = cv::mean(img);
        double sum = 0.0;
        for (int c = 0; c < img.channels(); ++c) {
            sum += m[c];
        }
        return sum / img.channels();
    };

    const double ref = WholeMean(images[0]);

    for (const auto& img : images) {
        if (img.empty()) {
            gains.push_back(1.0);
            continue;
        }
        const double m = WholeMean(img);
        if (m < 1e-6 || ref < 1e-6) {
            gains.push_back(1.0);
            continue;
        }
        gains.push_back(ref / m);
    }
    return gains;
}

// 对图像应用增益（乘性，钳制到 CV_8U 范围）。
inline cv::Mat ApplyGain(const cv::Mat& image, double gain)
{
    if (image.empty()) {
        return {};
    }
    cv::Mat out;
    image.convertTo(out, -1, gain, 0.0);
    return out;
}

} // namespace Alignment

#endif // INC_ALIGNMENT_EXPOSURECOMPENSATION_H
