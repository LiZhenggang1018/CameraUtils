#ifndef TESTS_COMPUTATIONALPHOTOGRAPHY_BEAUTYDEMO_H
#define TESTS_COMPUTATIONALPHOTOGRAPHY_BEAUTYDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "ComputationalPhotography/BeautyFilter.h"

#include "CommonEnv.h"

// 人像美颜/磨皮交互式 demo（GUI，不进 ctest）。
// 展示磨皮前后对比，可调磨皮强度与保边参数。
// 按键：
//   +/-      调整 detailGain（细节层增益，越小磨皮越强）
//   r/R      调整 radius（导向滤波窗口半径）
//   e/E      调整 eps（导向滤波正则项）
//   q / ESC  退出
inline int BeautyDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ComputationalPhotography/Beauty/input.jpg";

    cv::Mat src = cv::imread(inputPath);
    if (src.empty()) {
        std::printf("[BeautyDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        // 合成：平滑渐变的"皮肤" + 高频"瑕疵"（模拟毛孔/斑点）
        src = cv::Mat(512, 512, CV_8UC3);
        cv::RNG rng(13);
        for (int y = 0; y < 512; ++y) {
            for (int x = 0; x < 512; ++x) {
                const int base = static_cast<int>(150 + 60 * std::sin(x * 0.05) + 40 * std::cos(y * 0.04));
                // 加高频瑕疵
                const int speckle = (rng.uniform(0, 100) < 8) ? rng.uniform(-60, 60) : 0;
                const int b = std::min(255, std::max(0, base + speckle));
                const int g = std::min(255, std::max(0, base - 20 + speckle / 2));
                const int r = std::min(255, std::max(0, base + 20 + speckle / 2));
                src.at<cv::Vec3b>(y, x) =
                    cv::Vec3b(static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(r));
            }
        }
    }

    int radius = 8;
    double eps = 0.01 * 0.01 * 255.0 * 255.0;
    double detailGain = 0.3;

    const std::string windowName = "Beauty/Skin Smoothing Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    bool running = true;
    while (running) {
        const cv::Mat result = ComputationalPhotography::BeautySkin(src, radius, eps, detailGain);

        cv::Mat display;
        cv::hconcat(std::vector<cv::Mat>{src, result}, display);

        char status[256];
        std::snprintf(status, sizeof(status), "detailGain=%.2f radius=%d eps=%.1f  (left=orig right=beauty)",
                      detailGain, radius, eps);
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "+/-=detailGain r/R=radius e/E=eps q=quit", cv::Point(10, display.rows - 10),
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
            detailGain = std::min(1.0, detailGain + 0.1);
            break;
        case '-':
            detailGain = std::max(0.0, detailGain - 0.1);
            break;
        case 'r':
            radius = std::max(1, radius - 1);
            break;
        case 'R':
            radius = std::min(31, radius + 1);
            break;
        case 'e':
            eps = std::max(0.0, eps / 2.0);
            break;
        case 'E':
            eps = eps * 2.0 + 1.0;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_COMPUTATIONALPHOTOGRAPHY_BEAUTYDEMO_H
