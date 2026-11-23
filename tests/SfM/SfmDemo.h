#ifndef TESTS_SFM_SFMDEMO_H
#define TESTS_SFM_SFMDEMO_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "SfM/Calibration.h"
#include "SfM/Epipolar.h"
#include "SfM/Pnp.h"
#include "SfM/Triangulation.h"

// SfM 数值输出 demo（Label::UNIT，不进 GUI）。
// 用合成数据跑四个模块（标定/对极/三角化/PnP），打印关键数值结果，展示几何视觉的完整链路。

namespace {

cv::Mat DemoK()
{
    return (cv::Mat_<double>(3, 3) << 800, 0, 320, 0, 800, 240, 0, 0, 1);
}

} // namespace

inline int SfmDemo()
{
    std::cout << "===== SfM demo begin =====" << std::endl;

    // 1. 相机标定
    {
        const cv::Mat K = DemoK();
        std::vector<cv::Point3f> obj;
        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 9; ++c) {
                obj.emplace_back(static_cast<float>(c), static_cast<float>(r), 0.0f);
            }
        }
        std::vector<std::vector<cv::Point3f>> allObj;
        std::vector<std::vector<cv::Point2f>> allImg;
        for (int i = 0; i < 4; ++i) {
            cv::Mat rvec = (cv::Mat_<double>(3, 1) << i * 0.1, i * 0.05, 0.0);
            cv::Mat tvec = (cv::Mat_<double>(3, 1) << 0.0, 0.0, 25.0 + i);
            std::vector<cv::Point2f> img;
            cv::projectPoints(obj, rvec, tvec, K, cv::Mat(), img);
            allObj.push_back(obj);
            allImg.push_back(img);
        }
        cv::Mat Kcal;
        cv::Mat dist;
        if (SfM::CalibrateCamera(allObj, allImg, cv::Size(640, 480), Kcal, dist)) {
            std::cout << "  [Calibration] fx=" << Kcal.at<double>(0, 0) << " fy=" << Kcal.at<double>(1, 1)
                      << " cx=" << Kcal.at<double>(0, 2) << " cy=" << Kcal.at<double>(1, 2) << std::endl;
        }
    }

    // 2. 对极几何 + 三角化 + PnP 的完整链路
    {
        const cv::Mat K = DemoK();
        const cv::Mat R =
            (cv::Mat_<double>(3, 3) << std::cos(0.1), 0, std::sin(0.1), 0, 1, 0, -std::sin(0.1), 0, std::cos(0.1));
        const cv::Mat t = (cv::Mat_<double>(3, 1) << 1.0, 0.0, 0.0);

        // 3D 点
        std::vector<cv::Point3f> obj;
        cv::RNG rng(9);
        for (int i = 0; i < 15; ++i) {
            obj.emplace_back(rng.uniform(-1.5f, 1.5f), rng.uniform(-1.5f, 1.5f), rng.uniform(4.0f, 8.0f));
        }

        std::vector<cv::Point2f> pts1;
        std::vector<cv::Point2f> pts2;
        cv::Mat rvec;
        cv::Rodrigues(R, rvec);
        cv::projectPoints(obj, cv::Mat::zeros(3, 1, CV_64F), cv::Mat::zeros(3, 1, CV_64F), K, cv::Mat(), pts1);
        cv::projectPoints(obj, rvec, t, K, cv::Mat(), pts2);

        const cv::Mat F = SfM::ComputeFundamental(pts1, pts2);
        const cv::Mat E = SfM::ComputeEssential(pts1, pts2, K);
        std::cout << "  [Epipolar] F=" << (F.empty() ? "empty" : "3x3") << " E=" << (E.empty() ? "empty" : "3x3")
                  << std::endl;

        // 三角化
        const cv::Mat P1 = K * (cv::Mat_<double>(3, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
        cv::Mat Rt(3, 4, CV_64F);
        R.copyTo(Rt(cv::Rect(0, 0, 3, 3)));
        t.copyTo(Rt(cv::Rect(3, 0, 1, 3)));
        const cv::Mat P2 = K * Rt;
        const cv::Mat pts4d = SfM::TriangulatePoints(P1, P2, pts1, pts2);
        std::cout << "  [Triangulation] pts=" << (pts4d.empty() ? 0 : pts4d.cols) << " (4xN)" << std::endl;

        // PnP（用三角化的 3D 点 + 视图2 的 2D 点）
        cv::Mat Rp;
        cv::Mat tp;
        if (SfM::SolvePnp(obj, pts2, K, cv::Mat(), Rp, tp)) {
            std::cout << "  [PnP] R/t recovered OK" << std::endl;
        }
    }

    std::cout << "===== SfM demo end =====" << std::endl;
    return 0;
}

#endif // TESTS_SFM_SFMDEMO_H
