#ifndef TESTS_ALIGNMENT_BLENDINGTEST_H
#define TESTS_ALIGNMENT_BLENDINGTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "Alignment/Blending.h"

#include "cli/TestAssert.h"

// 图像融合（多波段 blending）模块断言测试。
// 真值来源：重叠区融合后与原因接近、亮度正常。

namespace {

// 生成左亮右暗的测试图（两半亮度不同，用于验证融合过渡）
cv::Mat MakeSplitImage(int size, std::uint8_t left, std::uint8_t right)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(left));
    for (int y = 0; y < size; ++y) {
        for (int x = size / 2; x < size; ++x) {
            img.at<std::uint8_t>(y, x) = right;
        }
    }
    return img;
}

} // namespace

inline void BlendingTest()
{
    std::cout << "===== Blending test begin =====" << std::endl;

    // 1) 相同图融合（mask 各半），结果尺寸正确且非空
    {
        const cv::Mat img = MakeSplitImage(128, 100, 100);
        const cv::Mat maskLeft = MakeSplitImage(128, 255, 0);
        const cv::Mat maskRight = cv::Scalar::all(255) - maskLeft;

        const cv::Mat blended = Alignment::MultiBandBlend(img, img, maskLeft, maskRight, 4);

        CU_CHECK(!blended.empty());
        CU_CHECK(blended.size() == img.size());
        std::cout << "  MultiBandBlend same-image OK" << std::endl;
    }

    // 2) 两半亮度不同的图，重叠区融合后亮度在两者之间（无硬接缝）
    {
        const cv::Mat img = MakeSplitImage(128, 50, 200);
        const cv::Mat maskLeft = MakeSplitImage(128, 255, 0);
        const cv::Mat maskRight = cv::Scalar::all(255) - maskLeft;

        const cv::Mat blended = Alignment::MultiBandBlend(img, img, maskLeft, maskRight, 4);
        CU_CHECK(!blended.empty());

        // 中心附近（重叠边界）亮度应在 50~200 之间
        const auto centerVal = blended.at<std::uint8_t>(64, 64);
        CU_CHECK(centerVal >= 50 && centerVal <= 200);
        std::cout << "  overlapping blend brightness OK (center=" << static_cast<int>(centerVal) << ")" << std::endl;
    }

    // 3) 空输入安全返回空
    {
        const cv::Mat empty;
        CU_CHECK(Alignment::MultiBandBlend(empty, empty, empty, empty, 4).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Blending test passed =====" << std::endl;
}

#endif // TESTS_ALIGNMENT_BLENDINGTEST_H
