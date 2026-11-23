#ifndef TESTS_DEPTH_BOKEHTEST_H
#define TESTS_DEPTH_BOKEHTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "Depth/Bokeh.h"

#include "cli/TestAssert.h"

// 深度图 + 虚化断言测试。
// 真值来源：已知深度图（前景近、背景远），聚焦前景时背景模糊、前景清晰。

namespace {

// 生成有结构的彩色图（纹理，模糊后才看得出效果）+ 对应深度图（前景近、背景远）
void MakeBokehInput(cv::Mat& image, cv::Mat& depth)
{
    const int size = 256;
    image = cv::Mat(size, size, CV_8UC3);
    cv::RNG rng(31);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
        }
    }

    depth = cv::Mat(size, size, CV_64F, cv::Scalar(5.0));                // 背景深度 5
    cv::rectangle(depth, cv::Rect(96, 96, 64, 64), cv::Scalar(1.0), -1); // 前景深度 1
}

} // namespace

inline void BokehTest()
{
    std::cout << "===== Bokeh test begin =====" << std::endl;

    // 1) 聚焦前景（focusDepth=1），背景被模糊、前景保持清晰
    {
        cv::Mat image;
        cv::Mat depth;
        MakeBokehInput(image, depth);

        const cv::Mat bokeh = Depth::ApplyBokeh(image, depth, 1.0, 2.0);

        CU_CHECK(!bokeh.empty());
        CU_CHECK(bokeh.size() == image.size());

        // 前景中心（深度 1，聚焦）应接近原图（不被模糊）
        const cv::Vec3b fgOrig = image.at<cv::Vec3b>(128, 128);
        const cv::Vec3b fgBokeh = bokeh.at<cv::Vec3b>(128, 128);
        const double fgDiff = cv::norm(fgOrig - fgBokeh);

        // 背景（深度 5，远离聚焦）应被模糊（与邻域平均，差异比前景大）
        const cv::Vec3b bgOrig = image.at<cv::Vec3b>(30, 30);
        const cv::Vec3b bgBokeh = bokeh.at<cv::Vec3b>(30, 30);
        const double bgDiff = cv::norm(bgOrig - bgBokeh);

        std::cout << "  fg diff=" << fgDiff << " bg diff=" << bgDiff << std::endl;
        CU_CHECK(fgDiff < bgDiff); // 背景模糊程度 > 前景
    }

    // 2) 空输入安全
    {
        CU_CHECK(Depth::ApplyBokeh(cv::Mat(), cv::Mat(), 1.0, 2.0).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Bokeh test passed =====" << std::endl;
}

#endif // TESTS_DEPTH_BOKEHTEST_H
