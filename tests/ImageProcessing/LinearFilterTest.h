#ifndef TESTS_IMAGEPROCESSING_LINEARFILTERTEST_H
#define TESTS_IMAGEPROCESSING_LINEARFILTERTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/LinearFilter.h"

#include "cli/TestAssert.h"

// 绾挎€ф护娉㈡ā鍧楁柇瑷€娴嬭瘯銆?
// 鐪熷€兼潵婧愶細甯告暟鍥句笉鍙樻€э紙婊ゆ尝瀵瑰父鏁板尯杈撳嚭浠嶄负甯告暟锛? 涓?OpenCV 鍙傝€冨疄鐜颁氦鍙夐獙璇併€?

namespace {

cv::Mat MakeConstantImage(int rows, int cols, std::uint8_t value)
{
    return cv::Mat(rows, cols, CV_8UC1, cv::Scalar(value));
}

// 3x3 鍨傜洿闃惰穬鍥撅細宸︿袱鍒?0锛屽彸涓€鍒?255
cv::Mat MakeVerticalStep()
{
    cv::Mat img(5, 5, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < img.rows; ++y) {
        img.at<std::uint8_t>(y, 3) = 255;
        img.at<std::uint8_t>(y, 4) = 255;
    }
    return img;
}

} // namespace

inline void LinearFilterTest()
{
    std::cout << "===== LinearFilter test begin =====" << std::endl;

    // 1) 甯告暟鍥句笉鍙樻€э細楂樻柉/鍧囧€?閿愬寲瀵瑰父鏁板浘杈撳嚭浠嶄负甯告暟
    {
        const cv::Mat zeros = MakeConstantImage(9, 9, 0);
        const cv::Mat full = MakeConstantImage(9, 9, 255);

        CU_CHECK_EQ(cv::countNonZero(ImageProcessing::GaussianBlur(zeros, 3, 1.0)), 0);
        CU_CHECK_EQ(cv::countNonZero(ImageProcessing::MeanBlur(zeros, 3)), 0);

        const cv::Mat sharpFull = ImageProcessing::Sharpen(full, 1.0);
        double minVal = 0.0;
        double maxVal = 0.0;
        cv::minMaxLoc(sharpFull, &minVal, &maxVal);
        CU_CHECK(minVal >= 250.0);
        CU_CHECK(maxVal <= 255.0);
        std::cout << "  constant-image invariance OK" << std::endl;
    }

    // 2) SobelX 瀵瑰瀭鐩撮樁璺冨浘涓績鍝嶅簲涓烘锛堟柟鍚戠湡鍊硷級
    {
        const cv::Mat step = MakeVerticalStep();
        const cv::Mat gx = ImageProcessing::SobelX(step);
        CU_CHECK(gx.type() == CV_16S);
        CU_CHECK(gx.at<short>(2, 2) > 0);
        std::cout << "  SobelX direction truth OK" << std::endl;
    }

    // 3) 浜ゅ弶楠岃瘉锛欸aussianBlur / MeanBlur 涓?cv::GaussianBlur / cv::blur 閫愬儚绱犱竴鑷?
    {
        cv::Mat src(21, 21, CV_8UC1);
        cv::randu(src, 0, 256);

        const cv::Mat g = ImageProcessing::GaussianBlur(src, 3, 1.0);
        cv::Mat refG;
        cv::GaussianBlur(src, refG, cv::Size(3, 3), 1.0);
        CU_CHECK_EQ(cv::countNonZero(g != refG), 0);

        const cv::Mat m = ImageProcessing::MeanBlur(src, 3);
        cv::Mat refM;
        cv::blur(src, refM, cv::Size(3, 3));
        CU_CHECK_EQ(cv::countNonZero(m != refM), 0);
        std::cout << "  cross-validation vs OpenCV OK" << std::endl;
    }

    // 4) 绌鸿緭鍏ュ畨鍏ㄨ繑鍥炵┖ Mat
    {
        const cv::Mat empty;
        CU_CHECK(ImageProcessing::GaussianBlur(empty, 3, 1.0).empty());
        CU_CHECK(ImageProcessing::MeanBlur(empty, 3).empty());
        CU_CHECK(ImageProcessing::Sharpen(empty, 1.0).empty());
        CU_CHECK(ImageProcessing::SobelX(empty).empty());
        CU_CHECK(ImageProcessing::SobelY(empty).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== LinearFilter test passed =====" << std::endl;
}

#endif // TESTS_IMAGEPROCESSING_LINEARFILTERTEST_H
