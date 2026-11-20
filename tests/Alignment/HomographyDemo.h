#ifndef TESTS_ALIGNMENT_HOMOGRAPHYDEMO_H
#define TESTS_ALIGNMENT_HOMOGRAPHYDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "Alignment/Homography.h"

#include "CommonEnv.h"

// 单应 + warping 交互式 demo（GUI，不进 ctest）。
// 展示两图（原图 + 已知单应变换图）的配准结果与 warp 效果。
// 按键：
//   q / ESC  退出
inline int HomographyDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/Alignment/Homography/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[HomographyDemo] %s not found, generating textured image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13 + (x * y) / 16) % 256);
            }
        }
    }

    // 生成第二张图（已知单应：旋转 + 平移）
    cv::Mat warped;
    cv::Mat hTrue = cv::getRotationMatrix2D(cv::Point2f(src.cols / 2.0f, src.rows / 2.0f), 20.0, 1.0);
    hTrue.at<double>(0, 2) += 30.0;
    hTrue.at<double>(1, 2) += 20.0;
    cv::warpAffine(src, warped, hTrue, src.size());

    // 计算单应
    std::vector<cv::DMatch> inliers;
    const cv::Mat h = Alignment::ComputeHomography(src, warped, inliers);

    // warp src 到 warped 坐标系（用估计的 H）
    cv::Mat srcWarped = Alignment::WarpImage(src, h, warped.size());

    const std::string windowName = "Homography Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::Mat display;
    cv::hconcat(std::vector<cv::Mat>{src, warped, srcWarped}, display);

    char status[256];
    std::snprintf(status, sizeof(status), "inliers=%zu  H=%s", inliers.size(), h.empty() ? "empty" : "ok");
    cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                cv::LINE_AA);
    cv::putText(display, "left=src middle=warped right=src-warped-by-H  q=quit", cv::Point(10, display.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1, cv::LINE_AA);

    cv::imshow(windowName, display);
    int key = 0;
    while (key != 27 && key != 'q') {
        key = cv::waitKey(30);
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_ALIGNMENT_HOMOGRAPHYDEMO_H
