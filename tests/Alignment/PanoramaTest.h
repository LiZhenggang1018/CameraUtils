#ifndef TESTS_ALIGNMENT_PANORAMATEST_H
#define TESTS_ALIGNMENT_PANORAMATEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Panorama.h"

#include "cli/TestAssert.h"

// 多图全景拼接断言测试。
// 真值来源：同一张大图切成重叠的若干张，拼接结果应覆盖完整场景。

namespace {

// 生成一张大纹理图（含明显结构/角点，利于特征匹配），然后切成横向重叠的若干张
std::vector<cv::Mat> MakeOverlappingSlices(int bigWidth, int height, int sliceCount, int overlap)
{
    cv::Mat big(height, bigWidth, CV_8UC1, cv::Scalar(128));
    // 随机块状纹理，提供丰富的特征点
    cv::RNG rng(12345);
    for (int i = 0; i < 200; ++i) {
        const int bx = rng.uniform(0, bigWidth - 30);
        const int by = rng.uniform(0, height - 30);
        const int bw = rng.uniform(10, 40);
        const int bh = rng.uniform(10, 40);
        const int intensity = rng.uniform(0, 256);
        cv::rectangle(big, cv::Rect(bx, by, bw, bh), cv::Scalar(intensity), -1);
    }

    const int sliceWidth = (bigWidth + (sliceCount - 1) * overlap) / sliceCount;
    std::vector<cv::Mat> slices;
    for (int i = 0; i < sliceCount; ++i) {
        int x0 = i * (sliceWidth - overlap);
        if (x0 + sliceWidth > bigWidth) {
            x0 = bigWidth - sliceWidth;
        }
        if (x0 < 0) {
            x0 = 0;
        }
        slices.push_back(big(cv::Rect(x0, 0, sliceWidth, height)).clone());
    }
    return slices;
}

} // namespace

inline void PanoramaTest()
{
    std::cout << "===== Panorama test begin =====" << std::endl;

    // 1) 重叠切片拼接，结果非空且宽度大于单张（拼接完整性由真实图 demo 验证）
    {
        const auto slices = MakeOverlappingSlices(800, 300, 3, 100);
        const cv::Mat pano = Alignment::StitchPanorama(slices);

        CU_CHECK(!pano.empty());
        // 拼接结果宽度应大于单张切片（至少拼上了相邻图）
        CU_CHECK(pano.cols > slices[0].cols);
        std::cout << "  StitchPanorama OK (pano=" << pano.cols << "x" << pano.rows << ")" << std::endl;
    }

    // 2) 空输入安全
    {
        CU_CHECK(Alignment::StitchPanorama({}).empty());
        CU_CHECK(Alignment::StitchPanorama({cv::Mat()}).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Panorama test passed =====" << std::endl;
}

#endif // TESTS_ALIGNMENT_PANORAMATEST_H
