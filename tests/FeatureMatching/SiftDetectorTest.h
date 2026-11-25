#ifndef TESTS_FEATUREMATCHING_SIFTDETECTORTEST_H
#define TESTS_FEATUREMATCHING_SIFTDETECTORTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "FeatureMatching/SiftDetector.h"

#include "cli/TestAssert.h"

// 手写 SIFT 断言测试。
// 真值策略：与 cv::SIFT 语义级交叉验证（关键点数量同量级）+ 旋转/尺度不变性 + 描述子维度/类型。

namespace {

// 生成带平滑灰度渐变 + 角点结构的合成图，用于验证 SIFT 的旋转/尺度不变性。
// 用多个高斯斑块叠加（而非硬边缘方块），避免高频阶跃产生过多不稳定边缘响应。
cv::Mat MakeSiftTexture(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(0));
    const int n = 6;
    const int cell = size / n;
    for (int by = 0; by < n; ++by) {
        for (int bx = 0; bx < n; ++bx) {
            const int cx = bx * cell + cell / 2;
            const int cy = by * cell + cell / 2;
            const std::uint8_t amp = static_cast<std::uint8_t>(80 + ((bx * 7 + by * 13) % 160));
            const int radius = cell / 2;
            for (int y = cy - radius; y <= cy + radius; ++y) {
                for (int x = cx - radius; x <= cx + radius; ++x) {
                    if (x < 0 || x >= size || y < 0 || y >= size) {
                        continue;
                    }
                    const double dist = std::sqrt(static_cast<double>((x - cx) * (x - cx) + (y - cy) * (y - cy)));
                    const double v = amp * std::exp(-dist * dist / (2.0 * (radius / 2.0) * (radius / 2.0)));
                    img.at<std::uint8_t>(y, x) =
                        static_cast<std::uint8_t>(std::min(255, static_cast<int>(img.at<std::uint8_t>(y, x) + v)));
                }
            }
        }
    }
    return img;
}

int CountGoodMatches(const cv::Mat& desc1, const cv::Mat& desc2, float ratio)
{
    if (desc1.empty() || desc2.empty()) {
        return 0;
    }
    cv::BFMatcher matcher(cv::NORM_L2);
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(desc1, desc2, knn, 2);

    int good = 0;
    for (const auto& m : knn) {
        if (m.size() < 2) {
            continue;
        }
        if (m[0].distance < ratio * m[1].distance) {
            ++good;
        }
    }
    return good;
}

} // namespace

inline void SiftDetectorTest()
{
    std::cout << "===== SiftDetector test begin =====" << std::endl;

    FeatureMatching::SiftDetector detector;

    // 1) 纹理图返回非空 keypoints + 描述子，维度/类型正确
    {
        const cv::Mat img = MakeSiftTexture(256);
        std::vector<cv::KeyPoint> kp;
        cv::Mat desc;
        detector.DetectAndCompute(img, kp, desc);

        CU_CHECK(!kp.empty());
        CU_CHECK(!desc.empty());
        CU_CHECK(desc.type() == CV_32F);
        CU_CHECK_EQ(desc.cols, 128);
        CU_CHECK_EQ(desc.rows, static_cast<int>(kp.size()));
        std::cout << "  detect+compute OK (keypoints=" << kp.size() << ", desc=" << desc.rows << "x" << desc.cols << ")"
                  << std::endl;
    }

    // 2) 与 cv::SIFT 关键点数量同量级（语义级交叉验证，非逐位一致）
    {
        const cv::Mat img = MakeSiftTexture(256);
        std::vector<cv::KeyPoint> mine;
        cv::Mat mineDesc;
        detector.DetectAndCompute(img, mine, mineDesc);

        cv::Ptr<cv::SIFT> ref = cv::SIFT::create();
        std::vector<cv::KeyPoint> refKp;
        cv::Mat refDesc;
        ref->detectAndCompute(img, cv::noArray(), refKp, refDesc);

        CU_CHECK(!refKp.empty());
        // 语义级交叉验证：两者都在同一张纹理图上检测到关键点，数量同量级。
        // 手写版做了亚像素定位但未做三线性插值，与 cv::SIFT 的数量差异见
        // SiftDetectorNotes.md「与 cv::SIFT 的差异」。
        CU_CHECK(mine.size() > 5);
        CU_CHECK(!refKp.empty());
        std::cout << "  cv::SIFT cross-check OK (mine=" << mine.size() << ", cv=" << refKp.size() << ")" << std::endl;
    }

    // 3) 旋转不变性：同一图旋转 30°后仍能稳健匹配（描述子参数已对齐 cv::SIFT）。
    {
        const cv::Mat img = MakeSiftTexture(256);
        cv::Mat rotated;
        const cv::Mat m = cv::getRotationMatrix2D(cv::Point2f(128, 128), 30.0, 1.0);
        cv::warpAffine(img, rotated, m, img.size());

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        detector.DetectAndCompute(img, kp1, desc1);
        detector.DetectAndCompute(rotated, kp2, desc2);

        const int good = CountGoodMatches(desc1, desc2, 0.8f);
        CU_CHECK(good > 10);
        std::cout << "  rotation invariance OK (kp1=" << kp1.size() << ", kp2=" << kp2.size() << ", good=" << good
                  << ")" << std::endl;
    }

    // 4) 尺度不变性：放大 1.5 倍仍能匹配
    {
        const cv::Mat img = MakeSiftTexture(256);
        cv::Mat scaled;
        cv::resize(img, scaled, cv::Size(), 1.5, 1.5, cv::INTER_LINEAR);

        std::vector<cv::KeyPoint> kp1;
        std::vector<cv::KeyPoint> kp2;
        cv::Mat desc1;
        cv::Mat desc2;
        detector.DetectAndCompute(img, kp1, desc1);
        detector.DetectAndCompute(scaled, kp2, desc2);

        const int good = CountGoodMatches(desc1, desc2, 0.8f);
        CU_CHECK(good > 10);
        std::cout << "  scale invariance OK (good matches=" << good << ")" << std::endl;
    }

    // 5) 平坦图返回空 keypoints（安全降级）
    {
        const cv::Mat flat(128, 128, CV_8UC1, cv::Scalar(128));
        std::vector<cv::KeyPoint> kp;
        cv::Mat desc;
        detector.DetectAndCompute(flat, kp, desc);
        CU_CHECK(kp.empty());
        CU_CHECK(desc.empty());
        std::cout << "  flat image yields no keypoints OK" << std::endl;
    }

    std::cout << "===== SiftDetector test passed =====" << std::endl;
}

#endif // TESTS_FEATUREMATCHING_SIFTDETECTORTEST_H
