#ifndef TESTS_ALIGNMENT_VIGNETTINGDEMO_H
#define TESTS_ALIGNMENT_VIGNETTINGDEMO_H

#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "Alignment/Vignetting.h"

#include "CommonEnv.h"

// 渐晕校正交互式 demo（GUI，不进 ctest）。
// 展示带暗角的图与校正结果对比。
// 按键：
//   q / ESC  退出
inline int VignettingDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/Alignment/Vignetting/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[VignettingDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(200));
    }

    // 构造带渐晕的图（径向衰减）
    cv::Mat vignetted;
    {
        const double strength = 0.5;
        const int rows = src.rows;
        const int cols = src.cols;
        const double cx = (cols - 1) / 2.0;
        const double cy = (rows - 1) / 2.0;
        const double maxR2 = cx * cx + cy * cy;
        cv::Mat imgF;
        src.convertTo(imgF, CV_32F);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const double dx = x - cx;
                const double dy = y - cy;
                const double r2 = (dx * dx + dy * dy) / maxR2;
                imgF.at<float>(y, x) *= static_cast<float>(1.0 - strength * r2);
            }
        }
        imgF.convertTo(vignetted, CV_8U);
    }

    const cv::Mat corrected = Alignment::CorrectVignetting(vignetted, 0.5);

    const std::string windowName = "Vignetting Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::Mat display;
    cv::hconcat(std::vector<cv::Mat>{vignetted, corrected}, display);

    cv::putText(display, "left=vignetted right=corrected", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6,
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

#endif // TESTS_ALIGNMENT_VIGNETTINGDEMO_H
