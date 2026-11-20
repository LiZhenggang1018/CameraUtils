#ifndef TESTS_ALIGNMENT_EXPOSURECOMPENSATIONTEST_H
#define TESTS_ALIGNMENT_EXPOSURECOMPENSATIONTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/ExposureCompensation.h"

#include "cli/TestAssert.h"

// 曝光补偿模块断言测试。
// 真值来源：构造不同增益的图，验证增益估计与 ApplyGain 正确性。

namespace {

cv::Mat MakeConstantImage(int size, std::uint8_t value)
{
    return cv::Mat(size, size, CV_8UC1, cv::Scalar(value));
}

} // namespace

inline void ExposureCompensationTest()
{
    std::cout << "===== ExposureCompensation test begin =====" << std::endl;

    // 1) 不同增益的常数图，EstimateGains 估计的增益能拉平亮度（比率≈增益比）
    {
        const cv::Mat img1 = MakeConstantImage(128, 100);
        const cv::Mat img2 = MakeConstantImage(128, 200);
        const std::vector<cv::Mat> images = {img1, img2};

        const auto gains = Alignment::EstimateGains(images);

        CU_CHECK(gains.size() == 2);
        CU_CHECK(gains[0] > 0.0 && gains[1] > 0.0);
        // img2 更亮，它的增益应更小（拉平到同一亮度）
        CU_CHECK(gains[0] > gains[1]);
        std::cout << "  EstimateGains OK (g0=" << gains[0] << " g1=" << gains[1] << ")" << std::endl;
    }

    // 2) ApplyGain 正确缩放亮度
    {
        const cv::Mat img = MakeConstantImage(128, 100);
        const cv::Mat out = Alignment::ApplyGain(img, 2.0);

        CU_CHECK(!out.empty());
        // 100 * 2 = 200
        CU_CHECK(out.at<std::uint8_t>(0, 0) >= 195 && out.at<std::uint8_t>(0, 0) <= 205);
        std::cout << "  ApplyGain OK (val=" << static_cast<int>(out.at<std::uint8_t>(0, 0)) << ")" << std::endl;
    }

    // 3) 全黑图除零防护（不崩溃）
    {
        const cv::Mat black = MakeConstantImage(64, 0);
        const std::vector<cv::Mat> images = {black, black};
        const auto gains = Alignment::EstimateGains(images);
        CU_CHECK(gains.size() == 2);
        std::cout << "  black-image safety OK" << std::endl;
    }

    // 4) 空输入安全
    {
        const std::vector<cv::Mat> empty;
        CU_CHECK(Alignment::EstimateGains(empty).empty());
        CU_CHECK(Alignment::ApplyGain(cv::Mat(), 1.0).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== ExposureCompensation test passed =====" << std::endl;
}

#endif // TESTS_ALIGNMENT_EXPOSURECOMPENSATIONTEST_H
