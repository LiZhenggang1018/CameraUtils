#ifndef TESTS_ALIGNMENT_BLENDINGDEMO_H
#define TESTS_ALIGNMENT_BLENDINGDEMO_H

#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "Alignment/Blending.h"

#include "CommonEnv.h"

// 图像融合交互式 demo（GUI，不进 ctest）。
// 展示两张亮度不同的图按 mask 多波段融合的效果。
// 按键：
//   q / ESC  退出
inline int BlendingDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/Alignment/Blending/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[BlendingDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
            }
        }
    }

    // 左半图和右半图（亮度不同），mask 各半重叠过渡
    cv::Mat left = src.clone();
    cv::Mat right = src.clone();
    // 让右半图变亮模拟曝光差异
    right.convertTo(right, -1, 1.3, 30);

    const int overlap = 128;
    const int cx = src.cols / 2;
    cv::Mat maskLeft = cv::Mat::zeros(src.size(), CV_8U);
    cv::Mat maskRight = cv::Mat::zeros(src.size(), CV_8U);
    for (int y = 0; y < src.rows; ++y) {
        for (int x = 0; x < src.cols; ++x) {
            if (x < cx - overlap / 2) {
                maskLeft.at<std::uint8_t>(y, x) = 255;
            } else if (x < cx + overlap / 2) {
                const double t = static_cast<double>(x - (cx - overlap / 2)) / overlap;
                maskLeft.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((1.0 - t) * 255);
                maskRight.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(t * 255);
            } else {
                maskRight.at<std::uint8_t>(y, x) = 255;
            }
        }
    }

    const cv::Mat blended = Alignment::MultiBandBlend(left, right, maskLeft, maskRight, 5);

    const std::string windowName = "Blending Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::Mat display;
    cv::hconcat(std::vector<cv::Mat>{left, right, blended}, display);

    cv::putText(display, "left right blended(multi-band)", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6,
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

#endif // TESTS_ALIGNMENT_BLENDINGDEMO_H
