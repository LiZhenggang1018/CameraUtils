#ifndef INC_IMAGEPROCESSING_LINEARFILTER_H
#define INC_IMAGEPROCESSING_LINEARFILTER_H

#include "OpenCVCommon.h"

#include <algorithm>

// 线性滤波模块（高斯/均值/锐化/Sobel 导数）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
// 输入 CV_8UC1 或 CV_8UC3；空输入安全返回空 Mat；核尺寸取正奇数（偶数时向上取奇）。
namespace ImageProcessing {

namespace Detail {

// 将核尺寸规整为正奇数（OpenCV 要求奇数核，偶数无定义）。
inline int OddKernel(int kernelSize)
{
    if (kernelSize < 1) {
        return 1;
    }
    return (kernelSize % 2 == 0) ? kernelSize + 1 : kernelSize;
}

} // namespace Detail

// 高斯模糊：核尺寸 kernelSize，x 方向标准差 sigmaX（<=0 时由 OpenCV 自动推导）。
inline cv::Mat GaussianBlur(const cv::Mat& src, int kernelSize, double sigmaX)
{
    if (src.empty()) {
        return {};
    }
    const int k = Detail::OddKernel(kernelSize);
    cv::Mat dst;
    cv::GaussianBlur(src, dst, cv::Size(k, k), sigmaX);
    return dst;
}

// 均值模糊：核尺寸 kernelSize。
inline cv::Mat MeanBlur(const cv::Mat& src, int kernelSize)
{
    if (src.empty()) {
        return {};
    }
    const int k = Detail::OddKernel(kernelSize);
    cv::Mat dst;
    cv::blur(src, dst, cv::Size(k, k));
    return dst;
}

// 锐化：unsharp 掩膜，amount 控制锐化强度。
// amount 被钳制到 [0, +inf)；负值会退化成模糊，予以拒绝。
// 注意：在 CV_8U 上运行，过冲会在边缘处饱和到 0/255（8-bit 固有行为）。
inline cv::Mat Sharpen(const cv::Mat& src, double amount)
{
    if (src.empty()) {
        return {};
    }
    const double a = std::max(0.0, amount);
    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(0, 0), 3.0);
    cv::Mat dst;
    cv::addWeighted(src, 1.0 + a, blurred, -a, 0.0, dst);
    return dst;
}

// Sobel x 方向梯度：CV_8U 输入 -> CV_16S 输出（与 cv::Sobel 默认语义一致）。
inline cv::Mat SobelX(const cv::Mat& src)
{
    if (src.empty()) {
        return {};
    }
    cv::Mat dst;
    cv::Sobel(src, dst, CV_16S, 1, 0, 3);
    return dst;
}

// Sobel y 方向梯度：CV_8U 输入 -> CV_16S 输出。
inline cv::Mat SobelY(const cv::Mat& src)
{
    if (src.empty()) {
        return {};
    }
    cv::Mat dst;
    cv::Sobel(src, dst, CV_16S, 0, 1, 3);
    return dst;
}

} // namespace ImageProcessing

#endif // INC_IMAGEPROCESSING_LINEARFILTER_H
