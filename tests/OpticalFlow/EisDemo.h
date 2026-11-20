#ifndef TESTS_OPTICALFLOW_EISDEMO_H
#define TESTS_OPTICALFLOW_EISDEMO_H

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "Eis.h"

#include "CommonEnv.h"

// EIS 电子防抖交互式 demo（GUI，不进 ctest）。
// 演示一段带抖动的合成视频的全局运动估计与稳定化对比。
// 按键：
//   q / ESC  退出
inline int EisDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/OpticalFlow/Eis/input.mp4";

    cv::VideoCapture cap(inputPath);
    std::vector<cv::Mat> frames;
    if (cap.isOpened()) {
        cv::Mat frame;
        while (frames.size() < 60 && cap.read(frame)) {
            cv::Mat gray;
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            frames.push_back(gray);
        }
    }

    if (frames.size() < 2) {
        std::printf("[EisDemo] 素材不足，生成带抖动的合成帧序列\n");
        cv::Mat base(360, 480, CV_8UC1, cv::Scalar(128));
        cv::RNG rng(31);
        for (int i = 0; i < 200; ++i) {
            const int bx = rng.uniform(0, 450);
            const int by = rng.uniform(0, 330);
            const int bw = rng.uniform(8, 40);
            const int bh = rng.uniform(8, 40);
            cv::rectangle(base, cv::Rect(bx, by, bw, bh), cv::Scalar(rng.uniform(0, 256)), -1);
        }
        // 加正弦抖动
        for (int i = 0; i < 30; ++i) {
            const int dx = static_cast<int>(10.0 * std::sin(i * 0.5));
            const int dy = static_cast<int>(6.0 * std::cos(i * 0.4));
            cv::Mat m = cv::Mat::eye(2, 3, CV_64F);
            m.at<double>(0, 2) = dx;
            m.at<double>(1, 2) = dy;
            cv::Mat shifted;
            cv::warpAffine(base, shifted, m, base.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            frames.push_back(shifted);
        }
    }

    // 全局运动估计（帧间）
    std::vector<cv::Mat> motions;
    motions.push_back(cv::Mat::eye(2, 3, CV_64F));
    for (std::size_t i = 1; i < frames.size(); ++i) {
        motions.push_back(OpticalFlow::EstimateGlobalMotionOptical(frames[i - 1], frames[i]));
    }

    const std::string windowName = "EIS Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    bool running = true;
    std::size_t idx = 0;
    while (running) {
        cv::Mat display;
        cv::cvtColor(frames[idx], display, cv::COLOR_GRAY2BGR);

        char status[256];
        std::snprintf(status, sizeof(status), "frame %zu/%zu", idx + 1, frames.size());
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "q=quit (此 demo 展示抖动帧，稳定化见 EisTest)", cv::Point(10, display.rows - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(50);
        if (key == 27 || key == 'q') {
            running = false;
        }
        idx = (idx + 1) % frames.size();
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_OPTICALFLOW_EISDEMO_H
