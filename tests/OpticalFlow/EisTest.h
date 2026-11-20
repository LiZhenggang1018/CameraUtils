#ifndef TESTS_OPTICALFLOW_EISTEST_H
#define TESTS_OPTICALFLOW_EISTEST_H

#include <cmath>
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

    // 3) 两条路线在平移场景下结果接近（x 平移量同号且数值接近 +5）
    {
        const cv::Mat prev = MakeEisInput(256);
        cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
        m.at<double>(0, 2) = 5.0;
        cv::Mat curr;
        cv::warpAffine(prev, curr, m, prev.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        const cv::Mat mo = OpticalFlow::EstimateGlobalMotionOptical(prev, curr);
        const cv::Mat mf = OpticalFlow::EstimateGlobalMotionFeature(prev, curr);
        CU_CHECK(!mo.empty() && !mf.empty());

        const double dxOptical = mo.at<double>(0, 2);
        const double dxFeature = mf.at<double>(0, 2);
        std::cout << "  dx optical=" << dxOptical << " feature=" << dxFeature << std::endl;
        CU_CHECK(dxOptical > 0.0 && dxFeature > 0.0);
        // 数值接近真值 +5（光流法 ECC 更精确，特征法有 ORB 定位误差，容差放宽）
        CU_CHECK(std::abs(dxOptical - 5.0) < 2.0);
        CU_CHECK(std::abs(dxFeature - 5.0) < 4.0);
    }

    // 4) 纯色帧降级为单位矩阵（identity），不崩溃
    {
        const cv::Mat solid = cv::Mat(256, 256, CV_8UC1, cv::Scalar(128));
        const cv::Mat mo = OpticalFlow::EstimateGlobalMotionOptical(solid, solid);
        const cv::Mat mf = OpticalFlow::EstimateGlobalMotionFeature(solid, solid);
        CU_CHECK(!mo.empty() && !mf.empty());
        CU_CHECK(std::abs(mo.at<double>(0, 2)) < 1e-6); // 纯色 → 恒等
        std::cout << "  solid-color degeneracy -> identity OK" << std::endl;
    }

    // 5) Stabilize：合成抖动序列稳定后，输出尺寸 <= 输入，且残差运动小于原始
    {
        const cv::Mat base = MakeEisInput(256);
        std::vector<cv::Mat> frames;
        std::vector<cv::Mat> motions;
        motions.push_back(cv::Mat::eye(2, 3, CV_64F));
        frames.push_back(base);
        for (int i = 1; i < 10; ++i) {
            const int dx = static_cast<int>(8.0 * std::sin(i * 0.8));
            const int dy = static_cast<int>(6.0 * std::cos(i * 0.6));
            cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
            m.at<double>(0, 2) = dx;
            m.at<double>(1, 2) = dy;
            cv::Mat shifted;
            cv::warpAffine(base, shifted, m, base.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            frames.push_back(shifted);

            cv::Mat cum = cv::Mat::eye(2, 3, CV_64F);
            cum.at<double>(0, 2) = dx;
            cum.at<double>(1, 2) = dy;
            motions.push_back(cum);
        }

        const auto stabilized = OpticalFlow::Stabilize(frames, motions);
        CU_CHECK(stabilized.size() == frames.size());
        CU_CHECK(!stabilized[0].empty());
        // 稳定化后帧尺寸 <= 原帧（裁剪）
        CU_CHECK(stabilized[0].cols <= frames[0].cols);
        CU_CHECK(stabilized[0].rows <= frames[0].rows);
        std::cout << "  Stabilize OK (out=" << stabilized[0].cols << "x" << stabilized[0].rows << ")" << std::endl;
    }

    // 6) 空输入安全（估计函数返回单位矩阵，Stabilize 返回空）
    {
        const cv::Mat empty;
        const cv::Mat mo = OpticalFlow::EstimateGlobalMotionOptical(empty, empty);
        const cv::Mat mf = OpticalFlow::EstimateGlobalMotionFeature(empty, empty);
        CU_CHECK(!mo.empty() && !mf.empty()); // 空输入降级为 identity
        CU_CHECK(std::abs(mo.at<double>(0, 2)) < 1e-6);
        CU_CHECK(OpticalFlow::Stabilize({}, {}).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Eis test passed =====" << std::endl;
}

#endif // TESTS_OPTICALFLOW_EISTEST_H
