#ifndef TESTS_COMPUTATIONALPHOTOGRAPHY_MFNRTEST_H
#define TESTS_COMPUTATIONALPHOTOGRAPHY_MFNRTEST_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "OpenCVCommon.h"

#include "ComputationalPhotography/Mfnr.h"

#include "cli/TestAssert.h"

// MFNR 多帧降噪断言测试。
// 真值来源：多张同图加独立噪声，降噪后方差应小于单张。

namespace {

cv::Mat MakeCleanImage(int size)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(128));
    cv::RNG rng(13);
    for (int i = 0; i < 100; ++i) {
        const int bx = rng.uniform(0, size - 30);
        const int by = rng.uniform(0, size - 30);
        const int bw = rng.uniform(8, 30);
        const int bh = rng.uniform(8, 30);
        cv::rectangle(img, cv::Rect(bx, by, bw, bh), cv::Scalar(rng.uniform(0, 256)), -1);
    }
    return img;
}

double Variance(const cv::Mat& img)
{
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(img, mean, stddev);
    return stddev[0] * stddev[0];
}

} // namespace

inline void MfnrTest()
{
    std::cout << "===== Mfnr test begin =====" << std::endl;

    // 1) 多张同图加独立噪声，降噪后方差 < 单张噪声方差
    {
        const cv::Mat clean = MakeCleanImage(128);

        std::vector<cv::Mat> noisyFrames;
        for (int i = 0; i < 8; ++i) {
            cv::RNG rng(1000 + i * 7);
            cv::Mat noise(clean.size(), CV_32FC1);
            rng.fill(noise, cv::RNG::NORMAL, 0.0, 20.0);
            cv::Mat noisy;
            cv::Mat tmp;
            clean.convertTo(tmp, CV_32F);
            cv::add(tmp, noise, tmp);
            tmp.convertTo(noisy, CV_8U);
            noisyFrames.push_back(noisy);
        }

        const double singleVar = Variance(noisyFrames[0]);
        const cv::Mat denoised = ComputationalPhotography::MfnrDenoise(noisyFrames, 1);

        CU_CHECK(!denoised.empty());
        CU_CHECK(denoised.size() == clean.size());

        const double denoisedVar = Variance(denoised);
        std::cout << "  var single=" << singleVar << " denoised=" << denoisedVar << std::endl;
        CU_CHECK(denoisedVar < singleVar);
    }

    // 2) 输出尺寸正确
    {
        const cv::Mat clean = MakeCleanImage(128);
        std::vector<cv::Mat> frames;
        for (int i = 0; i < 4; ++i) {
            cv::Mat noise(clean.size(), CV_32FC1);
            cv::randn(noise, 0, 15);
            cv::Mat noise8u;
            noise.convertTo(noise8u, CV_8U);
            cv::Mat noisy;
            cv::add(clean, noise8u, noisy);
            frames.push_back(noisy);
        }
        const cv::Mat out = ComputationalPhotography::MfnrDenoise(frames, 1);
        CU_CHECK(out.size() == clean.size());
        std::cout << "  output size OK" << std::endl;
    }

    // 3) 单张输入降级（不崩）
    {
        const cv::Mat clean = MakeCleanImage(128);
        const cv::Mat out = ComputationalPhotography::MfnrDenoise({clean}, 1);
        CU_CHECK(!out.empty());
        std::cout << "  single-frame degrade OK" << std::endl;
    }

    // 4) 空输入安全
    {
        CU_CHECK(ComputationalPhotography::MfnrDenoise({}, 1).empty());
        std::cout << "  empty-input safety OK" << std::endl;
    }

    std::cout << "===== Mfnr test passed =====" << std::endl;
}

#endif // TESTS_COMPUTATIONALPHOTOGRAPHY_MFNRTEST_H
