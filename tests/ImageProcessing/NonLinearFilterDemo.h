#ifndef TESTS_IMAGEPROCESSING_NONLINEARFILTERDEMO_H
#define TESTS_IMAGEPROCESSING_NONLINEARFILTERDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "ImageProcessing/NonLinearFilter.h"
#include "ImageProcessing/NonLinearFilterConfig.h"

#include "CommonEnv.h"

// 非线性滤波交互式调参 demo（GUI，不进 ctest）。
// 按键：
//   m        切换滤波类型（median / bilateral）
//   +/-      调整核尺寸（中值）或直径（双边）
//   c/C      调整 sigmaColor（双边）
//   s/S      调整 sigmaSpace（双边）
//   r        重置参数
//   q / ESC  退出
inline int NonLinearFilterDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/NonLinearFilter/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[NonLinearFilterDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        cv::randu(src, 0, 256);
    }

    NonLinearFilterConfig cfg = NonLinearFilterConfig::LoadFromFile(NonLinearFilterConfig::DefaultPath());

    const std::string windowName = "NonLinear Filter Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    cv::Mat result;
    bool running = true;
    while (running) {
        if (cfg.filterType == "median") {
            result = ImageProcessing::MedianBlur(src, cfg.kernelSize);
        } else {
            result = ImageProcessing::BilateralFilter(src, cfg.diameter, cfg.sigmaColor, cfg.sigmaSpace);
        }

        cv::Mat display = result.empty() ? src.clone() : result.clone();
        char status[256];
        std::snprintf(status, sizeof(status), "type=%s kernel=%d diameter=%d sigmaColor=%.1f sigmaSpace=%.1f",
                      cfg.filterType.c_str(), cfg.kernelSize, cfg.diameter, cfg.sigmaColor, cfg.sigmaSpace);
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255), 1, cv::LINE_AA);
        cv::putText(display, "m=mode +/-=size c/C=sigmaColor s/S=sigmaSpace r=reset q=quit",
                    cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 'm':
            cfg.filterType = (cfg.filterType == "median") ? "bilateral" : "median";
            break;
        case '+':
        case '=':
            cfg.kernelSize += 2;
            cfg.diameter += 2;
            break;
        case '-':
            if (cfg.kernelSize > 1) {
                cfg.kernelSize -= 2;
            }
            if (cfg.diameter > 1) {
                cfg.diameter -= 2;
            }
            break;
        case 'c':
            cfg.sigmaColor = std::max(1.0, cfg.sigmaColor - 10.0);
            break;
        case 'C':
            cfg.sigmaColor += 10.0;
            break;
        case 's':
            cfg.sigmaSpace = std::max(1.0, cfg.sigmaSpace - 10.0);
            break;
        case 'S':
            cfg.sigmaSpace += 10.0;
            break;
        case 'r':
            cfg = NonLinearFilterConfig{};
            break;
        default:
            break;
        }
    }

    cfg.SaveToFile(NonLinearFilterConfig::DefaultPath());
    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_IMAGEPROCESSING_NONLINEARFILTERDEMO_H
