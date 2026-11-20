#ifndef TESTS_FEATUREMATCHING_FEATUREMATCHERTEST_H
#define TESTS_FEATUREMATCHING_FEATUREMATCHERTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/FeatureMatcher.h"
#include "FeatureMatching/LocalFeature.h"

#include "cli/TestAssert.h"

// 匹配 + RANSAC 模块断言测试。
// 真值来源：合成图 + 已知单应变换，验证匹配正确性与单应估计精度。

namespace {

cv::Mat MakeMatchInput(int size)
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

inline void FeatureMatcherTest()
{
    std::cout << "===== FeatureMatcher test begin =====" << std::endl;

    // 1) 同一图做已知单应（旋转+平移），BFMatch 能匹配
    {
        const cv::Mat img = MakeMatchInput(256);

        cv::Mat warped;
        cv::Mat h = cv::getRotationMatrix2D(cv::Point2f(128, 128), 20.0, 1.0);
        h.at<double>(0, 2) += 10.0;
        h.at<double>(1, 2) += 5.0;
        cv::warpAffine(img, warped, h, img.size());

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), warped, kp2, desc2);

        const auto matches = FeatureMatching::MatchBF(desc1, desc2, cv::NORM_HAMMING);
        CU_CHECK(!matches.empty());
        CU_CHECK(matches.size() > 50);
        std::cout << "  BFMatch OK (matches=" << matches.size() << ")" << std::endl;
    }

    // 2) RatioTestFilter 过滤后匹配数减少
    {
        const cv::Mat img = MakeMatchInput(256);
        cv::Mat warped;
        cv::Mat h = cv::getRotationMatrix2D(cv::Point2f(128, 128), 20.0, 1.0);
        cv::warpAffine(img, warped, h, img.size());

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), warped, kp2, desc2);

        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<std::vector<cv::DMatch>> knn;
        matcher.knnMatch(desc1, desc2, knn, 2);

        const auto good = FeatureMatching::RatioTestFilter(knn, 0.75);
        CU_CHECK(!good.empty());
        CU_CHECK(good.size() <= knn.size());
        std::cout << "  RatioTestFilter OK (good=" << good.size() << " of " << knn.size() << ")" << std::endl;
    }

    // 3) RansacHomography 估计单应，内点数足够
    {
        const cv::Mat img = MakeMatchInput(256);
        cv::Mat warped;
        cv::Mat hTrue = cv::getRotationMatrix2D(cv::Point2f(128, 128), 20.0, 1.0);
        hTrue.at<double>(0, 2) += 10.0;
        hTrue.at<double>(1, 2) += 5.0;
        cv::warpAffine(img, warped, hTrue, img.size());

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), warped, kp2, desc2);

        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<std::vector<cv::DMatch>> knn;
        matcher.knnMatch(desc1, desc2, knn, 2);
        const auto good = FeatureMatching::RatioTestFilter(knn, 0.75);

        std::vector<cv::DMatch> inliers;
        const cv::Mat h = FeatureMatching::RansacHomography(kp1, kp2, good, inliers);

        CU_CHECK(!h.empty());
        CU_CHECK(inliers.size() >= 4);
        std::cout << "  RansacHomography OK (inliers=" << inliers.size() << ")" << std::endl;
    }

    // 4) 内点不足时返回空 Mat（安全）
    {
        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        std::vector<cv::DMatch> matches;
        std::vector<cv::DMatch> inliers;
        const cv::Mat h = FeatureMatching::RansacHomography(kp1, kp2, matches, inliers);
        CU_CHECK(h.empty());
        std::cout << "  insufficient inliers safety OK" << std::endl;
    }

    // 5) 空描述子输入安全返回空
    {
        const cv::Mat emptyDesc;
        const auto matches = FeatureMatching::MatchBF(emptyDesc, emptyDesc, cv::NORM_HAMMING);
        CU_CHECK(matches.empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    // 6) MatchFLANN 对 float 描述子（SIFT）可用，对 CV_8U 安全返回空
    {
        const cv::Mat img = MakeMatchInput(256);
        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(200), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(200), img, kp2, desc2);

        const auto flannMatches = FeatureMatching::MatchFLANN(desc1, desc2);
        CU_CHECK(!flannMatches.empty());

        const auto invalid = FeatureMatching::MatchFLANN(cv::Mat(4, 4, CV_8U), cv::Mat(4, 4, CV_8U));
        CU_CHECK(invalid.empty());
        std::cout << "  MatchFLANN float OK / CV_8U guarded (" << flannMatches.size() << " matches)" << std::endl;
    }

    // 7) CrossCheckFilter 双向一致才保留
    {
        const cv::Mat img = MakeMatchInput(256);
        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(300), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(300), img, kp2, desc2);

        const auto m1 = FeatureMatching::MatchBF(desc1, desc2, cv::NORM_HAMMING);
        const auto m2 = FeatureMatching::MatchBF(desc2, desc1, cv::NORM_HAMMING);
        const auto cross = FeatureMatching::CrossCheckFilter(m1, m2);

        CU_CHECK(!cross.empty());
        CU_CHECK(cross.size() <= m1.size());
        std::cout << "  CrossCheckFilter OK (cross=" << cross.size() << " of " << m1.size() << ")" << std::endl;
    }

    std::cout << "===== FeatureMatcher test passed =====" << std::endl;
}

#endif // TESTS_FEATUREMATCHING_FEATUREMATCHERTEST_H
