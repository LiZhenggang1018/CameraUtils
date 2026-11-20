#ifndef TESTS_FEATUREMATCHING_LOCALFEATUREDEMO_H
#define TESTS_FEATUREMATCHING_LOCALFEATUREDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/LocalFeature.h"

#include "CommonEnv.h"

// 局部特征交互式 demo（GUI，不进 ctest）。
// 按键：
//   s        切换 SIFT
//   o        切换 ORB
//   a        切换 AKAZE
//   q / ESC  退出
inline int LocalFeatureDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/FeatureMatching/LocalFeature/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[LocalFeatureDemo] %s not found, generating textured image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13 + (x * y) / 16) % 256);
            }
        }
    }

    enum class Mode { Sift, Orb, Akaze };
    Mode mode = Mode::Sift;

    const std::string windowName = "Local Feature Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    bool running = true;
    while (running) {
        cv::Mat display;
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);

        std::vector<cv::KeyPoint> keypoints;
        cv::Mat descriptors;
        std::string label;
        if (mode == Mode::Sift) {
            FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(500), src, keypoints, descriptors);
            label = "SIFT";
        } else if (mode == Mode::Orb) {
            FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), src, keypoints, descriptors);
            label = "ORB";
        } else {
            FeatureMatching::DetectAndCompute(FeatureMatching::CreateAkaze(), src, keypoints, descriptors);
            label = "AKAZE";
        }

        cv::drawKeypoints(display, keypoints, display, cv::Scalar(0, 0, 255),
                          cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

        char status[256];
        std::snprintf(status, sizeof(status), "%s: %zu keypoints  desc=%dx%d type=%d", label.c_str(), keypoints.size(),
                      descriptors.cols, descriptors.rows, descriptors.type());
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "s=SIFT o=ORB a=AKAZE q=quit", cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX,
                    0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 's':
            mode = Mode::Sift;
            break;
        case 'o':
            mode = Mode::Orb;
            break;
        case 'a':
            mode = Mode::Akaze;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_FEATUREMATCHING_LOCALFEATUREDEMO_H
