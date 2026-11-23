#ifndef TESTS_SFM_CALIBRATIONTEST_H
#define TESTS_SFM_CALIBRATIONTEST_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Calibration.h"

#include "cli/TestAssert.h"

// 相机标定断言测试。
// 真值来源：已知内参 K 生成多视角棋盘投影，标定恢复的内参应接近真值。

namespace {

// 生成棋盘格 objectPoints（9x6 内角点，格子 1.0）
std::vector<cv::Point3f> MakeChessboardObjectPoints(int cols, int rows)
{
    std::vector<cv::Point3f> pts;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            pts.emplace_back(static_cast<float>(c), static_cast<float>(r), 0.0f);
        }
    }
    return pts;
}

} // namespace

inline void CalibrationTest()
{
    std::cout << "===== Calibration test begin =====" << std::endl;

    // 真值内参
    const cv::Mat KTrue = (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
    const cv::Size imageSize(640, 480);
    const std::vector<cv::Point3f> objPts = MakeChessboardObjectPoints(9, 6);

    // 生成多视角投影（每视角一个已知外参 rvec/tvec）
    std::vector<std::vector<cv::Point3f>> allObj;
    std::vector<std::vector<cv::Point2f>> allImg;
    for (int i = 0; i < 6; ++i) {
        const double angle = i * 0.5;
        cv::Mat rvec = (cv::Mat_<double>(3, 1) << angle * 0.3, angle * 0.2, angle * 0.1);
        cv::Mat tvec = (cv::Mat_<double>(3, 1) << 0.0, 0.0, 20.0 + i);

        std::vector<cv::Point2f> imgPts;
        cv::projectPoints(objPts, rvec, tvec, KTrue, cv::Mat(), imgPts);

        allObj.push_back(objPts);
        allImg.push_back(imgPts);
    }

    // 标定
    cv::Mat K;
    cv::Mat dist;
    const bool ok = SfM::CalibrateCamera(allObj, allImg, imageSize, K, dist);

    CU_CHECK(ok);
    CU_CHECK(!K.empty());
    CU_CHECK(K.rows == 3 && K.cols == 3);

    // 内参接近真值（fx/fy/cx/cy 误差 < 5%）
    const double fx = K.at<double>(0, 0);
    const double fy = K.at<double>(1, 1);
    const double cx = K.at<double>(0, 2);
    const double cy = K.at<double>(1, 2);
    std::cout << "  K = [fx=" << fx << " fy=" << fy << " cx=" << cx << " cy=" << cy << "]" << std::endl;

    CU_CHECK(std::abs(fx - 800.0) < 40.0);
    CU_CHECK(std::abs(fy - 800.0) < 40.0);
    CU_CHECK(std::abs(cx - 320.0) < 16.0);
    CU_CHECK(std::abs(cy - 240.0) < 12.0);

    std::cout << "===== Calibration test passed =====" << std::endl;
}

#endif // TESTS_SFM_CALIBRATIONTEST_H
