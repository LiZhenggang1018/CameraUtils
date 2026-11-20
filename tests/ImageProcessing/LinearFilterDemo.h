#ifndef TESTS_IMAGEPROCESSING_LINEARFILTERDEMO_H
#define TESTS_IMAGEPROCESSING_LINEARFILTERDEMO_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "ImageProcessing/LinearFilter.h"
#include "ImageProcessing/LinearFilterConfig.h"

#include "CommonEnv.h"

// 绾挎€ф护娉氦浜掑紡璋冨弬 demo锛圙UI锛屼笉杩?ctest锛夈€?
// 鎸夐敭锛?
//   m        鍒囨崲婊ゆ尝绫诲瀷锛坓aussian / mean / sharpen / sobel锛?
//   +/-      璋冩暣鏍稿昂瀵革紙妯＄硦绫伙級
//   s/S      璋冩暣 sigma锛堥珮鏂級
//   a/A      璋冩暣閿愬寲寮哄害锛堥攼鍖栵級
//   r        閲嶇疆鍙傛暟
//   q / ESC  閫€鍑?
inline int LinearFilterDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/LinearFilter/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[LinearFilterDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        cv::randu(src, 0, 256);
    }

    LinearFilterConfig cfg = LinearFilterConfig::LoadFromFile(LinearFilterConfig::DefaultPath());

    const std::string windowName = "Linear Filter Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    const std::vector<std::string> filterTypes = {"gaussian", "mean", "sharpen", "sobel"};

    cv::Mat result;
    bool running = true;
    while (running) {
        if (cfg.filterType == "gaussian") {
            result = ImageProcessing::GaussianBlur(src, cfg.kernelSize, cfg.sigma);
        } else if (cfg.filterType == "mean") {
            result = ImageProcessing::MeanBlur(src, cfg.kernelSize);
        } else if (cfg.filterType == "sharpen") {
            result = ImageProcessing::Sharpen(src, cfg.sharpenAmount);
        } else if (cfg.filterType == "sobel") {
            cv::Mat gx = ImageProcessing::SobelX(src);
            cv::Mat gy = ImageProcessing::SobelY(src);
            cv::Mat mag;
            cv::magnitude(gx, gy, mag);
            cv::convertScaleAbs(mag, result);
        } else {
            result = src.clone();
        }

        cv::Mat display = result.empty() ? src.clone() : result.clone();
        char status[256];
        std::snprintf(status, sizeof(status), "type=%s kernel=%d sigma=%.1f sharp=%.1f", cfg.filterType.c_str(),
                      cfg.kernelSize, cfg.sigma, cfg.sharpenAmount);
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255), 1, cv::LINE_AA);
        cv::putText(display, "m=mode +/-=kernel s/S=sigma a/A=sharp r=reset q=quit", cv::Point(10, display.rows - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 'm': {
            auto it = std::find(filterTypes.begin(), filterTypes.end(), cfg.filterType);
            int idx = (it == filterTypes.end()) ? 0 : static_cast<int>(std::distance(filterTypes.begin(), it));
            cfg.filterType = filterTypes[(idx + 1) % filterTypes.size()];
            break;
        }
        case '+':
        case '=':
            cfg.kernelSize += 2;
            break;
        case '-':
            if (cfg.kernelSize > 1) {
                cfg.kernelSize -= 2;
            }
            break;
        case 's':
            cfg.sigma = std::max(0.1, cfg.sigma - 0.5);
            break;
        case 'S':
            cfg.sigma += 0.5;
            break;
        case 'a':
            cfg.sharpenAmount = std::max(0.0, cfg.sharpenAmount - 0.5);
            break;
        case 'A':
            cfg.sharpenAmount += 0.5;
            break;
        case 'r':
            cfg = LinearFilterConfig{};
            break;
        default:
            break;
        }
    }

    cfg.SaveToFile(LinearFilterConfig::DefaultPath());
    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_IMAGEPROCESSING_LINEARFILTERDEMO_H
