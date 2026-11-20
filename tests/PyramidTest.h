#ifndef TESTS_PYRAMIDTEST_H
#define TESTS_PYRAMIDTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "ImageProcessing/Pyramid.h"

#include "cli/TestAssert.h"

// 金字塔模块断言测试。
// 真值来源：层数/尺寸关系 + 拉普拉斯重构与原图一致（交叉验证）。

namespace {

cv::Mat MakePyramidInput()
{
    cv::Mat img(64, 64, CV_8UC1);
    for (int y = 0; y < img.rows; ++y) {
        for (int x = 0; x < img.cols; ++x) {
            img.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
        }
    }
    return img;
}

} // namespace

inline void PyramidTest()
{
    std::cout << "===== Pyramid test begin =====" << std::endl;

    // 1) 高斯金字塔：第 0 层 == 原图，后续层尺寸逐层减半
    {
        const cv::Mat src = MakePyramidInput();
        const auto pyr = ImageProcessing::BuildGaussianPyramid(src, 4);

        CU_CHECK(pyr.size() == 4);
        CU_CHECK_EQ(cv::countNonZero(pyr[0] != src), 0);
        for (std::size_t i = 1; i < pyr.size(); ++i) {
            CU_CHECK(pyr[i].cols == (pyr[i - 1].cols + 1) / 2);
            CU_CHECK(pyr[i].rows == (pyr[i - 1].rows + 1) / 2);
        }
        std::cout << "  gaussian pyramid sizes OK" << std::endl;
    }

    // 2) 拉普拉斯金字塔：层数正确，重构与原图一致
    {
        const cv::Mat src = MakePyramidInput();
        const auto pyr = ImageProcessing::BuildLaplacianPyramid(src, 4);
        CU_CHECK(pyr.size() == 4);

        const cv::Mat recon = ImageProcessing::ReconstructFromLaplacian(pyr);
        CU_CHECK(recon.size() == src.size());

        cv::Mat diff;
        cv::absdiff(recon, src, diff);
        double maxDiff = 0.0;
        cv::minMaxLoc(diff, nullptr, &maxDiff);
        CU_CHECK(maxDiff < 2.0);
        std::cout << "  laplacian reconstruction OK (maxDiff=" << maxDiff << ")" << std::endl;
    }

    // 3) 层数过大时安全限制到最大可行层数，不崩溃
    {
        const cv::Mat src = MakePyramidInput();
        const auto pyr = ImageProcessing::BuildGaussianPyramid(src, 100);
        CU_CHECK(!pyr.empty());
        CU_CHECK(pyr.back().cols >= 1);
        CU_CHECK(pyr.back().rows >= 1);
        std::cout << "  level clamping OK (levels=" << pyr.size() << ")" << std::endl;
    }

    std::cout << "===== Pyramid test passed =====" << std::endl;
}

#endif // TESTS_PYRAMIDTEST_H
