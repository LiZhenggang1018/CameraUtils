#ifndef TESTS_OPTICALFLOW_EISTEST_H
#define TESTS_OPTICALFLOW_EISTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "Eis.h"

#include "cli/TestAssert.h"

// EIS 防抖断言测试。
// 真值来源：合成平移序列 + 全局运动估计 + 稳定化裁剪。

namespace {

cv::Mat MakeEisInput(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(128));
    cv::RNG rng(23);
    for (int i = 0; i < 150; ++i) {
        const int bx = rng.uniform(0, size - 30);
        const int by = rng.uniform(0, size - 30);
        const int bw = rng.uniform(8, 30);
        const int bh = rng.uniform(8, 30);
        cv::rectangle(img, cv::Rect(bx, by, bw, bh), cv::Scalar(rng.uniform(0, 256)), -1);
    }
    return img;
}

} // namespace

inline void EisTest()
{
    std::cout << "===== Eis test begin =====" << std::endl;

    // 1) 光流法估计平移帧的全局运动，返回有效变换
    {
        const cv::Mat prev = MakeEisInput(256);

        cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
        m.at<double>(0, 2) = 5.0;
        cv::Mat curr;
        cv::warpAffine(prev, curr, m, prev.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        const cv::Mat motion = OpticalFlow::EstimateGlobalMotionOptical(prev, curr);
        CU_CHECK(!motion.empty());
        std::cout << "  optical global motion OK (motion rows=" << motion.rows << ")" << std::endl;
    }

    // 2) 特征法估计同样返回有效变换
    {
        const cv::Mat prev = MakeEisInput(256);
        cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
        m.at<double>(0, 2) = 5.0;
        cv::Mat curr;
        cv::warpAffine(prev, curr, m, prev.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        const cv::Mat motion = OpticalFlow::EstimateGlobalMotionFeature(prev, curr);
        CU_CHECK(!motion.empty());
        std::cout << "  feature global motion OK" << std::endl;
    }

    // 3) 两条路线在平移场景下结果接近（x 平移量符号一致）
    {
        const cv::Mat prev = MakeEisInput(256);
        cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
        m.at<double>(0, 2) = 5.0;
        cv::Mat curr;
        cv::warpAffine(prev, curr, m, prev.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        const cv::Mat mo = OpticalFlow::EstimateGlobalMotionOptical(prev, curr);
        const cv::Mat mf = OpticalFlow::EstimateGlobalMotionFeature(prev, curr);
        CU_CHECK(!mo.empty() && !mf.empty());

        // 两路线估计的 x 平移量应同号（都 > 0，右移）
        const double dxOptical = mo.at<double>(0, 2);
        const double dxFeature = mf.at<double>(0, 2);
        std::cout << "  dx optical=" << dxOptical << " feature=" << dxFeature << std::endl;
        CU_CHECK(dxOptical > 0.0 && dxFeature > 0.0);
    }

    // 4) 空输入安全
    {
        const cv::Mat empty;
        CU_CHECK(OpticalFlow::EstimateGlobalMotionOptical(empty, empty).empty());
        CU_CHECK(OpticalFlow::EstimateGlobalMotionFeature(empty, empty).empty());
        CU_CHECK(OpticalFlow::Stabilize({}, {}).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Eis test passed =====" << std::endl;
}

#endif // TESTS_OPTICALFLOW_EISTEST_H
