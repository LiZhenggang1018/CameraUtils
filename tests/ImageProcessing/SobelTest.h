#ifndef TESTS_IMAGEPROCESSING_SOBELTEST_H
#define TESTS_IMAGEPROCESSING_SOBELTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/SobelDetect.h"

#include "cli/TestAssert.h"

// 阶段0（外化理解）——Sobel 算子的数学定义：
//   一阶离散微分，用两个 3x3 核分别近似 x/y 方向梯度：
//     Gx = [[-1, 0, 1], [-2, 0, 2], [-1, 0, 1]]
//     Gy = [[-1,-2,-1], [ 0, 0, 0], [ 1, 2, 1]]
//   对 CV_8U 输入输出 CV_16S，边界用 BORDER_REFLECT_101（与 cv::Sobel 默认一致）。
//
//   手算真值（3x3 垂直阶跃图，中心像素 (1,1) 邻域即整图）：
//     src = [[0,0,255],[0,0,255],[0,0,255]]
//     Gx(1,1) = 1*255 + 2*255 + 1*255 = 1020
//     Gy(1,1) = -1*255 + 1*255 = 0

namespace {
// 构造 3x3 垂直阶跃图（左黑右白），用于手算真值断言
cv::Mat MakeStepImage()
{
    cv::Mat img(3, 3, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < 3; ++y) {
        img.at<std::uint8_t>(y, 2) = 255;
    }
    return img;
}

// 构造一张确定性的小图（非随机），用于与 cv::Sobel 交叉验证
cv::Mat MakePatternImage()
{
    cv::Mat img(9, 13, CV_8UC1);
    for (int y = 0; y < img.rows; ++y) {
        for (int x = 0; x < img.cols; ++x) {
            img.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
        }
    }
    return img;
}
} // namespace

inline void SobelEdgeTest()
{
    std::cout << "===== Sobel test begin =====" << std::endl;

    // 1) 手算真值：垂直阶跃图中心像素的 Gx=1020, Gy=0
    {
        const cv::Mat step = MakeStepImage();
        const cv::Mat gx = Sobel::GradientX(step);
        const cv::Mat gy = Sobel::GradientY(step);

        CU_CHECK(!gx.empty());
        CU_CHECK(!gy.empty());
        CU_CHECK(gx.type() == CV_16S);
        CU_CHECK(gy.type() == CV_16S);

        CU_CHECK_EQ(gx.at<short>(1, 1), 1020);
        CU_CHECK_EQ(gy.at<short>(1, 1), 0);
        std::cout << "  hand-computed truth: Gx(1,1)=1020, Gy(1,1)=0 OK" << std::endl;
    }

    // 2) 交叉验证：手写实现与 cv::Sobel 在整张图上逐像素一致
    {
        const cv::Mat src = MakePatternImage();

        const cv::Mat gx = Sobel::GradientX(src);
        const cv::Mat gy = Sobel::GradientY(src);

        cv::Mat refGx;
        cv::Mat refGy;
        cv::Sobel(src, refGx, CV_16S, 1, 0, 3);
        cv::Sobel(src, refGy, CV_16S, 0, 1, 3);

        CU_CHECK(gx.size() == refGx.size());
        CU_CHECK(gy.size() == refGy.size());

        double maxDiffGx = 0.0;
        double maxDiffGy = 0.0;
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                maxDiffGx = std::max(maxDiffGx, std::abs(static_cast<double>(gx.at<short>(y, x)) -
                                                         static_cast<double>(refGx.at<short>(y, x))));
                maxDiffGy = std::max(maxDiffGy, std::abs(static_cast<double>(gy.at<short>(y, x)) -
                                                         static_cast<double>(refGy.at<short>(y, x))));
            }
        }

        CU_CHECK(maxDiffGx == 0.0);
        CU_CHECK(maxDiffGy == 0.0);
        std::cout << "  cross-validation vs cv::Sobel: maxDiff(Gx)=" << maxDiffGx << ", maxDiff(Gy)=" << maxDiffGy
                  << " OK" << std::endl;
    }

    std::cout << "===== Sobel test passed =====" << std::endl;
}

#endif // TESTS_IMAGEPROCESSING_SOBELTEST_H
