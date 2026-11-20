#ifndef TESTS_IMAGEPROCESSING_PYRAMIDDEMO_H
#define TESTS_IMAGEPROCESSING_PYRAMIDDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "ImageProcessing/Pyramid.h"

#include "CommonEnv.h"

// 閲戝瓧濉斾氦浜掑紡 demo锛圙UI锛屼笉杩?ctest锛夈€?
// 灞曠ず楂樻柉閲戝瓧濉斿悇灞傦紙resize 鍒板悓涓€楂樺害骞抽摵锛夛紝鎸夐敭鍒囨崲鏄剧ず楂樻柉/鎷夋櫘鎷夋柉銆?
// 鎸夐敭锛?
//   g        鏄剧ず楂樻柉閲戝瓧濉?
//   l        鏄剧ず鎷夋櫘鎷夋柉閲戝瓧濉旓紙鍚勫眰娈嬪樊褰掍竴鍖栨樉绀猴級
//   q / ESC  閫€鍑?
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
        // 鍚勫眰 resize 鍒扮粺涓€楂樺害 256锛屾í鍚戝钩閾?
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
