#ifndef TESTS_IMAGEPROCESSING_FOURIERDEMO_H
#define TESTS_IMAGEPROCESSING_FOURIERDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "CommonEnv.h"

// 傅里叶变换简单展示（GUI，不进 ctest）。
// 展示幅度谱 + 高低频掩码滤波效果（低通保留轮廓、高通保留边缘）。
// 按键：
//   s        显示幅度谱（对数归一化）
//   l        低通滤波结果
//   h        高通滤波结果
//   o        原始图
//   q / ESC  退出
inline int FourierDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/Fourier/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[FourierDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        cv::randu(src, 0, 256);
    }

    // 扩到最优尺寸 + 转浮点，做 DFT
    const int optimalRows = cv::getOptimalDFTSize(src.rows);
    const int optimalCols = cv::getOptimalDFTSize(src.cols);
    cv::Mat padded;
    cv::copyMakeBorder(src, padded, 0, optimalRows - src.rows, 0, optimalCols - src.cols, cv::BORDER_CONSTANT,
                       cv::Scalar(0));

    cv::Mat planes[] = {cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexImg;
    cv::merge(planes, 2, complexImg);
    cv::dft(complexImg, complexImg);

    // 幅度谱（对数 + 归一化），并做 fftshift 让低频居中便于观察
    cv::split(complexImg, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat magnitude = planes[0];
    magnitude += cv::Scalar::all(1);
    cv::log(magnitude, magnitude);
    magnitude = magnitude(cv::Rect(0, 0, magnitude.cols & -2, magnitude.rows & -2));
    cv::normalize(magnitude, magnitude, 0, 1, cv::NORM_MINMAX);

    // 直接在原始（未 shift）频谱上构造"低频在四角"的掩码，避免额外的频谱搬移
    const int radius = std::min(complexImg.cols, complexImg.rows) / 8;

    // 低通掩码：dft 输出低频在四角，构造四角保留的掩码。
    // 用周期性距离（到 0 或尺寸边界的较小者）度量到 DC 的距离，偶数/奇数尺寸都对称。
    cv::Mat lowPassMask = cv::Mat::zeros(complexImg.size(), CV_8U);
    for (int y = 0; y < complexImg.rows; ++y) {
        for (int x = 0; x < complexImg.cols; ++x) {
            const int dx = std::min(x, complexImg.cols - x);
            const int dy = std::min(y, complexImg.rows - y);
            const int cornerDist = std::max(dx, dy);
            if (cornerDist <= radius) {
                lowPassMask.at<std::uint8_t>(y, x) = 1;
            }
        }
    }
    // 高通掩码 = 1 - 低通，且显式剔除 DC 分量（原点）
    cv::Mat highPassMask = cv::Scalar::all(1) - lowPassMask;
    highPassMask.at<std::uint8_t>(0, 0) = 0;

    // 用掩码滤波（逐像素置零）
    cv::Mat complexLow = complexImg.clone();
    cv::Mat complexHigh = complexImg.clone();
    for (int y = 0; y < complexImg.rows; ++y) {
        for (int x = 0; x < complexImg.cols; ++x) {
            if (lowPassMask.at<std::uint8_t>(y, x) == 0) {
                complexLow.at<cv::Vec2f>(y, x) = cv::Vec2f(0, 0);
            }
            if (highPassMask.at<std::uint8_t>(y, x) == 0) {
                complexHigh.at<cv::Vec2f>(y, x) = cv::Vec2f(0, 0);
            }
        }
    }

    cv::Mat lowPassImg;
    cv::Mat highPassImg;
    cv::idft(complexLow, lowPassImg, cv::DFT_SCALE | cv::DFT_REAL_OUTPUT);
    cv::idft(complexHigh, highPassImg, cv::DFT_SCALE | cv::DFT_REAL_OUTPUT);
    lowPassImg = lowPassImg(cv::Rect(0, 0, src.cols, src.rows));
    highPassImg = highPassImg(cv::Rect(0, 0, src.cols, src.rows));
    cv::normalize(lowPassImg, lowPassImg, 0, 255, cv::NORM_MINMAX, CV_8U);
    cv::normalize(highPassImg, highPassImg, 0, 255, cv::NORM_MINMAX, CV_8U);

    const std::string windowName = "Fourier Demo";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    enum class View { Original, Magnitude, LowPass, HighPass };
    View view = View::Magnitude;
    bool running = true;
    while (running) {
        cv::Mat display;
        switch (view) {
        case View::Original:
            display = src;
            break;
        case View::Magnitude:
            display = magnitude;
            break;
        case View::LowPass:
            display = lowPassImg;
            break;
        case View::HighPass:
            display = highPassImg;
            break;
        }
        cv::putText(display, "s=spectrum l=lowpass h=highpass o=original q=quit", cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255), 1, cv::LINE_AA);
        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 's':
            view = View::Magnitude;
            break;
        case 'l':
            view = View::LowPass;
            break;
        case 'h':
            view = View::HighPass;
            break;
        case 'o':
            view = View::Original;
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_IMAGEPROCESSING_FOURIERDEMO_H
