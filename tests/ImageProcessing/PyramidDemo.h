#ifndef TESTS_IMAGEPROCESSING_PYRAMIDDEMO_H
#define TESTS_IMAGEPROCESSING_PYRAMIDDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "ImageProcessing/Pyramid.h"

#include "CommonEnv.h"

// 金字塔交互式 demo（GUI，不进 ctest）。
// 展示高斯金字塔各层（resize 到同一高度平铺），按键切换显示高斯/拉普拉斯。
// 按键：
//   g        显示高斯金字塔
//   l        显示拉普拉斯金字塔（各层残差归一化显示）
//   q / ESC  退出
inline int PyramidDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/Pyramid/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[PyramidDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        cv::randu(src, 0, 256);
    }

    const int levels = 4;
    const auto gaussian = ImageProcessing::BuildGaussianPyramid(src, levels);
    const auto laplacian = ImageProcessing::BuildLaplacianPyramid(src, levels);

    const std::string windowName = "Pyramid Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    bool showLaplacian = false;
    bool running = true;
    while (running) {
        // 各层 resize 到统一高度 256，横向平铺
        const int tileHeight = 256;
        std::vector<cv::Mat> tiles;
        int totalWidth = 0;
        for (const auto& layer : showLaplacian ? laplacian : gaussian) {
            cv::Mat norm;
            if (showLaplacian) {
                cv::normalize(layer, norm, 0, 255, cv::NORM_MINMAX, CV_8U);
            } else {
                norm = layer;
            }
            cv::Mat resized;
            const double scale = static_cast<double>(tileHeight) / norm.rows;
            cv::resize(norm, resized, cv::Size(), scale, scale, cv::INTER_NEAREST);
            tiles.push_back(resized);
            totalWidth += resized.cols;
        }

        cv::Mat canvas(tileHeight, totalWidth, CV_8UC1, cv::Scalar(0));
        int offset = 0;
        for (const auto& tile : tiles) {
            tile.copyTo(canvas(cv::Rect(offset, 0, tile.cols, tile.rows)));
            offset += tile.cols;
        }

        const char* label = showLaplacian ? "Laplacian" : "Gaussian";
        cv::putText(canvas, label, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255), 2, cv::LINE_AA);
        cv::putText(canvas, "g=gaussian l=laplacian q=quit", cv::Point(10, canvas.rows - 10), cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, canvas);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 'g':
            showLaplacian = false;
            break;
        case 'l':
            showLaplacian = true;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_IMAGEPROCESSING_PYRAMIDDEMO_H
