#ifndef INC_IMAGEPROCESSING_NONLINEARFILTER_H
#define INC_IMAGEPROCESSING_NONLINEARFILTER_H

#include "OpenCVCommon.h"

// 非线性滤波模块（中值/双边）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
// 输入 CV_8UC1 或 CV_8UC3；空输入安全返回空 Mat。
namespace ImageProcessing {

namespace Detail {

// 将核尺寸规整为正奇数（OpenCV 要求奇数核，偶数无定义）。
inline int OddKernelNonLinear(int kernelSize)
{
    if (kernelSize < 1) {
        return 1;
    }
    return (kernelSize % 2 == 0) ? kernelSize + 1 : kernelSize;
}

} // namespace Detail

// 中值滤波：核尺寸 kernelSize，输出为邻域像素的中值（有效去除椒盐噪声）。
inline cv::Mat MedianBlur(const cv::Mat& src, int kernelSize)
{
    if (src.empty()) {
        return {};
    }
    const int k = Detail::OddKernelNonLinear(kernelSize);
    cv::Mat dst;
    cv::medianBlur(src, dst, k);
    return dst;
}

// 双边滤波：空间核直径 diameter + 灰度相似度 sigmaColor + 空间 sigmaSpace（保边降噪）。
// diameter 规整为正奇数（OpenCV 要求奇数，偶数无定义）。
inline cv::Mat BilateralFilter(const cv::Mat& src, int diameter, double sigmaColor, double sigmaSpace)
{
    if (src.empty()) {
        return {};
    }
    const int d = Detail::OddKernelNonLinear(diameter);
    cv::Mat dst;
    cv::bilateralFilter(src, dst, d, sigmaColor, sigmaSpace);
    return dst;
}

} // namespace ImageProcessing

#endif // INC_IMAGEPROCESSING_NONLINEARFILTER_H
