#ifndef INC_IMAGEPROCESSING_SOBELDETECT_H
#define INC_IMAGEPROCESSING_SOBELDETECT_H

#include <algorithm>
#include <cstdint>

#include "OpenCVCommon.h"

// Sobel 边缘检测算子（学习 demo）。
//
// 数学定义：一阶离散微分，用两个 3x3 卷积核分别近似 x/y 方向梯度：
//   Gx = [[-1, 0, 1],
//         [-2, 0, 2],
//         [-1, 0, 1]]
//   Gy = [[-1,-2,-1],
//         [ 0, 0, 0],
//         [ 1, 2, 1]]
//
// 语义约定（与 cv::Sobel 的 CV_8U -> CV_16S 默认路径一致）：
//   - 输入 CV_8UC1 单通道灰度图
//   - 输出 CV_16S，值域可正可负（不取绝对值、不归一化）
//   - 边界处理：BORDER_REFLECT_101（边缘像素镜像复制）
namespace Sobel {

namespace Detail {

// 以 BORDER_REFLECT_101 方式取像素值，坐标越界时镜像回折。
inline int ReflectIndex(int idx, int size)
{
    if (idx < 0) {
        return -idx;
    }
    if (idx >= size) {
        return 2 * size - idx - 2;
    }
    return idx;
}

// 用给定 3x3 核做一次卷积（stride=1，REFLECT_101 边界），输出 CV_16S。
inline cv::Mat Convolve3x3(const cv::Mat& src, const int kernel[3][3])
{
    CV_Assert(src.type() == CV_8UC1);

    cv::Mat dst(src.size(), CV_16S);

    for (int y = 0; y < src.rows; ++y) {
        for (int x = 0; x < src.cols; ++x) {
            int acc = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sy = ReflectIndex(y + ky, src.rows);
                    const int sx = ReflectIndex(x + kx, src.cols);
                    acc += static_cast<int>(src.at<std::uint8_t>(sy, sx)) * kernel[ky + 1][kx + 1];
                }
            }
            dst.at<short>(y, x) = static_cast<short>(acc);
        }
    }
    return dst;
}

} // namespace Detail

inline cv::Mat GradientX(const cv::Mat& src)
{
    static const int kGx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1},
    };
    return Detail::Convolve3x3(src, kGx);
}

inline cv::Mat GradientY(const cv::Mat& src)
{
    static const int kGy[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1},
    };
    return Detail::Convolve3x3(src, kGy);
}

} // namespace Sobel

#endif // INC_IMAGEPROCESSING_SOBELDETECT_H
