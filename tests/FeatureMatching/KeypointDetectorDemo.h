#ifndef TESTS_FEATUREMATCHING_KEYPOINTDETECTORDEMO_H
#define TESTS_FEATUREMATCHING_KEYPOINTDETECTORDEMO_H

#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "FeatureMatching/KeypointDetector.h"

#include "CommonEnv.h"

// 关键点检测交互式 demo（GUI，不进 ctest）。
// 按键：
//   h        切换 Harris
//   s        切换 Shi-Tomasi
//   f        切换 FAST
//   +/-      调整阈值（Harris/FAST）
//   r        重置参数
//   q / ESC  退出
inline int KeypointDetectorDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/FeatureMatching/KeypointDetector/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[KeypointDetectorDemo] %s not found, generating checkerboard\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        const int cell = 64;
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                const bool white = ((x / cell) + (y / cell)) % 2 == 0;
                if (white) {
                    src.at<std::uint8_t>(y, x) = 255;
                }
            }
        }
    }

    enum class Mode { Harris, ShiTomasi, Fast };
    Mode mode = Mode::Harris;
    double harrisThreshold = 120.0;
    int fastThreshold = 25;

    const std::string windowName = "Keypoint Detector Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    bool running = true;
    while (running) {
        cv::Mat display;
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);

        std::vector<cv::KeyPoint> keypoints;
        std::string label;
        if (mode == Mode::Harris) {
            keypoints = FeatureMatching::HarrisCorners(src, 2, 3, 0.04, harrisThreshold);
            label = "Harris";
        } else if (mode == Mode::ShiTomasi) {
            keypoints = FeatureMatching::ShiTomasiCorners(src, 200, 0.01, 10.0);
            label = "Shi-Tomasi";
        } else {
            keypoints = FeatureMatching::FastCorners(src, fastThreshold, true);
            label = "FAST";
        }

        cv::drawKeypoints(display, keypoints, display, cv::Scalar(0, 0, 255),
                          cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

        char status[256];
        std::snprintf(status, sizeof(status), "%s: %zu keypoints  harrisThr=%.0f fastThr=%d", label.c_str(),
                      keypoints.size(), harrisThreshold, fastThreshold);
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "h=Harris s=ShiTomasi f=FAST +/-=threshold r=reset q=quit",
                    cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 'h':
            mode = Mode::Harris;
            break;
        case 's':
            mode = Mode::ShiTomasi;
            break;
        case 'f':
            mode = Mode::Fast;
            break;
        case '+':
        case '=':
            harrisThreshold += 10.0;
            fastThreshold += 5;
            break;
        case '-':
            harrisThreshold = std::max(10.0, harrisThreshold - 10.0);
            fastThreshold = std::max(5, fastThreshold - 5);
            break;
        case 'r':
            harrisThreshold = 120.0;
            fastThreshold = 25;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_FEATUREMATCHING_KEYPOINTDETECTORDEMO_H
