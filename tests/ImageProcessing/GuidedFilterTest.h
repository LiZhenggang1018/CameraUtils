#ifndef TESTS_IMAGEPROCESSING_GUIDEDFILTERTEST_H
#define TESTS_IMAGEPROCESSING_GUIDEDFILTERTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/GuidedFilter.h"

#include "cli/TestAssert.h"

// 导向滤波断言测试。
// 真值来源：保边（阶跃边缘两侧保持）、去噪（平坦区平滑）、空输入安全。

namespace {

// 水平阶跃图：上半 0、下半 255（强边缘）
cv::Mat MakeGuidedStep()
{
    cv::Mat img = cv::Mat::zeros(64, 64, CV_8UC1);
    for (int y = 32; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            img.at<std::uint8_t>(y, x) = 255;
        }
    }
    return img;
}

// 带噪声的平坦图
cv::Mat MakeGuidedNoisy()
{
    cv::Mat img = cv::Mat(64, 64, CV_8UC1, cv::Scalar(128));
    cv::RNG rng(3);
    cv::Mat noise(64, 64, CV_32FC1);
    rng.fill(noise, cv::RNG::NORMAL, 0.0, 20.0);
    cv::Mat f32;
    img.convertTo(f32, CV_32F);
    cv::add(f32, noise, f32);
    cv::Mat out;
    f32.convertTo(out, CV_8U);
    return out;
}

} // namespace

inline void GuidedFilterTest()
{
    std::cout << "===== GuidedFilter test begin =====" << std::endl;

    // 1) 保边：阶跃边缘两侧各自保持接近原值
    {
        const cv::Mat step = MakeGuidedStep();
        const cv::Mat out = ImageProcessing::GuidedFilter(step, 8, 0.01 * 0.01 * 255.0 * 255.0);

        CU_CHECK(!out.empty());
        CU_CHECK(out.size() == step.size());

        const auto darkSide = out.at<std::uint8_t>(16, 16);
        const auto brightSide = out.at<std::uint8_t>(48, 48);
        CU_CHECK(darkSide < 30);
        CU_CHECK(brightSide > 225);
        std::cout << "  edge preserved (dark=" << static_cast<int>(darkSide)
                  << " bright=" << static_cast<int>(brightSide) << ")" << std::endl;
    }

    // 2) 去噪：带噪平坦图滤波后方差降低
    {
        const cv::Mat noisy = MakeGuidedNoisy();
        const cv::Mat out = ImageProcessing::GuidedFilter(noisy, 4, 0.01 * 0.01 * 255.0 * 255.0);

        cv::Scalar mean;
        cv::Scalar stddev;
        cv::meanStdDev(out, mean, stddev);
        const double outStd = stddev[0];

        cv::meanStdDev(noisy, mean, stddev);
        const double noisyStd = stddev[0];

        std::cout << "  std noisy=" << noisyStd << " filtered=" << outStd << std::endl;
        CU_CHECK(outStd < noisyStd);
    }

    // 3) 空输入安全
    {
        CU_CHECK(ImageProcessing::GuidedFilter(cv::Mat(), 8, 0.01).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== GuidedFilter test passed =====" << std::endl;
}

#endif // TESTS_IMAGEPROCESSING_GUIDEDFILTERTEST_H
