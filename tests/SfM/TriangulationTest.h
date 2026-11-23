#ifndef TESTS_SFM_TRIANGULATIONTEST_H
#define TESTS_SFM_TRIANGULATIONTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Triangulation.h"

#include "cli/TestAssert.h"

// 三角化断言测试。
// 真值来源：已知投影矩阵 P1/P2 与 3D 点，投影到两视图，重建的 3D 点应接近真值。

namespace {

cv::Mat MakeTriangK()
{
    return (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
}

} // namespace

inline void TriangulationTest()
{
    std::cout << "===== Triangulation test begin =====" << std::endl;

    const cv::Mat K = MakeTriangK();
    // 视图1：原点；视图2：绕 y 轴转 0.1，x 平移 1.0
    const cv::Mat R2 =
        (cv::Mat_<double>(3, 3) << std::cos(0.1), 0, std::sin(0.1), 0, 1, 0, -std::sin(0.1), 0, std::cos(0.1));
    const cv::Mat t2 = (cv::Mat_<double>(3, 1) << 1.0, 0.0, 0.0);

    // 投影矩阵 P1 = K[I|0], P2 = K[R|t]
    const cv::Mat P1 = K * (cv::Mat_<double>(3, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
    cv::Mat Rt2(3, 4, CV_64F);
    R2.copyTo(Rt2(cv::Rect(0, 0, 3, 3)));
    t2.copyTo(Rt2(cv::Rect(3, 0, 1, 3)));
    const cv::Mat P2 = K * Rt2;

    // 真值 3D 点（相机前方）
    std::vector<cv::Point3f> truePts;
    cv::RNG rng(3);
    for (int i = 0; i < 10; ++i) {
        truePts.emplace_back(rng.uniform(-1.5f, 1.5f), rng.uniform(-1.5f, 1.5f), rng.uniform(4.0f, 8.0f));
    }

    // 投影到两视图
    std::vector<cv::Point2f> pts1;
    std::vector<cv::Point2f> pts2;
    for (const auto& p : truePts) {
        const cv::Mat x = (cv::Mat_<double>(4, 1) << p.x, p.y, p.z, 1.0);
        const cv::Mat u1 = P1 * x;
        const cv::Mat u2 = P2 * x;
        pts1.emplace_back(static_cast<float>(u1.at<double>(0, 0) / u1.at<double>(2, 0)),
                          static_cast<float>(u1.at<double>(1, 0) / u1.at<double>(2, 0)));
        pts2.emplace_back(static_cast<float>(u2.at<double>(0, 0) / u2.at<double>(2, 0)),
                          static_cast<float>(u2.at<double>(1, 0) / u2.at<double>(2, 0)));
    }

    // 三角化
    const cv::Mat pts4d = SfM::TriangulatePoints(P1, P2, pts1, pts2);

    CU_CHECK(!pts4d.empty());
    CU_CHECK(pts4d.rows == 4);

    // 重建的 3D 点（齐次坐标归一化）接近真值，且深度 z > 0
    double maxErr = 0.0;
    for (int i = 0; i < pts4d.cols; ++i) {
        const double w = pts4d.at<double>(3, i);
        CU_CHECK(std::abs(w) > 1e-9);
        const double X = pts4d.at<double>(0, i) / w;
        const double Y = pts4d.at<double>(1, i) / w;
        const double Z = pts4d.at<double>(2, i) / w;
        CU_CHECK(Z > 0.0); // 相机前方

        const double err = std::sqrt((X - truePts[i].x) * (X - truePts[i].x) + (Y - truePts[i].y) * (Y - truePts[i].y) +
                                     (Z - truePts[i].z) * (Z - truePts[i].z));
        maxErr = std::max(maxErr, err);
    }
    std::cout << "  triangulation maxErr=" << maxErr << std::endl;
    CU_CHECK(maxErr < 0.1);

    // 空输入安全
    {
        const std::vector<cv::Point2f> empty;
        CU_CHECK(SfM::TriangulatePoints(P1, P2, empty, empty).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Triangulation test passed =====" << std::endl;
}

#endif // TESTS_SFM_TRIANGULATIONTEST_H
