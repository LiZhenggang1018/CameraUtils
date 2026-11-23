#ifndef TESTS_DEPTH_RECTIFICATIONTEST_H
#define TESTS_DEPTH_RECTIFICATIONTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "Depth/Rectification.h"

#include "cli/TestAssert.h"

// 极线校正断言测试。
// 真值来源：合成两视图对应点（有已知 F），校正后对应点 y 坐标应对齐（对极线水平）。

namespace {

// 生成两视图对应点：3D 点投影到两个有相对位姿的相机
void MakePointPairs(std::vector<cv::Point2f>& pts1, std::vector<cv::Point2f>& pts2)
{
    const cv::Mat K = (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
    const cv::Mat R = (cv::Mat_<double>(3, 3) << 1, 0, 0, 0, 1, 0, 0, 0, 1); // 纯平移（理想双目）
    const cv::Mat t = (cv::Mat_<double>(3, 1) << 1.0, 0.0, 0.0);

    std::vector<cv::Point3f> obj;
    cv::RNG rng(11);
    for (int i = 0; i < 30; ++i) {
        obj.emplace_back(rng.uniform(-2.0f, 2.0f), rng.uniform(-2.0f, 2.0f), rng.uniform(4.0f, 10.0f));
    }

    cv::projectPoints(obj, cv::Mat::zeros(3, 1, CV_64F), cv::Mat::zeros(3, 1, CV_64F), K, cv::Mat(), pts1);
    cv::projectPoints(obj, cv::Mat::zeros(3, 1, CV_64F), t, K, cv::Mat(), pts2);
}

} // namespace

inline void RectificationTest()
{
    std::cout << "===== Rectification test begin =====" << std::endl;

    std::vector<cv::Point2f> pts1;
    std::vector<cv::Point2f> pts2;
    MakePointPairs(pts1, pts2);

    const cv::Size imageSize(640, 480);
    cv::Mat H1;
    cv::Mat H2;
    const bool ok = Depth::RectifyUncalibrated(pts1, pts2, imageSize, H1, H2);

    CU_CHECK(ok);
    CU_CHECK(!H1.empty() && !H2.empty());

    // 校正后对应点 y 坐标对齐（对极线水平）
    double maxDy = 0.0;
    for (std::size_t i = 0; i < pts1.size(); ++i) {
        const cv::Mat x1 = (cv::Mat_<double>(3, 1) << pts1[i].x, pts1[i].y, 1.0);
        const cv::Mat x2 = (cv::Mat_<double>(3, 1) << pts2[i].x, pts2[i].y, 1.0);
        const cv::Mat r1 = H1 * x1;
        const cv::Mat r2 = H2 * x2;
        const double y1 = r1.at<double>(1, 0) / r1.at<double>(2, 0);
        const double y2 = r2.at<double>(1, 0) / r2.at<double>(2, 0);
        maxDy = std::max(maxDy, std::abs(y1 - y2));
    }
    std::cout << "  rectified max |dy| = " << maxDy << std::endl;
    CU_CHECK(maxDy < 2.0);

    // 空输入安全
    {
        const std::vector<cv::Point2f> empty;
        cv::Mat h1;
        cv::Mat h2;
        CU_CHECK(!Depth::RectifyUncalibrated(empty, empty, imageSize, h1, h2));
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Rectification test passed =====" << std::endl;
}

#endif // TESTS_DEPTH_RECTIFICATIONTEST_H
