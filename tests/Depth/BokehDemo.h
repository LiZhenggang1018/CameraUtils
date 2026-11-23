#ifndef TESTS_DEPTH_BOKEHDEMO_H
#define TESTS_DEPTH_BOKEHDEMO_H

#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "Depth/Bokeh.h"

#include "CommonEnv.h"

// 深度图 + 虚化交互式 demo（GUI，不进 ctest）。
// 展示原图 + 深度图 + 虚化结果。
// 按键：
//   q / ESC  退出
inline int BokehDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/Depth/Bokeh/input.jpg";

    cv::Mat image = cv::imread(inputPath);
    if (image.empty()) {
        std::printf("[BokehDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        image = cv::Mat(360, 480, CV_8UC3);
        cv::RNG rng(37);
        for (int y = 0; y < image.rows; ++y) {
            for (int x = 0; x < image.cols; ++x) {
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
            }
        }
    }

    // 合成深度图：中心圆形前景（近），周围背景（远）
    cv::Mat depth(image.size(), CV_64F, cv::Scalar(5.0));
    cv::circle(depth, cv::Point(image.cols / 2, image.rows / 2), image.rows / 3, cv::Scalar(1.0), -1);

    const cv::Mat bokeh = Depth::ApplyBokeh(image, depth, 1.0, 2.0);

    const std::string windowName = "Bokeh Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    // 原图 + 虚化结果并排
    cv::Mat display;
    cv::hconcat(std::vector<cv::Mat>{image, bokeh}, display);

    cv::putText(display, "left=original right=bokeh(focus on center)", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6,
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

#endif // TESTS_DEPTH_BOKEHDEMO_H
