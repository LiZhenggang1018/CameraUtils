#ifndef TESTS_SFM_EPIPOLARTEST_H
#define TESTS_SFM_EPIPOLARTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Epipolar.h"

#include "cli/TestAssert.h"

// 对极几何断言测试。
// 真值来源：已知内参 K + 已知 R/t 生成两视图对应点，验证 E/F 满足对极约束。

namespace {

cv::Mat MakeK()
{
    return (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
}

// 生成一组 3D 点，投影到两个视图（第二个视图相对第一个有 R/t 位姿）
void MakeCorrespondences(const cv::Mat& K, const cv::Mat& R, const cv::Mat& t, std::vector<cv::Point2f>& pts1,
                         std::vector<cv::Point2f>& pts2)
{
    // 在相机前方生成 3D 点（z 在 3~10 之间）
    std::vector<cv::Point3f> objPts;
    cv::RNG rng(7);
    for (int i = 0; i < 20; ++i) {
        objPts.emplace_back(rng.uniform(-2.0f, 2.0f), rng.uniform(-2.0f, 2.0f), rng.uniform(3.0f, 10.0f));
    }

    cv::Mat rvec;
    cv::Rodrigues(R, rvec);
    cv::projectPoints(objPts, cv::Mat::zeros(3, 1, CV_64F), cv::Mat::zeros(3, 1, CV_64F), K, cv::Mat(), pts1);
    cv::projectPoints(objPts, rvec, t, K, cv::Mat(), pts2);
}

} // namespace

inline void EpipolarTest()
{
    std::cout << "===== Epipolar test begin =====" << std::endl;

    const cv::Mat K = MakeK();
    // 第二视图相对第一视图：绕 y 轴转 0.1 rad，x 平移 1.0
    const cv::Mat R =
        (cv::Mat_<double>(3, 3) << std::cos(0.1), 0, std::sin(0.1), 0, 1, 0, -std::sin(0.1), 0, std::cos(0.1));
    const cv::Mat t = (cv::Mat_<double>(3, 1) << 1.0, 0.0, 0.0);

    std::vector<cv::Point2f> pts1;
    std::vector<cv::Point2f> pts2;
    MakeCorrespondences(K, R, t, pts1, pts2);

    // 1) ComputeFundamental 返回 3×3 非空，且满足 x'ᵀ F x = 0
    {
        const cv::Mat F = SfM::ComputeFundamental(pts1, pts2);
        CU_CHECK(!F.empty());
        CU_CHECK(F.rows == 3 && F.cols == 3);

        double maxErr = 0.0;
        for (std::size_t i = 0; i < pts1.size(); ++i) {
            const cv::Mat x1 = (cv::Mat_<double>(3, 1) << pts1[i].x, pts1[i].y, 1.0);
            const cv::Mat x2 = (cv::Mat_<double>(3, 1) << pts2[i].x, pts2[i].y, 1.0);
            const double err = std::abs(cv::Mat(x2.t() * F * x1).at<double>(0, 0));
            maxErr = std::max(maxErr, err);
        }
        std::cout << "  F epipolar constraint maxErr=" << maxErr << std::endl;
        CU_CHECK(maxErr < 1.0);
    }

    // 2) ComputeEssential 返回 3×3 非空
    {
        const cv::Mat E = SfM::ComputeEssential(pts1, pts2, K);
        CU_CHECK(!E.empty());
        CU_CHECK(E.rows == 3 && E.cols == 3);
        std::cout << "  E is 3x3 OK" << std::endl;
    }

    // 3) 空输入安全
    {
        const std::vector<cv::Point2f> empty;
        CU_CHECK(SfM::ComputeFundamental(empty, empty).empty());
        CU_CHECK(SfM::ComputeEssential(empty, empty, K).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Epipolar test passed =====" << std::endl;
}

#endif // TESTS_SFM_EPIPOLARTEST_H
