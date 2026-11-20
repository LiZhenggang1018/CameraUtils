#ifndef TESTS_IMAGEPROCESSING_SOBELTEST_H
#define TESTS_IMAGEPROCESSING_SOBELTEST_H

#include <cstdint>
#include <iostream>

#include "OpenCVCommon.h"

#include "ImageProcessing/SobelDetect.h"

#include "cli/TestAssert.h"

// 闃舵0锛堝鍖栫悊瑙ｏ級鈥斺€擲obel 绠楀瓙鐨勬暟瀛﹀畾涔夛細
//   涓€闃剁鏁ｅ井鍒嗭紝鐢ㄤ袱涓?3x3 鏍稿垎鍒繎浼?x/y 鏂瑰悜姊害锛?
//     Gx = [[-1, 0, 1], [-2, 0, 2], [-1, 0, 1]]
//     Gy = [[-1,-2,-1], [ 0, 0, 0], [ 1, 2, 1]]
//   瀵?CV_8U 杈撳叆杈撳嚭 CV_16S锛岃竟鐣岀敤 BORDER_REFLECT_101锛堜笌 cv::Sobel 榛樿涓€鑷达級銆?
//
//   鎵嬬畻鐪熷€硷紙3x3 鍨傜洿闃惰穬鍥撅紝涓績鍍忕礌 (1,1) 閭诲煙鍗虫暣鍥撅級锛?
//     src = [[0,0,255],[0,0,255],[0,0,255]]
//     Gx(1,1) = 1*255 + 2*255 + 1*255 = 1020
//     Gy(1,1) = -1*255 + 1*255 = 0

namespace {
// 鏋勯€?3x3 鍨傜洿闃惰穬鍥撅紙宸﹂粦鍙崇櫧锛夛紝鐢ㄤ簬鎵嬬畻鐪熷€兼柇瑷€
cv::Mat MakeStepImage()
{
    cv::Mat img(3, 3, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < 3; ++y) {
        img.at<std::uint8_t>(y, 2) = 255;
    }
    return img;
}

// 鏋勯€犱竴寮犵‘瀹氭€х殑灏忓浘锛堥潪闅忔満锛夛紝鐢ㄤ簬涓?cv::Sobel 浜ゅ弶楠岃瘉
cv::Mat MakePatternImage()
{
    cv::Mat img(9, 13, CV_8UC1);
    for (int y = 0; y < img.rows; ++y) {
        for (int x = 0; x < img.cols; ++x) {
            img.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
        }
    }
    return img;
}
} // namespace

inline void SobelEdgeTest()
{
    std::cout << "===== Sobel test begin =====" << std::endl;

    // 1) 鎵嬬畻鐪熷€硷細鍨傜洿闃惰穬鍥句腑蹇冨儚绱犵殑 Gx=1020, Gy=0
    {
        const cv::Mat step = MakeStepImage();
        const cv::Mat gx = Sobel::GradientX(step);
        const cv::Mat gy = Sobel::GradientY(step);

        CU_CHECK(!gx.empty());
        CU_CHECK(!gy.empty());
        CU_CHECK(gx.type() == CV_16S);
        CU_CHECK(gy.type() == CV_16S);

        CU_CHECK_EQ(gx.at<short>(1, 1), 1020);
        CU_CHECK_EQ(gy.at<short>(1, 1), 0);
        std::cout << "  hand-computed truth: Gx(1,1)=1020, Gy(1,1)=0 OK" << std::endl;
    }

    // 2) 浜ゅ弶楠岃瘉锛氭墜鍐欏疄鐜颁笌 cv::Sobel 鍦ㄦ暣寮犲浘涓婇€愬儚绱犱竴鑷?
    {
        const cv::Mat src = MakePatternImage();

        const cv::Mat gx = Sobel::GradientX(src);
        const cv::Mat gy = Sobel::GradientY(src);

        cv::Mat refGx;
        cv::Mat refGy;
        cv::Sobel(src, refGx, CV_16S, 1, 0, 3);
        cv::Sobel(src, refGy, CV_16S, 0, 1, 3);

        CU_CHECK(gx.size() == refGx.size());
        CU_CHECK(gy.size() == refGy.size());

        double maxDiffGx = 0.0;
        double maxDiffGy = 0.0;
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                maxDiffGx = std::max(maxDiffGx, std::abs(static_cast<double>(gx.at<short>(y, x)) -
                                                         static_cast<double>(refGx.at<short>(y, x))));
                maxDiffGy = std::max(maxDiffGy, std::abs(static_cast<double>(gy.at<short>(y, x)) -
                                                         static_cast<double>(refGy.at<short>(y, x))));
            }
        }

        CU_CHECK(maxDiffGx == 0.0);
        CU_CHECK(maxDiffGy == 0.0);
        std::cout << "  cross-validation vs cv::Sobel: maxDiff(Gx)=" << maxDiffGx << ", maxDiff(Gy)=" << maxDiffGy
                  << " OK" << std::endl;
    }

    std::cout << "===== Sobel test passed =====" << std::endl;
}

#endif // TESTS_IMAGEPROCESSING_SOBELTEST_H
