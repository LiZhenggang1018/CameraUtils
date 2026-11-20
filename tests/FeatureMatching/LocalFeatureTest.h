#ifndef TESTS_FEATUREMATCHING_LOCALFEATURETEST_H
#define TESTS_FEATUREMATCHING_LOCALFEATURETEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/LocalFeature.h"

#include "cli/TestAssert.h"

// 局部特征模块断言测试。
// 真值来源：描述子类型断言 + 旋转/缩放不变性交叉验证。

namespace {

// 生成有纹理的合成图（确定性图案）
cv::Mat MakeTextured(int size)
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

inline void LocalFeatureTest()
{
    std::cout << "===== LocalFeature test begin =====" << std::endl;

    // 1) 创建器返回非空 Ptr
    {
        CU_CHECK(FeatureMatching::CreateSift(500) != nullptr);
        CU_CHECK(FeatureMatching::CreateOrb(500) != nullptr);
        CU_CHECK(FeatureMatching::CreateAkaze() != nullptr);
        std::cout << "  detectors created OK" << std::endl;
    }

    // 2) DetectAndCompute 在纹理图上返回非空 keypoints + descriptors，行数一致
    {
        const cv::Mat img = MakeTextured(256);

        std::vector<cv::KeyPoint> kp1;
        cv::Mat desc1;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(200), img, kp1, desc1);
        CU_CHECK(!kp1.empty());
        CU_CHECK(!desc1.empty());
        CU_CHECK_EQ(desc1.rows, static_cast<int>(kp1.size()));

        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(200), img, kp2, desc2);
        CU_CHECK(!kp2.empty());
        CU_CHECK(!desc2.empty());
        CU_CHECK_EQ(desc2.rows, static_cast<int>(kp2.size()));

        std::cout << "  detect+compute OK (sift=" << kp1.size() << " orb=" << kp2.size() << ")" << std::endl;
    }

    // 3) 描述子类型断言：SIFT=CV_32F，ORB/AKAZE=CV_8U
    {
        const cv::Mat img = MakeTextured(256);

        std::vector<cv::KeyPoint> kp;
        cv::Mat desc;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(100), img, kp, desc);
        CU_CHECK(desc.type() == CV_32F);

        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(100), img, kp, desc);
        CU_CHECK(desc.type() == CV_8U);

        FeatureMatching::DetectAndCompute(FeatureMatching::CreateAkaze(), img, kp, desc);
        CU_CHECK(desc.type() == CV_8U);
        std::cout << "  descriptor types OK" << std::endl;
    }

    // 4) 旋转不变性：同一图旋转后，ORB 仍能匹配（用 BFMatcher 交叉验证）
    {
        const cv::Mat img = MakeTextured(256);

        cv::Mat rotated;
        cv::Mat m = cv::getRotationMatrix2D(cv::Point2f(128, 128), 30.0, 1.0);
        cv::warpAffine(img, rotated, m, img.size());

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), img, kp1, desc1);
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateOrb(500), rotated, kp2, desc2);

        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<cv::DMatch> matches;
        matcher.match(desc1, desc2, matches);

        CU_CHECK(matches.size() > 50);
        std::cout << "  rotation invariance OK (matches=" << matches.size() << ")" << std::endl;
    }

    // 5) 平坦图返回空 keypoints（安全）
    {
        const cv::Mat flat(128, 128, CV_8UC1, cv::Scalar(128));
        std::vector<cv::KeyPoint> kp;
        cv::Mat desc;
        FeatureMatching::DetectAndCompute(FeatureMatching::CreateSift(100), flat, kp, desc);
        CU_CHECK(kp.empty());
        std::cout << "  flat image yields no keypoints OK" << std::endl;
    }

    std::cout << "===== LocalFeature test passed =====" << std::endl;
}

#endif // TESTS_FEATUREMATCHING_LOCALFEATURETEST_H
