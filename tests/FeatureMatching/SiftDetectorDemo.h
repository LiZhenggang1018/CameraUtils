#ifndef TESTS_FEATUREMATCHING_SIFTDETECTORDEMO_H
#define TESTS_FEATUREMATCHING_SIFTDETECTORDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/SiftDetector.h"

#include "CommonEnv.h"

// 手写 SIFT 可视化 demo（GUI，不进 ctest）。
// 展示手写 SIFT 检测到的关键点（含方向箭头）。按键 q / ESC 退出。
inline int SiftDetectorDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/FeatureMatching/SiftDetector/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[SiftDetectorDemo] %s not found, generating textured image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13 + (x * y) / 16) % 256);
            }
        }
    }

    FeatureMatching::SiftDetector detector;
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    detector.DetectAndCompute(src, keypoints, descriptors);

    cv::Mat display;
    cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    cv::drawKeypoints(display, keypoints, display, cv::Scalar(0, 0, 255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

    char status[256];
    std::snprintf(status, sizeof(status), "SIFT (handwritten): %zu keypoints  desc=%dx%d type=%d", keypoints.size(),
                  descriptors.cols, descriptors.rows, descriptors.type());
    cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                cv::LINE_AA);
    cv::putText(display, "q/ESC=quit", cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(200), 1, cv::LINE_AA);

    cv::namedWindow("Handwritten SIFT Demo", cv::WINDOW_AUTOSIZE);
    cv::imshow("Handwritten SIFT Demo", display);
    while (true) {
        const int key = cv::waitKey(30);
        if (key == 27 || key == 'q') {
            break;
        }
    }
    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_FEATUREMATCHING_SIFTDETECTORDEMO_H
