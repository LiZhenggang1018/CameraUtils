#ifndef TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGEUNITTEST_H
#define TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGEUNITTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "ComputationalPhotography/HdrMerge.h"

#include "cli/TestAssert.h"

// HDR 融合纯内存断言测试（不依赖 data/ 素材）。
// 真值来源：合成多曝光图 + 类型/非空断言。

namespace {

cv::Mat MakeBgrImage(int size, double scale)
{
    cv::Mat img(size, size, CV_8UC3);
    cv::RNG rng(5);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            auto b = static_cast<std::uint8_t>(cv::saturate_cast<int>((x * 7 + y * 3) * scale));
            auto g = static_cast<std::uint8_t>(cv::saturate_cast<int>((x * 5 + y * 11) * scale));
            auto r = static_cast<std::uint8_t>(cv::saturate_cast<int>((x * 13 + y * 2) * scale));
            img.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
        }
    }
    return img;
}

} // namespace

inline void HdrMergeUnitTest()
{
    std::cout << "===== HdrMerge unit test begin =====" << std::endl;

    // 1) MergeMertens 对多张不同亮度图返回非空 LDR（CV_8UC3）
    {
        const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());
        const std::vector<cv::Mat> images = {MakeBgrImage(128, 0.3), MakeBgrImage(128, 1.0), MakeBgrImage(128, 2.5)};

        const cv::Mat fused = ComputationalPhotography::MergeMertens(images, cfg);

        CU_CHECK(!fused.empty());
        CU_CHECK(fused.type() == CV_8UC3);
        CU_CHECK(fused.size() == images[0].size());
        std::cout << "  MergeMertens OK" << std::endl;
    }

    // 2) TonemapHdr 对合成 HDR 32F 返回 LDR 8U（CV_8UC3）
    {
        const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());
        cv::Mat hdr(128, 128, CV_32FC3, cv::Scalar(0.2, 0.5, 0.8));

        const cv::Mat ldr = ComputationalPhotography::TonemapHdr(hdr, cfg);

        CU_CHECK(!ldr.empty());
        CU_CHECK(ldr.type() == CV_8UC3);
        std::cout << "  TonemapHdr OK (type=" << ldr.type() << ")" << std::endl;
    }

    // 3) UnsharpMask 对常数图不改变（平坦区无高频）
    {
        const cv::Mat flat(128, 128, CV_8UC3, cv::Scalar(100, 100, 100));
        const cv::Mat sharp = ComputationalPhotography::UnsharpMask(flat, 1.0, 2.0, 0.0);

        CU_CHECK(!sharp.empty());
        // 平坦区锐化后仍接近原值
        const auto diff = cv::norm(sharp, flat);
        CU_CHECK(diff < 30.0);
        std::cout << "  UnsharpMask flat-invariant OK" << std::endl;
    }

    // 4) 空输入安全
    {
        const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());
        CU_CHECK(ComputationalPhotography::MergeMertens({}, cfg).empty());
        CU_CHECK(ComputationalPhotography::TonemapHdr(cv::Mat(), cfg).empty());
        CU_CHECK(ComputationalPhotography::UnsharpMask(cv::Mat(), 1.0, 2.0, 0.0).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== HdrMerge unit test passed =====" << std::endl;
}

#endif // TESTS_COMPUTATIONALPHOTOGRAPHY_HDRMERGEUNITTEST_H
