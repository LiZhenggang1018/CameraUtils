#ifndef TESTS_FEATUREMATCHING_KEYPOINTDETECTORTEST_H
#define TESTS_FEATUREMATCHING_KEYPOINTDETECTORTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/KeypointDetector.h"

#include "cli/TestAssert.h"

// 关键点检测模块断言测试。
// 真值来源：合成棋盘格（已知角点）+ 平坦图边界情况。

namespace {

// 黑白棋盘格：角点数量确定、位置可预期
cv::Mat MakeCheckerboard(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(0));
    const int cell = size / 8;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool white = ((x / cell) + (y / cell)) % 2 == 0;
            if (white) {
                img.at<std::uint8_t>(y, x) = 255;
            }
        }
    }
    return img;
}

} // namespace

inline void KeypointDetectorTest()
{
    std::cout << "===== KeypointDetector test begin =====" << std::endl;

    // 1) Harris 在棋盘格上检测到角点
    {
        const cv::Mat board = MakeCheckerboard(256);
        const auto corners = FeatureMatching::HarrisCorners(board, 2, 3, 0.04, 120.0);
        CU_CHECK(!corners.empty());
        CU_CHECK(corners.size() > 10);
        std::cout << "  Harris corners on checkerboard: " << corners.size() << std::endl;
    }

    // 2) 平坦图返回空或极少（安全，不崩）
    {
        const cv::Mat flat(128, 128, CV_8UC1, cv::Scalar(128));
        const auto h = FeatureMatching::HarrisCorners(flat, 2, 3, 0.04, 120.0);
        const auto s = FeatureMatching::ShiTomasiCorners(flat, 100, 0.01, 10.0);
        const auto f = FeatureMatching::FastCorners(flat, 25, true);
        CU_CHECK(h.size() < 10);
        CU_CHECK(s.size() < 10);
        CU_CHECK(f.size() < 10);
        std::cout << "  flat image yields few corners OK" << std::endl;
    }

    // 3) Shi-Tomasi 的 maxCorners 参数生效
    {
        const cv::Mat board = MakeCheckerboard(256);
        const auto corners = FeatureMatching::ShiTomasiCorners(board, 20, 0.01, 10.0);
        CU_CHECK(corners.size() <= 20);
        CU_CHECK(corners.size() > 0);
        std::cout << "  Shi-Tomasi maxCorners respected: " << corners.size() << std::endl;
    }

    // 4) FAST 非极大值抑制开关影响结果
    {
        const cv::Mat board = MakeCheckerboard(256);
        const auto withNms = FeatureMatching::FastCorners(board, 25, true);
        const auto withoutNms = FeatureMatching::FastCorners(board, 25, false);
        CU_CHECK(withoutNms.size() >= withNms.size());
        std::cout << "  FAST NMS: with=" << withNms.size() << " without=" << withoutNms.size() << std::endl;
    }

    // 5) 空输入安全返回空
    {
        const cv::Mat empty;
        CU_CHECK(FeatureMatching::HarrisCorners(empty, 2, 3, 0.04, 120.0).empty());
        CU_CHECK(FeatureMatching::ShiTomasiCorners(empty, 20, 0.01, 10.0).empty());
        CU_CHECK(FeatureMatching::FastCorners(empty, 25, true).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== KeypointDetector test passed =====" << std::endl;
}

#endif // TESTS_FEATUREMATCHING_KEYPOINTDETECTORTEST_H
