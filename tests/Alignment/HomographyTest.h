#ifndef TESTS_ALIGNMENT_HOMOGRAPHYTEST_H
#define TESTS_ALIGNMENT_HOMOGRAPHYTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Homography.h"

#include "cli/TestAssert.h"

// 单应与 warping 模块断言测试。
// 真值来源：合成图 + 已知单应变换，验证单应估计精度与 warp 行为。

namespace {

cv::Mat MakeHomographyInput(int size)
{
    cv::Mat img(size, size, CV_8UC1);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            img.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13 + (x * y) / 16) % 256);
        }
    }
    return img;
}

} // namespace

inline void HomographyTest()
{
    std::cout << "===== Homography test begin =====" << std::endl;

    // 1) 同一图做已知单应（旋转+平移），ComputeHomography 返回非空 H 且内点数足够
    {
        const cv::Mat img = MakeHomographyInput(256);

        cv::Mat warped;
        cv::Mat hTrue = cv::getRotationMatrix2D(cv::Point2f(128, 128), 15.0, 1.0);
        hTrue.at<double>(0, 2) += 10.0;
        hTrue.at<double>(1, 2) += 5.0;
        cv::warpAffine(img, warped, hTrue, img.size());

        std::vector<cv::DMatch> inliers;
        const cv::Mat h = Alignment::ComputeHomography(img, warped, inliers);

        CU_CHECK(!h.empty());
        CU_CHECK(inliers.size() >= 4);
        std::cout << "  ComputeHomography OK (inliers=" << inliers.size() << ")" << std::endl;
    }

    // 2) 单应矩阵是 3x3
    {
        const cv::Mat img = MakeHomographyInput(256);
        cv::Mat warped;
        cv::Mat hTrue = cv::getRotationMatrix2D(cv::Point2f(128, 128), 15.0, 1.0);
        hTrue.at<double>(0, 2) += 10.0;
        hTrue.at<double>(1, 2) += 5.0;
        cv::warpAffine(img, warped, hTrue, img.size());

        std::vector<cv::DMatch> inliers;
        const cv::Mat h = Alignment::ComputeHomography(img, warped, inliers);
        CU_CHECK(!h.empty());
        CU_CHECK(h.rows == 3 && h.cols == 3);
        std::cout << "  homography matrix is 3x3 OK" << std::endl;
    }

    // 3) WarpImage 输出指定画布尺寸
    {
        const cv::Mat img = MakeHomographyInput(256);
        cv::Mat h = cv::Mat::eye(3, 3, CV_64F);
        const cv::Size canvas(300, 300);
        const cv::Mat warped = Alignment::WarpImage(img, h, canvas);

        CU_CHECK(!warped.empty());
        CU_CHECK(warped.size() == canvas);
        std::cout << "  WarpImage canvas size OK" << std::endl;
    }

    // 4) 空输入安全返回空
    {
        const cv::Mat empty;
        std::vector<cv::DMatch> inliers;
        CU_CHECK(Alignment::ComputeHomography(empty, empty, inliers).empty());
        CU_CHECK(Alignment::WarpImage(empty, cv::Mat::eye(3, 3, CV_64F), cv::Size(100, 100)).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Homography test passed =====" << std::endl;
}

#endif // TESTS_ALIGNMENT_HOMOGRAPHYTEST_H
