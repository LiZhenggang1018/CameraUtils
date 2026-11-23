#ifndef TESTS_DEPTH_PORTRAITDEMO_H
#define TESTS_DEPTH_PORTRAITDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "Depth/Bokeh.h"
#include "Depth/Rectification.h"
#include "Depth/StereoMatching.h"
#include "FeatureMatching/FeatureMatcher.h"
#include "FeatureMatching/LocalFeature.h"

#include "CommonEnv.h"

// 双摄虚化 / 人像模式完整链路 demo（GUI，不进 ctest）。
// 链路：真实手机左右照片 → 特征匹配 → 未标定极线校正 → SGBM 视差 → 深度 → 虚化。
// 素材：data/Depth/Portrait/left.jpg + right.jpg；缺素材时用合成左右图。
// 按键：
//   q / ESC  退出
inline int PortraitDemo()
{
    const std::string dir = EnvRootPath() + "/data/Depth/Portrait/";

    cv::Mat left = cv::imread(dir + "left.jpg", cv::IMREAD_GRAYSCALE);
    cv::Mat right = cv::imread(dir + "right.jpg", cv::IMREAD_GRAYSCALE);

    if (left.empty() || right.empty()) {
        std::printf("[PortraitDemo] 素材不足，生成合成左右图\n");
        // 合成：有纹理的图 + 已知视差（前景近、背景远）
        const int size = 400;
        left = cv::Mat(size, size, CV_8UC1);
        cv::RNG rng(41);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                left.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(rng.uniform(0, 256));
            }
        }
        cv::rectangle(left, cv::Rect(150, 150, 100, 100), cv::Scalar(200), -1);

        right = cv::Mat(size, size, CV_8UC1, cv::Scalar(0));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int srcX = x - 4; // 背景视差 4
                if (srcX >= 0 && srcX < size) {
                    right.at<std::uint8_t>(y, x) = left.at<std::uint8_t>(y, srcX);
                }
            }
        }
        for (int y = 150; y < 250; ++y) {
            for (int x = 150; x < 250; ++x) {
                int srcX = x - 16; // 前景视差 16（近）
                if (srcX >= 0 && srcX < size) {
                    right.at<std::uint8_t>(y, x) = left.at<std::uint8_t>(y, srcX);
                }
            }
        }
    }

    // 1. 特征匹配（用于极线校正的对应点）
    std::vector<cv::KeyPoint> kp1;
    std::vector<cv::KeyPoint> kp2;
    cv::Mat desc1;
    cv::Mat desc2;
    FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), left, kp1, desc1);
    FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), right, kp2, desc2);

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(desc1, desc2, knn, 2);
    const auto good = FeatureMatching::RatioTestFilter(knn, 0.75);

    std::vector<cv::Point2f> pts1;
    std::vector<cv::Point2f> pts2;
    for (const auto& m : good) {
        pts1.push_back(kp1[m.queryIdx].pt);
        pts2.push_back(kp2[m.trainIdx].pt);
    }

    // 2. 未标定极线校正
    cv::Mat H1;
    cv::Mat H2;
    const bool rectified = Depth::RectifyUncalibrated(pts1, pts2, left.size(), H1, H2);

    cv::Mat leftRect;
    cv::Mat rightRect;
    if (rectified) {
        cv::warpPerspective(left, leftRect, H1, left.size());
        cv::warpPerspective(right, rightRect, H2, right.size());
    } else {
        std::printf("[PortraitDemo] 极线校正失败，直接用原图\n");
        leftRect = left;
        rightRect = right;
    }

    // 3. SGBM 视差
    const cv::Mat disp = Depth::ComputeDisparity(leftRect, rightRect, 64, 11);

    // 4. 视差 → 深度（假设焦距 800，基线 0.1，仅相对深度）
    cv::Mat disp8u;
    if (!disp.empty()) {
        cv::Mat disp16 = disp.clone();
        disp16.convertTo(disp8u, CV_8U, 255.0 / (64.0 * 16.0));
    }

    // 5. 虚化（聚焦前景，用视差作为深度的代理）
    // 简化：直接把视差当深度（视差大 = 近）
    cv::Mat depth;
    if (!disp.empty()) {
        depth = Depth::DisparityToDepth(disp, 800.0, 0.1);
    }

    cv::Mat leftColor;
    cv::cvtColor(left, leftColor, cv::COLOR_GRAY2BGR);
    cv::Mat bokeh = leftColor;
    if (!depth.empty()) {
        // 聚焦在最小深度（前景）附近
        double minDepth = 1e9;
        double maxDepth = 0.0;
        for (int y = 0; y < depth.rows; ++y) {
            for (int x = 0; x < depth.cols; ++x) {
                const double d = depth.at<double>(y, x);
                if (d > 0) {
                    minDepth = std::min(minDepth, d);
                    maxDepth = std::max(maxDepth, d);
                }
            }
        }
        bokeh = Depth::ApplyBokeh(leftColor, depth, minDepth, std::max(1.0, (maxDepth - minDepth) * 0.5));
    }

    const std::string windowName = "Portrait Bokeh Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    // 显示：原图 + 视差图 + 虚化结果
    cv::Mat display;
    cv::Mat dispColor;
    if (!disp8u.empty()) {
        cv::applyColorMap(disp8u, dispColor, cv::COLORMAP_JET);
    } else {
        dispColor = cv::Mat::zeros(left.size(), CV_8UC3);
    }
    cv::hconcat(std::vector<cv::Mat>{leftColor, dispColor, bokeh}, display);

    cv::putText(display, "left=original middle=disparity right=bokeh", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    cv::putText(display, "q=quit", cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1,
                cv::LINE_AA);

    cv::imshow(windowName, display);
    int key = 0;
    while (key != 27 && key != 'q') {
        key = cv::waitKey(30);
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_DEPTH_PORTRAITDEMO_H
