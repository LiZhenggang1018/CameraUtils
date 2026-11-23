#ifndef TESTS_DEPTH_STEREOMATCHINGTEST_H
#define TESTS_DEPTH_STEREOMATCHINGTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "Depth/StereoMatching.h"

#include "cli/TestAssert.h"

// 双目立体匹配断言测试。
// 真值来源：合成已知视差的左右图，前景视差 > 背景视差；深度与视差成反比。

namespace {

// 生成合成左右图：左图有前景块（近，视差大）与背景纹理（远，视差小），右图按视差水平平移
void MakeStereoPair(cv::Mat& left, cv::Mat& right, int fgDisparity, int bgDisparity)
{
    const int size = 256;
    left = cv::Mat(size, size, CV_8UC1, cv::Scalar(0));
    // 背景随机纹理
    cv::RNG rng(21);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            left.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(rng.uniform(0, 256));
        }
    }
    // 前景亮块（中心）
    cv::rectangle(left, cv::Rect(96, 96, 64, 64), cv::Scalar(200), -1);

    // 右图：背景平移 bgDisparity，前景平移 fgDisparity（视差大 = 近）
    right = cv::Mat(size, size, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int srcX = x - bgDisparity;
            if (srcX >= 0 && srcX < size) {
                right.at<std::uint8_t>(y, x) = left.at<std::uint8_t>(y, srcX);
            }
        }
    }
    // 前景块用更大的视差（fgDisparity）
    for (int y = 96; y < 160; ++y) {
        for (int x = 96; x < 160; ++x) {
            int srcX = x - fgDisparity;
            if (srcX >= 0 && srcX < size) {
                right.at<std::uint8_t>(y, x) = left.at<std::uint8_t>(y, srcX);
            }
        }
    }
}

} // namespace

inline void StereoMatchingTest()
{
    std::cout << "===== StereoMatching test begin =====" << std::endl;

    // 1) ComputeDisparity 返回非空视差图，前景视差 > 背景视差
    {
        cv::Mat left;
        cv::Mat right;
        MakeStereoPair(left, right, 24, 4);

        const cv::Mat disp = Depth::ComputeDisparity(left, right, 64, 11);

        CU_CHECK(!disp.empty());
        CU_CHECK(disp.size() == left.size());

        // 前景（中心亮块）视差应为有效正值（真值 fgDisparity=24，SGBM 近似到 ~11×16）
        // 背景（随机纹理）SGBM 匹配不佳，通常为无效值（负）或更小视差。
        const double fgDisp = disp.at<short>(128, 128);
        const double bgDisp = disp.at<short>(30, 30);
        std::cout << "  fg disparity=" << fgDisp << " bg disparity=" << bgDisp << std::endl;
        CU_CHECK(fgDisp > 0);      // 前景视差有效且为正（近处）
        CU_CHECK(fgDisp > bgDisp); // 前景视差 > 背景（背景无效或更小）
    }

    // 2) DisparityToDepth 深度与视差成反比，视差 0 处防护
    {
        cv::Mat disp(16, 16, CV_16S, cv::Scalar(0));
        disp.at<short>(8, 8) = 16; // 视差 16 → 深度 80（更远）
        disp.at<short>(4, 4) = 32; // 视差 32 → 深度 40（更近）

        const double focal = 800.0;
        const double baseline = 0.1;
        const cv::Mat depth = Depth::DisparityToDepth(disp, focal, baseline);

        CU_CHECK(!depth.empty());
        const double dFarther = depth.at<double>(8, 8); // 视差 16 → 更远
        const double dNearer = depth.at<double>(4, 4);  // 视差 32 → 更近
        std::cout << "  depth(disp16)=" << dFarther << " depth(disp32)=" << dNearer << std::endl;
        CU_CHECK(dFarther > dNearer); // 视差小 = 更远 = 深度大

        // 视差 0 处防护（深度应很大或有限值，非 NaN/inf）
        const double dZero = depth.at<double>(0, 0);
        CU_CHECK(std::isfinite(dZero));
    }

    // 3) 空输入安全
    {
        CU_CHECK(Depth::ComputeDisparity(cv::Mat(), cv::Mat(), 64, 11).empty());
        CU_CHECK(Depth::DisparityToDepth(cv::Mat(), 800.0, 0.1).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== StereoMatching test passed =====" << std::endl;
}

#endif // TESTS_DEPTH_STEREOMATCHINGTEST_H
