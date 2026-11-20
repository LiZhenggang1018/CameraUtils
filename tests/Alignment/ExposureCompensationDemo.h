#ifndef TESTS_ALIGNMENT_EXPOSURECOMPENSATIONDEMO_H
#define TESTS_ALIGNMENT_EXPOSURECOMPENSATIONDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/ExposureCompensation.h"

#include "CommonEnv.h"

// 曝光补偿交互式 demo（GUI，不进 ctest）。
// 展示多张不同曝光的图，增益估计 + 补偿后的亮度对齐效果。
// 按键：
//   q / ESC  退出
inline int ExposureCompensationDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/Alignment/ExposureCompensation/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[ExposureCompensationDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
            }
        }
    }

    // 三张不同曝光的图
    std::vector<cv::Mat> images;
    cv::Mat img1;
    src.convertTo(img1, -1, 0.5, 0);
    cv::Mat img2 = src.clone();
    cv::Mat img3;
    src.convertTo(img3, -1, 1.5, 0);
    images = {img1, img2, img3};

    const auto gains = Alignment::EstimateGains(images);

    // 补偿后对齐
    std::vector<cv::Mat> compensated;
    for (std::size_t i = 0; i < images.size(); ++i) {
        compensated.push_back(Alignment::ApplyGain(images[i], gains[i]));
    }

    const std::string windowName = "Exposure Compensation Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::Mat display;
    cv::hconcat(std::vector<cv::Mat>{images[0], images[1], images[2]}, display);
    cv::Mat displayComp;
    cv::hconcat(std::vector<cv::Mat>{compensated[0], compensated[1], compensated[2]}, displayComp);

    cv::Mat combined;
    cv::vconcat(std::vector<cv::Mat>{display, displayComp}, combined);

    char status[256];
    std::snprintf(status, sizeof(status), "gains: %.2f %.2f %.2f  (top=original bottom=compensated)", gains[0],
                  gains[1], gains[2]);
    cv::putText(combined, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                cv::LINE_AA);
    cv::putText(combined, "q=quit", cv::Point(10, combined.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200),
                1, cv::LINE_AA);

    cv::imshow(windowName, combined);
    int key = 0;
    while (key != 27 && key != 'q') {
        key = cv::waitKey(30);
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_ALIGNMENT_EXPOSURECOMPENSATIONDEMO_H
