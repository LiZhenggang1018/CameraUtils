#ifndef TESTS_FEATUREMATCHING_SIFTDETECTORREPORT_H
#define TESTS_FEATUREMATCHING_SIFTDETECTORREPORT_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/SiftDetector.h"

#include "CommonEnv.h"

// 手写 SIFT 数值输出 demo（Label::UNIT，不进 GUI，纯打印）。
// 读取 data/FeatureMatching/SiftDetector/input.jpg，打印关键点数量、尺度/方向分布，
// 便于在命令行量化观察手写 SIFT 在真实图片上的表现，无需开窗口。
inline int SiftDetectorReport()
{
    const std::string inputPath = EnvRootPath() + "/data/FeatureMatching/SiftDetector/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[SiftDetectorReport] %s not found or unreadable\n", inputPath.c_str());
        return 1;
    }

    std::cout << "===== SiftDetector report begin =====" << std::endl;
    std::cout << "image: " << inputPath << "  " << src.cols << "x" << src.rows << std::endl;

    FeatureMatching::SiftDetector detector;
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    detector.DetectAndCompute(src, keypoints, descriptors);

    std::cout << "keypoints: " << keypoints.size() << std::endl;
    if (keypoints.empty()) {
        std::cout << "===== SiftDetector report end (no keypoints) =====" << std::endl;
        return 0;
    }

    std::cout << "descriptors: " << descriptors.rows << "x" << descriptors.cols << " type=" << descriptors.type()
              << std::endl;

    // 尺度统计（size 即尺度）。
    float minSize = keypoints.front().size;
    float maxSize = keypoints.front().size;
    for (const auto& kp : keypoints) {
        minSize = std::min(minSize, kp.size);
        maxSize = std::max(maxSize, kp.size);
    }
    std::cout << "size range: " << minSize << " .. " << maxSize << std::endl;

    // 方向分布（按 8 个象限桶统计主方向角度）。
    constexpr int bins = 8;
    std::vector<int> orientHist(bins, 0);
    for (const auto& kp : keypoints) {
        double angle = kp.angle;
        if (angle < 0.0) {
            angle += 360.0;
        }
        const int bin = std::min(bins - 1, static_cast<int>(angle / (360.0 / bins)));
        ++orientHist[bin];
    }
    std::cout << "orientation histogram (deg -> count):" << std::endl;
    for (int i = 0; i < bins; ++i) {
        std::printf("  [%3d, %3d): %d\n", i * 45, (i + 1) * 45, orientHist[i]);
    }

    std::cout << "===== SiftDetector report end =====" << std::endl;
    return 0;
}

#endif // TESTS_FEATUREMATCHING_SIFTDETECTORREPORT_H
