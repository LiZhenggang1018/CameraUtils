#ifndef TESTS_IMAGEPROCESSING_FOURIERDEMO_H
#define TESTS_IMAGEPROCESSING_FOURIERDEMO_H

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "OpenCVCommon.h"

#include "CommonEnv.h"

// 鍌呴噷鍙跺彉鎹㈢畝鍗曞睍绀猴紙GUI锛屼笉杩?ctest锛夈€?
// 灞曠ず骞呭害璋?+ 楂樹綆棰戞帺鐮佹护娉㈡晥鏋滐紙浣庨€氫繚鐣欒疆寤撱€侀珮閫氫繚鐣欒竟缂橈級銆?
// 鎸夐敭锛?
//   s        鏄剧ず骞呭害璋憋紙瀵规暟褰掍竴鍖栵級
//   l        浣庨€氭护娉㈢粨鏋?
//   h        楂橀€氭护娉㈢粨鏋?
//   o        鍘熷鍥?
//   q / ESC  閫€鍑?
inline int FourierDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/ImageProcessing/Fourier/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[FourierDemo] %s not found, generating synthetic image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1, cv::Scalar(0));
        cv::randu(src, 0, 256);
    }

    // 鎵╁埌鏈€浼樺昂瀵?+ 杞诞鐐癸紝鍋?DFT
    const int optimalRows = cv::getOptimalDFTSize(src.rows);
    const int optimalCols = cv::getOptimalDFTSize(src.cols);
    cv::Mat padded;
    cv::copyMakeBorder(src, padded, 0, optimalRows - src.rows, 0, optimalCols - src.cols, cv::BORDER_CONSTANT,
                       cv::Scalar(0));

    cv::Mat planes[] = {cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexImg;
    cv::merge(planes, 2, complexImg);
    cv::dft(complexImg, complexImg);

    // 骞呭害璋憋紙瀵规暟 + 褰掍竴鍖栵級锛屽苟鍋?fftshift 璁╀綆棰戝眳涓究浜庤瀵?
    cv::split(complexImg, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat magnitude = planes[0];
    magnitude += cv::Scalar::all(1);
    cv::log(magnitude, magnitude);
    magnitude = magnitude(cv::Rect(0, 0, magnitude.cols & -2, magnitude.rows & -2));
    cv::normalize(magnitude, magnitude, 0, 1, cv::NORM_MINMAX);

    // 鐩存帴鍦ㄥ師濮嬶紙鏈?shift锛夐璋变笂鏋勯€?浣庨鍦ㄥ洓瑙?鐨勬帺鐮侊紝閬垮厤棰濆鐨勯璋辨惉绉?
    const int radius = std::min(complexImg.cols, complexImg.rows) / 8;

    // 浣庨€氭帺鐮侊細dft 杈撳嚭浣庨鍦ㄥ洓瑙掞紝鏋勯€犲洓瑙掍繚鐣欑殑鎺╃爜銆?
    // 鐢ㄥ懆鏈熸€ц窛绂伙紙鍒?0 鎴栧昂瀵歌竟鐣岀殑杈冨皬鑰咃級搴﹂噺鍒?DC 鐨勮窛绂伙紝鍋舵暟/濂囨暟灏哄閮藉绉般€?
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
    // 楂橀€氭帺鐮?= 1 - 浣庨€氾紝涓旀樉寮忓墧闄?DC 鍒嗛噺锛堝師鐐癸級
    cv::Mat highPassMask = cv::Scalar::all(1) - lowPassMask;
    highPassMask.at<std::uint8_t>(0, 0) = 0;

    // 鐢ㄦ帺鐮佹护娉紙閫愬儚绱犵疆闆讹級
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
