#ifndef TESTS_NONLINEARFILTERTEST_H
#define TESTS_NONLINEARFILTERTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/NonLinearFilter.h"

#include "cli/TestAssert.h"

// 非线性滤波模块断言测试。
// 真值来源：中值滤波去椒盐噪声、双边滤波保边 + 与 OpenCV 参考实现交叉验证。

namespace {

// 3x3 中心为 255、周围为 0 的椒盐噪声点
cv::Mat MakeSaltNoise()
{
    cv::Mat img = cv::Mat::zeros(5, 5, CV_8UC1);
    img.at<std::uint8_t>(2, 2) = 255;
    return img;
}

// 水平阶跃图：上半 0、下半 255（强边缘）
cv::Mat MakeHorizontalStep()
{
    cv::Mat img = cv::Mat::zeros(9, 9, CV_8UC1);
    for (int y = 4; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            img.at<std::uint8_t>(y, x) = 255;
        }
    }
    return img;
}

} // namespace

inline void NonLinearFilterTest()
{
    std::cout << "===== NonLinearFilter test begin =====" << std::endl;

    // 1) 中值滤波去除椒盐噪声：中心噪声点被周围 0 取代
    {
        const cv::Mat noise = MakeSaltNoise();
        const cv::Mat out = ImageProcessing::MedianBlur(noise, 3);
        CU_CHECK(out.at<std::uint8_t>(2, 2) == 0);
        std::cout << "  median removes salt noise OK" << std::endl;
    }

    // 2) 双边滤波保边：阶跃边缘两侧各自保持接近原值
    {
        const cv::Mat step = MakeHorizontalStep();
        const cv::Mat out = ImageProcessing::BilateralFilter(step, 9, 50.0, 50.0);
        const std::uint8_t darkSide = out.at<std::uint8_t>(2, 4);
        const std::uint8_t brightSide = out.at<std::uint8_t>(6, 4);
        CU_CHECK(darkSide < 30);
        CU_CHECK(brightSide > 225);
        std::cout << "  bilateral preserves edge OK" << std::endl;
    }

    // 3) 交叉验证：与 cv::medianBlur / cv::bilateralFilter 逐像素一致
    {
        cv::Mat src(21, 21, CV_8UC1);
        cv::randu(src, 0, 256);

        const cv::Mat m = ImageProcessing::MedianBlur(src, 3);
        cv::Mat refM;
        cv::medianBlur(src, refM, 3);
        CU_CHECK_EQ(cv::countNonZero(m != refM), 0);

        const cv::Mat b = ImageProcessing::BilateralFilter(src, 9, 50.0, 50.0);
        cv::Mat refB;
        cv::bilateralFilter(src, refB, 9, 50.0, 50.0);
        CU_CHECK_EQ(cv::countNonZero(b != refB), 0);
        std::cout << "  cross-validation vs OpenCV OK" << std::endl;
    }

    // 4) 空输入安全返回空 Mat
    {
        const cv::Mat empty;
        CU_CHECK(ImageProcessing::MedianBlur(empty, 3).empty());
        CU_CHECK(ImageProcessing::BilateralFilter(empty, 9, 50.0, 50.0).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    // 5) 偶数/非法 diameter 规整为奇数，不崩溃
    {
        cv::Mat src(21, 21, CV_8UC1);
        cv::randu(src, 0, 256);
        const cv::Mat out = ImageProcessing::BilateralFilter(src, 10, 50.0, 50.0);
        CU_CHECK(!out.empty());
        CU_CHECK(out.size() == src.size());
        std::cout << "  even diameter normalization OK" << std::endl;
    }

    std::cout << "===== NonLinearFilter test passed =====" << std::endl;
}

#endif // TESTS_NONLINEARFILTERTEST_H
