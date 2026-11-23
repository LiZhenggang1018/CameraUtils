#ifndef TESTS_SFM_PNPTEST_H
#define TESTS_SFM_PNPTEST_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Pnp.h"

#include "cli/TestAssert.h"

// PnP 断言测试。
// 真值来源：已知 R/t 与非共面 3D 点，投影得 2D 点，恢复的 R/t 应接近真值（重投影误差小）。

namespace {

cv::Mat MakePnpK()
{
    return (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
}

} // namespace

inline void PnpTest()
{
    std::cout << "===== Pnp test begin =====" << std::endl;

    const cv::Mat K = MakePnpK();

    // 非共面 3D 点（z 变化，避免 PnP 退化）
    std::vector<cv::Point3f> objPts;
    cv::RNG rng(5);
    for (int i = 0; i < 20; ++i) {
        objPts.emplace_back(rng.uniform(-2.0f, 2.0f), rng.uniform(-2.0f, 2.0f), rng.uniform(2.0f, 10.0f));
    }

    // 真值位姿
    const cv::Mat rvecTrue = (cv::Mat_<double>(3, 1) << 0.2, 0.1, -0.05);
    const cv::Mat tvecTrue = (cv::Mat_<double>(3, 1) << 0.5, -0.3, 3.0);

    // 投影
    std::vector<cv::Point2f> imgPts;
    cv::projectPoints(objPts, rvecTrue, tvecTrue, K, cv::Mat(), imgPts);

    // PnP 求解
    cv::Mat R;
    cv::Mat t;
    const bool ok = SfM::SolvePnp(objPts, imgPts, K, cv::Mat(), R, t);

    CU_CHECK(ok);
    CU_CHECK(!R.empty() && !t.empty());

    // 重投影误差小（用恢复的 R/t 重新投影，与输入 2D 点误差 < 阈值）
    cv::Mat rvecRecovered;
    cv::Rodrigues(R, rvecRecovered);
    std::vector<cv::Point2f> reproj;
    cv::projectPoints(objPts, rvecRecovered, t, K, cv::Mat(), reproj);

    double maxErr = 0.0;
    for (std::size_t i = 0; i < imgPts.size(); ++i) {
        maxErr = std::max(maxErr, cv::norm(imgPts[i] - reproj[i]));
    }
    std::cout << "  PnP reprojection maxErr=" << maxErr << std::endl;
    CU_CHECK(maxErr < 1.0);

    // 空输入安全
    {
        const std::vector<cv::Point3f> emptyObj;
        const std::vector<cv::Point2f> emptyImg;
        cv::Mat r;
        cv::Mat tv;
        CU_CHECK(!SfM::SolvePnp(emptyObj, emptyImg, K, cv::Mat(), r, tv));
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Pnp test passed =====" << std::endl;
}

#endif // TESTS_SFM_PNPTEST_H
