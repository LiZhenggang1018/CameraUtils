#ifndef INC_COMPUTATIONALPHOTOGRAPHY_BEAUTYFILTER_H
#define INC_COMPUTATIONALPHOTOGRAPHY_BEAUTYFILTER_H

#include "OpenCVCommon.h"

#include "ImageProcessing/GuidedFilter.h"

// 人像磨皮/美颜核心（高低频分离法）。
// 定位：读懂原理 + 调用 OpenCV。用保边滤波分离基础层与细节层，弱化细节层实现磨皮。
namespace ComputationalPhotography {

// 磨皮：高低频分离法。
//   基础层 = 保边滤波（导向滤波，平滑皮肤但保留边缘）
//   细节层 = 原图 - 基础层（高频瑕疵：毛孔/细纹/斑点）
//   结果   = 基础层 + 细节层 × detailGain（detailGain < 1 弱化瑕疵 = 磨皮）
// 输入 CV_8UC3 彩色图；空输入返回空。
// radius：导向滤波窗口半径；eps：导向滤波正则项；detailGain：细节层增益（0~1，越小磨皮越强）。
inline cv::Mat BeautySkin(const cv::Mat& src, int radius, double eps, double detailGain)
{
    if (src.empty()) {
        return {};
    }
    if (radius < 1) {
        radius = 1;
    }
    if (eps < 0) {
        eps = 0.0;
    }
    if (detailGain < 0.0) {
        detailGain = 0.0;
    }
    if (detailGain > 1.0) {
        detailGain = 1.0;
    }

    // 对每个通道做导向滤波（保边基础层）
    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    std::vector<cv::Mat> baseChannels;
    for (const auto& ch : channels) {
        baseChannels.push_back(ImageProcessing::GuidedFilter(ch, radius, eps));
    }

    cv::Mat base;
    cv::merge(baseChannels, base);

    // 细节层 = 原图 - 基础层
    cv::Mat srcF;
    cv::Mat baseF;
    src.convertTo(srcF, CV_32F);
    base.convertTo(baseF, CV_32F);
    cv::Mat detail = srcF - baseF;

    // 结果 = 基础层 + 细节层 × detailGain
    cv::Mat resultF = baseF + detail * detailGain;

    cv::Mat result;
    resultF.convertTo(result, src.type());
    return result;
}

} // namespace ComputationalPhotography

#endif // INC_COMPUTATIONALPHOTOGRAPHY_BEAUTYFILTER_H
