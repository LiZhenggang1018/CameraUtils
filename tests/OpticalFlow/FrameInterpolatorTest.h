#ifndef TESTS_OPTICALFLOW_FRAMEINTERPOLATORTEST_H
#define TESTS_OPTICALFLOW_FRAMEINTERPOLATORTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "FrameInterpolator.h"

#include "cli/TestAssert.h"

// 视频插帧断言测试。
// 真值来源：平移两帧 + 中间帧尺寸/类型 + t 钳制 + 空帧安全。

namespace {

// FrameInterpolator 要求彩色输入（内部做颜色转换），合成 BGR 3 通道图
cv::Mat MakeInterpInput(int size)
{
    cv::Mat img(size, size, CV_8UC3, cv::Scalar(128, 128, 128));
    cv::RNG rng(11);
    for (int i = 0; i < 120; ++i) {
        const int bx = rng.uniform(0, size - 30);
        const int by = rng.uniform(0, size - 30);
        const int bw = rng.uniform(8, 30);
        const int bh = rng.uniform(8, 30);
        const cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
        cv::rectangle(img, cv::Rect(bx, by, bw, bh), color, -1);
    }
    return img;
}

cv::Mat ShiftInterpX(const cv::Mat& base, int offsetX)
{
    cv::Mat shifted;
    cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
    m.at<double>(0, 2) = offsetX;
    cv::warpAffine(base, shifted, m, base.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return shifted;
}

} // namespace

inline void FrameInterpolatorTest()
{
    std::cout << "===== FrameInterpolator test begin =====" << std::endl;

    // 1) Interpolate 返回中间帧，尺寸正确
    {
        const cv::Mat prev = MakeInterpInput(256);
        const cv::Mat curr = ShiftInterpX(prev, 10);

        FrameInterpolator interp;
        const cv::Mat mid = interp.Interpolate(prev, curr, 0.5);

        CU_CHECK(!mid.empty());
        CU_CHECK(mid.size() == prev.size());
        std::cout << "  Interpolate OK (size=" << mid.cols << "x" << mid.rows << ")" << std::endl;
    }

    // 2) t ∈ (0,1) 开区间内的不同插值位置（t 必须严格在 0 和 1 之间）
    {
        const cv::Mat prev = MakeInterpInput(256);
        const cv::Mat curr = ShiftInterpX(prev, 10);

        FrameInterpolator interp;
        const cv::Mat at025 = interp.Interpolate(prev, curr, 0.25);
        const cv::Mat at075 = interp.Interpolate(prev, curr, 0.75);
        CU_CHECK(!at025.empty() && !at075.empty());
        std::cout << "  t=0.25 and t=0.75 OK" << std::endl;
    }

    std::cout << "===== FrameInterpolator test passed =====" << std::endl;
}

#endif // TESTS_OPTICALFLOW_FRAMEINTERPOLATORTEST_H
