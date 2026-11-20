#ifndef TESTS_ALIGNMENT_VIGNETTINGTEST_H
#define TESTS_ALIGNMENT_VIGNETTINGTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "Alignment/Vignetting.h"

#include "cli/TestAssert.h"

// 渐晕校正模块断言测试。
// 真值来源：构造已知径向衰减图（中心亮、边缘暗），验证校正后边缘亮度提升。

namespace {

// 构造径向衰减图：中心亮度 255，边缘按 (1 - strength * r^2) 衰减
cv::Mat MakeVignettedImage(int size, double strength)
{
    cv::Mat img(size, size, CV_8UC1);
    const double cx = (size - 1) / 2.0;
    const double cy = (size - 1) / 2.0;
    const double maxR = cx * cx + cy * cy;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            const double r2 = (dx * dx + dy * dy) / maxR;
            const double attenuation = 1.0 - strength * r2;
            img.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(255.0 * std::max(0.0, attenuation));
        }
    }
    return img;
}

} // namespace

inline void VignettingTest()
{
    std::cout << "===== Vignetting test begin =====" << std::endl;

    // 1) 校正后边缘亮度提升（接近中心）
    {
        const cv::Mat img = MakeVignettedImage(256, 0.6);
        const cv::Mat corrected = Alignment::CorrectVignetting(img, 0.6);

        CU_CHECK(!corrected.empty());
        CU_CHECK(corrected.size() == img.size());

        // 边缘角落像素（暗）校正后应明显变亮
        const auto cornerBefore = img.at<std::uint8_t>(0, 0);
        const auto cornerAfter = corrected.at<std::uint8_t>(0, 0);
        CU_CHECK(cornerAfter > cornerBefore);
        std::cout << "  corner brightened: " << static_cast<int>(cornerBefore) << " -> "
                  << static_cast<int>(cornerAfter) << std::endl;
    }

    // 2) 中心区域基本不变（增益≈1）
    {
        const cv::Mat img = MakeVignettedImage(256, 0.6);
        const cv::Mat corrected = Alignment::CorrectVignetting(img, 0.6);

        const int cy = img.rows / 2;
        const int cx = img.cols / 2;
        const auto centerBefore = img.at<std::uint8_t>(cy, cx);
        const auto centerAfter = corrected.at<std::uint8_t>(cy, cx);
        CU_CHECK(std::abs(static_cast<int>(centerAfter) - static_cast<int>(centerBefore)) <= 5);
        std::cout << "  center unchanged OK" << std::endl;
    }

    // 3) 空输入安全
    {
        CU_CHECK(Alignment::CorrectVignetting(cv::Mat(), 0.5).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Vignetting test passed =====" << std::endl;
}

#endif // TESTS_ALIGNMENT_VIGNETTINGTEST_H
