#ifndef TESTS_FEATUREMATCHING_FEATUREMATCHERDEMO_H
#define TESTS_FEATUREMATCHING_FEATUREMATCHERDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/FeatureMatcher.h"
#include "FeatureMatching/LocalFeature.h"

#include "CommonEnv.h"

// 特征匹配 + RANSAC 交互式 demo（GUI，不进 ctest）。
// 展示两图（原图 + 已知单应变换图）的匹配线与 RANSAC 内点。
// 按键：
//   r        调整 ratio 阈值
//   q / ESC  退出
inline int FeatureMatcherDemo()
{
    const std::string inputPath = EnvRootPath() + "/data/FeatureMatching/FeatureMatcher/input.jpg";

    cv::Mat src = cv::imread(inputPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::printf("[FeatureMatcherDemo] %s not found, generating textured image\n", inputPath.c_str());
        src = cv::Mat(512, 512, CV_8UC1);
        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                src.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x * 7 + y * 13 + (x * y) / 16) % 256);
            }
        }
    }

    // 生成第二张图（已知单应：旋转 + 平移）
    cv::Mat warped;
    cv::Mat h = cv::getRotationMatrix2D(cv::Point2f(src.cols / 2.0f, src.rows / 2.0f), 15.0, 1.0);
    h.at<double>(0, 2) += 20.0;
    h.at<double>(1, 2) += 10.0;
    cv::warpAffine(src, warped, h, src.size());

    double ratio = 0.75;

    const std::string windowName = "Feature Matcher Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    bool running = true;
    while (running) {
        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), src, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), warped, kp2, desc2);

        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<std::vector<cv::DMatch>> knn;
        matcher.knnMatch(desc1, desc2, knn, 2);
        const auto good = FeatureMatching::RatioTestFilter(knn, ratio);

        std::vector<cv::DMatch> inliers;
        const cv::Mat homography = FeatureMatching::RansacHomography(kp1, kp2, good, inliers);

        cv::Mat display;
        cv::drawMatches(src, kp1, warped, kp2, inliers, display);

        char status[256];
        std::snprintf(status, sizeof(status), "ratio=%.2f  good=%zu  inliers=%zu  H=%s", ratio, good.size(),
                      inliers.size(), homography.empty() ? "empty" : "ok");
        cv::putText(display, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1,
                    cv::LINE_AA);
        cv::putText(display, "r=ratio +/- q=quit", cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(200), 1, cv::LINE_AA);

        cv::imshow(windowName, display);

        const int key = cv::waitKey(30);
        switch (key) {
        case 27:
        case 'q':
            running = false;
            break;
        case 'r':
        case '+':
        case '=':
            ratio = std::min(0.95, ratio + 0.05);
            break;
        case '-':
            ratio = std::max(0.5, ratio - 0.05);
            break;
        default:
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_FEATUREMATCHING_FEATUREMATCHERDEMO_H
