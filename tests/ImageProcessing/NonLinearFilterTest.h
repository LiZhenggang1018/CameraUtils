#ifndef TESTS_IMAGEPROCESSING_NONLINEARFILTERTEST_H
#define TESTS_IMAGEPROCESSING_NONLINEARFILTERTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/NonLinearFilter.h"

#include "cli/TestAssert.h"

// 闈炵嚎鎬ф护娉㈡ā鍧楁柇瑷€娴嬭瘯銆?
// 鐪熷€兼潵婧愶細涓€兼护娉㈠幓妞掔洂鍣０銆佸弻杈规护娉繚杈?+ 涓?OpenCV 鍙傝€冨疄鐜颁氦鍙夐獙璇併€?

namespace {

// 3x3 涓績涓?255銆佸懆鍥翠负 0 鐨勬鐩愬櫔澹扮偣
cv::Mat MakeSaltNoise()
{
    cv::Mat img = cv::Mat::zeros(5, 5, CV_8UC1);
    img.at<std::uint8_t>(2, 2) = 255;
    return img;
}

// 姘村钩闃惰穬鍥撅細涓婂崐 0銆佷笅鍗?255锛堝己杈圭紭锛?
cv::Mat MakeHorizontalStep()
{
    cv::Mat img = cv::Mat::zeros(9, 9, CV_8UC1);
    for (int y = 4; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            img.at<std::uint8_t>(y, x) = 255;
        }
    }
    return img;
}

} // namespace

inline void NonLinearFilterTest()
{
    std::cout << "===== NonLinearFilter test begin =====" << std::endl;

    // 1) 涓€兼护娉㈠幓闄ゆ鐩愬櫔澹帮細涓績鍣０鐐硅鍛ㄥ洿 0 鍙栦唬
    {
        const cv::Mat noise = MakeSaltNoise();
        const cv::Mat out = ImageProcessing::MedianBlur(noise, 3);
        CU_CHECK(out.at<std::uint8_t>(2, 2) == 0);
        std::cout << "  median removes salt noise OK" << std::endl;
    }

    // 2) 鍙岃竟婊ゆ尝淇濊竟锛氶樁璺冭竟缂樹袱渚у悇鑷繚鎸佹帴杩戝師鍊?
    {
        const cv::Mat step = MakeHorizontalStep();
        const cv::Mat out = ImageProcessing::BilateralFilter(step, 9, 50.0, 50.0);
        const std::uint8_t darkSide = out.at<std::uint8_t>(2, 4);
        const std::uint8_t brightSide = out.at<std::uint8_t>(6, 4);
        CU_CHECK(darkSide < 30);
        CU_CHECK(brightSide > 225);
        std::cout << "  bilateral preserves edge OK" << std::endl;
    }

    // 3) 浜ゅ弶楠岃瘉锛氫笌 cv::medianBlur / cv::bilateralFilter 閫愬儚绱犱竴鑷?
    {
        cv::Mat src(21, 21, CV_8UC1);
        cv::randu(src, 0, 256);

        const cv::Mat m = ImageProcessing::MedianBlur(src, 3);
        cv::Mat refM;
        cv::medianBlur(src, refM, 3);
        CU_CHECK_EQ(cv::countNonZero(m != refM), 0);

        const cv::Mat b = ImageProcessing::BilateralFilter(src, 9, 50.0, 50.0);
        cv::Mat refB;
        cv::bilateralFilter(src, refB, 9, 50.0, 50.0);
        CU_CHECK_EQ(cv::countNonZero(b != refB), 0);
        std::cout << "  cross-validation vs OpenCV OK" << std::endl;
    }

    // 4) 绌鸿緭鍏ュ畨鍏ㄨ繑鍥炵┖ Mat
    {
        const cv::Mat empty;
        CU_CHECK(ImageProcessing::MedianBlur(empty, 3).empty());
        CU_CHECK(ImageProcessing::BilateralFilter(empty, 9, 50.0, 50.0).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    // 5) 鍋舵暟/闈炴硶 diameter 瑙勬暣涓哄鏁帮紝涓嶅穿婧?
    {
        cv::Mat src(21, 21, CV_8UC1);
        cv::randu(src, 0, 256);
        const cv::Mat out = ImageProcessing::BilateralFilter(src, 10, 50.0, 50.0);
        CU_CHECK(!out.empty());
        CU_CHECK(out.size() == src.size());
        std::cout << "  even diameter normalization OK" << std::endl;
    }

    std::cout << "===== NonLinearFilter test passed =====" << std::endl;
}

#endif // TESTS_IMAGEPROCESSING_NONLINEARFILTERTEST_H
