#ifndef INC_IMAGEPROCESSING_GUIDEDFILTER_H
#define INC_IMAGEPROCESSING_GUIDEDFILTER_H

#include "OpenCVCommon.h"

// 导向滤波模块。
// 定位：读懂原理 + 调用 OpenCV。保边平滑（双边滤波的主流替代）。
// 实现：cv::ximgproc::guidedFilter，guide = 自身（自引导），radius 控制保边窗口，eps 控制平滑程度。
namespace ImageProcessing {

// 导向滤波（自引导）：guide = src。空输入安全返回空。
// radius：滤波窗口半径（越大越平滑）；eps：正则项（越大越接近均值滤波，越小越保留细节）。
inline cv::Mat GuidedFilter(const cv::Mat& src, int radius, double eps)
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

    cv::Mat dst;
    try {
        cv::ximgproc::guidedFilter(src, src, dst, radius, eps);
    } catch (const cv::Exception&) {
        return {};
    }
    return dst;
}

} // namespace ImageProcessing

#endif // INC_IMAGEPROCESSING_GUIDEDFILTER_H
