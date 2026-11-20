#ifndef TESTS_ALIGNMENT_PANORAMADEMO_H
#define TESTS_ALIGNMENT_PANORAMADEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Panorama.h"

#include "CommonEnv.h"

// 多图全景拼接交互式 demo（GUI，不进 ctest）。
// 从 data/Alignment/Panorama/ 读取多张横向重叠图，拼接成全景。
// 若素材缺失，用合成图（大图切片）演示。
// 按键：
//   q / ESC  退出
inline int PanoramaDemo()
{
    const std::string dir = EnvRootPath() + "/data/Alignment/Panorama/";

    std::vector<cv::Mat> images;
    for (int i = 1; i <= 6; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "%s%d.jpg", dir.c_str(), i);
        cv::Mat img = cv::imread(name, cv::IMREAD_GRAYSCALE);
        if (!img.empty()) {
            images.push_back(img);
        }
    }

    if (images.size() < 2) {
        std::printf("[PanoramaDemo] 素材不足，用合成大图切片演示\n");
        // 生成一张带结构的大图，切 4 张
        cv::Mat big(400, 1200, CV_8UC1, cv::Scalar(128));
        cv::RNG rng(42);
        for (int i = 0; i < 300; ++i) {
            const int bx = rng.uniform(0, 1200 - 30);
            const int by = rng.uniform(0, 400 - 30);
            const int bw = rng.uniform(10, 50);
            const int bh = rng.uniform(10, 50);
            const int intensity = rng.uniform(0, 256);
            cv::rectangle(big, cv::Rect(bx, by, bw, bh), cv::Scalar(intensity), -1);
        }
        const int overlap = 150;
        const int sliceW = 400;
        for (int i = 0; i < 3; ++i) {
            int x0 = i * (sliceW - overlap);
            if (x0 + sliceW > big.cols) {
                x0 = big.cols - sliceW;
            }
            images.push_back(big(cv::Rect(x0, 0, sliceW, big.rows)).clone());
        }
    }

    const cv::Mat pano = Alignment::StitchPanorama(images);

    const std::string windowName = "Panorama Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::Mat display;
    if (pano.cols > 2000) {
        const double scale = 2000.0 / pano.cols;
        cv::resize(pano, display, cv::Size(), scale, scale, cv::INTER_LINEAR);
    } else {
        display = pano.clone();
    }

    char status[256];
    std::snprintf(status, sizeof(status), "panorama: %dx%d from %zu images", pano.cols, pano.rows, images.size());
    cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                cv::LINE_AA);
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

#endif // TESTS_ALIGNMENT_PANORAMADEMO_H
