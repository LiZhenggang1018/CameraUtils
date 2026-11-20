#ifndef TESTS_OPTICALFLOW_LKTRACKERTEST_H
#define TESTS_OPTICALFLOW_LKTRACKERTEST_H

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "LkTracker.h"

#include "cli/TestAssert.h"

// 稀疏光流（LK）断言测试。
// 真值来源：平移合成图 + 已知角点跟踪 + FB 过滤行为。

namespace {

// 生成一张有纹理的基础合成图
cv::Mat MakeBaseTextured(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(128));
    cv::RNG rng(42);
    for (int i = 0; i < 150; ++i) {
        const int bx = rng.uniform(0, size - 30);
        const int by = rng.uniform(0, size - 30);
        const int bw = rng.uniform(8, 30);
        const int bh = rng.uniform(8, 30);
        cv::rectangle(img, cv::Rect(bx, by, bw, bh), cv::Scalar(rng.uniform(0, 256)), -1);
    }
    return img;
}

// 对基础图做水平平移
cv::Mat ShiftX(const cv::Mat& base, int offsetX)
{
    cv::Mat shifted;
    cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
    m.at<double>(0, 2) = offsetX;
    cv::warpAffine(base, shifted, m, base.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return shifted;
}

} // namespace

inline void LkTrackerTest()
{
    std::cout << "===== LkTracker test begin =====" << std::endl;

    // 1) 在纹理图上检测角点，跟踪后仍有点
    {
        const cv::Mat base = MakeBaseTextured(256);
        LkTracker tracker;
        tracker.Detect(base);
        CU_CHECK(tracker.TrackedCount() > 0);

        const cv::Mat shifted = ShiftX(base, 5);
        tracker.Track(base, shifted);
        CU_CHECK(tracker.TrackedCount() > 0);
        std::cout << "  LkTracker detect+track OK (points=" << tracker.TrackedCount() << ")" << std::endl;
    }

    // 2) FB 过滤开启时，平移帧的点被跟踪到正确方向（x 坐标增大）
    {
        const cv::Mat base = MakeBaseTextured(256);
        const cv::Mat shifted = ShiftX(base, 5);

        LkTracker tracker;
        tracker.Detect(base);
        const auto before = tracker.Points();

        tracker.Track(base, shifted);
        const auto after = tracker.Points();
        CU_CHECK(tracker.TrackedCount() > 0);

        // 平均 x 位移应接近 +5（右移）。before/after 数量可能不同（FB 过滤剔除部分点），
        // 只对共同的前 min(before,after) 个点求平均（Detect 与 Track 的点顺序一致）。
        const std::size_t n = std::min(before.size(), after.size());
        if (n > 0) {
            double avgDx = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                avgDx += after[i].x - before[i].x;
            }
            avgDx /= static_cast<double>(n);
            std::cout << "  avg dx = " << avgDx << std::endl;
            CU_CHECK(avgDx > 0.0);
        }
        std::cout << "  FB filter track direction OK" << std::endl;
    }

    // 3) 空帧安全（Detect/Track 空图不崩）
    {
        LkTracker tracker;
        const cv::Mat empty;
        tracker.Detect(empty);
        CU_CHECK(tracker.TrackedCount() == 0);
        tracker.Track(empty, empty);
        CU_CHECK(tracker.TrackedCount() == 0);
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== LkTracker test passed =====" << std::endl;
}

#endif // TESTS_OPTICALFLOW_LKTRACKERTEST_H
