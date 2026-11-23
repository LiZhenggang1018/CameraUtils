#ifndef TESTS_IMAGEPROCESSING_GUIDEDFILTERDEMO_H
#define TESTS_IMAGEPROCESSING_GUIDEDFILTERDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "ImageProcessing/GuidedFilter.h"

#include "CommonEnv.h"

// 导向滤波交互式 demo（GUI，不进 ctest）。
// 展示导向滤波 vs 高斯滤波的保边去噪对比。
// 按键：
//   +/-      调整 radius（窗口半径）
//   e/E      调整 eps（正则项）
//   r        重置参数
//   q / ESC  退出
inline int GuidedFilterDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/GuidedFilter/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[GuidedFilterDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        // 合成：带噪声的纹理图（含明显边缘）
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(128));
        cv::RNG rng(7);
        for (int y = 0; y < 512; ++y) {
            for (int x = 0; x < 512; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x / 8 + y / 8) % 2 == 0 ? 60 : 200);
            }
        }
        cv::RNG nrng(9);
        cv::Mat noise(512, 512, CV_32FC1);
        nrng.fill(noise, cv::RNG::NORMAL, 0.0, 25.0);
        cv::Mat f32;
        src.convertTo(f32, CV_32F);
        cv::add(f32, noise, f32);
        f32.convertTo(src, CV_8U);
    }

    int radius = 8;
    double eps = 0.01 * 0.01 * 255.0 * 255.0; // eps 通常用 (0.01*255)^2 量级

    const std::string windowName = "Guided Filter Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    bool running = true;
    while (running) {
        const cv::Mat guided = ImageProcessing::GuidedFilter(src, radius, eps);

        cv::Mat gaussian;
        cv::GaussianBlur(src, gaussian, cv::Size(radius * 2 + 1, radius * 2 + 1), 0);

        cv::Mat origColor;
        cv::Mat guidedColor;
        cv::Mat gaussColor;
        cv::cvtColor(src, origColor, cv::COLOR_GRAY2BGR);
        cv::cvtColor(guided, guidedColor, cv::COLOR_GRAY2BGR);
        cv::cvtColor(gaussian, gaussColor, cv::COLOR_GRAY2BGR);

        cv::Mat display;
        cv::hconcat(std::vector<cv::Mat>{origColor, guidedColor, gaussColor}, display);

        char status[256];
        std::snprintf(status, sizeof(status), "radius=%d eps=%.1f  (left=orig middle=guided right=gaussian)", radius,
                      eps);
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "+/-=radius e/E=eps r=reset q=quit", cv::Point(10, display.rows - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case '+':
        case '=':
            radius = std::min(31, radius + 1);
            break;
        case '-':
            radius = std::max(1, radius - 1);
            break;
        case 'e':
            eps = std::max(0.0, eps / 2.0);
            break;
        case 'E':
            eps = eps * 2.0 + 1.0;
            break;
        case 'r':
            radius = 8;
            eps = 0.01 * 0.01 * 255.0 * 255.0;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_IMAGEPROCESSING_GUIDEDFILTERDEMO_H
