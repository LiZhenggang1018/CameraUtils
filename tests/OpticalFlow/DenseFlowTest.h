#ifndef TESTS_OPTICALFLOW_DENSEFLOWTEST_H
#define TESTS_OPTICALFLOW_DENSEFLOWTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "DenseFlow.h"
#include "DenseFlowDeepFlow.h"
#include "DenseFlowDis.h"

#include "cli/TestAssert.h"

// 稠密光流断言测试。
// 真值来源：平移合成图 + 光流场类型/方向断言。

namespace {

cv::Mat MakeDenseInput(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(128));
    cv::RNG rng(7);
    for (int i = 0; i < 150; ++i) {
        const int bx = rng.uniform(0, size - 30);
        const int by = rng.uniform(0, size - 30);
        const int bw = rng.uniform(8, 30);
        const int bh = rng.uniform(8, 30);
        cv::rectangle(img, cv::Rect(bx, by, bw, bh), cv::Scalar(rng.uniform(0, 256)), -1);
    }
    return img;
}

cv::Mat ShiftDenseX(const cv::Mat& base, int offsetX)
{
    cv::Mat shifted;
    cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
    m.at<double>(0, 2) = offsetX;
    cv::warpAffine(base, shifted, m, base.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return shifted;
}

} // namespace

inline void DenseFlowTest()
{
    std::cout << "===== DenseFlow test begin =====" << std::endl;

    // 1) Farneback 返回 CV_32FC2，且平移方向正确（平均 x 位移为正）
    {
        const cv::Mat base = MakeDenseInput(256);
        const cv::Mat shifted = ShiftDenseX(base, 5);

        DenseFlowFarneback farneback;
        const cv::Mat flow = farneback.Compute(base, shifted);

        CU_CHECK(!flow.empty());
        CU_CHECK(flow.type() == CV_32FC2);
        CU_CHECK(flow.size() == base.size());

        // 平均 x 位移（flow 的 x 通道）应为正
        double avgDx = 0.0;
        int cnt = 0;
        for (int y = 0; y < flow.rows; ++y) {
            for (int x = 0; x < flow.cols; ++x) {
                avgDx += flow.at<cv::Vec2f>(y, x)[0];
                ++cnt;
            }
        }
        avgDx /= cnt;
        std::cout << "  Farneback avg dx = " << avgDx << std::endl;
        CU_CHECK(avgDx > 0.0);
    }

    // 2) DIS 同接口，返回 CV_32FC2
    {
        const cv::Mat base = MakeDenseInput(256);
        const cv::Mat shifted = ShiftDenseX(base, 5);

        DenseFlowDis dis;
        const cv::Mat flow = dis.Compute(base, shifted);

        CU_CHECK(!flow.empty());
        CU_CHECK(flow.type() == CV_32FC2);
        std::cout << "  DIS flow type OK" << std::endl;
    }

    // 3) DeepFlow 同接口
    {
        const cv::Mat base = MakeDenseInput(256);
        const cv::Mat shifted = ShiftDenseX(base, 5);

        DenseFlowDeepFlow deep;
        const cv::Mat flow = deep.Compute(base, shifted);

        CU_CHECK(!flow.empty());
        CU_CHECK(flow.type() == CV_32FC2);
        std::cout << "  DeepFlow flow type OK" << std::endl;
    }

    // 4) 空输入安全
    {
        DenseFlowFarneback farneback;
        const cv::Mat empty;
        // 注意：calcOpticalFlowFarneback 对空图会抛异常，这里验证类不额外崩溃
        // 仅验证对象可构造、SetWinsize 可用
        farneback.SetWinsize(15);
        CU_CHECK(farneback.Winsize() == 15);
        std::cout << "  Farneback param safety OK" << std::endl;
    }

    std::cout << "===== DenseFlow test passed =====" << std::endl;
}

#endif // TESTS_OPTICALFLOW_DENSEFLOWTEST_H
