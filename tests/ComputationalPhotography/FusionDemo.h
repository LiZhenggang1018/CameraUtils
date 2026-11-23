#ifndef TESTS_COMPUTATIONALPHOTOGRAPHY_FUSIONDEMO_H
#define TESTS_COMPUTATIONALPHOTOGRAPHY_FUSIONDEMO_H

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "OpenCVCommon.h"

#include "ComputationalPhotography/HdrMerge.h"
#include "ComputationalPhotography/Mfnr.h"

#include "CommonEnv.h"

// 多帧融合（HDR + 降噪）交互式 demo（GUI，不进 ctest）。
// 演示：
//   1. MFNR：同曝光多帧降噪（左：单帧噪声，右：多帧降噪）
//   2. HDR：多曝光融合（Mertens）
// 按键：
//   q / ESC  退出
inline int FusionDemo()
{
    const std::string dir = EnvRootPath() + "/data/ComputationalPhotography/Fusion/";

    // 尝试读真实素材（MFNR：同曝光多帧；HDR：多曝光）
    std::vector<cv::Mat> mfnrFrames;
    std::vector<cv::Mat> hdrFrames;
    for (int i = 1; i <= 6; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "%smfnr_%d.jpg", dir.c_str(), i);
        cv::Mat img = cv::imread(name);
        if (!img.empty()) {
            mfnrFrames.push_back(img);
        }
        std::snprintf(name, sizeof(name), "%shdr_%d.jpg", dir.c_str(), i);
        img = cv::imread(name);
        if (!img.empty()) {
            hdrFrames.push_back(img);
        }
    }

    // 素材不足时生成合成图
    if (mfnrFrames.size() < 4) {
        std::printf("[FusionDemo] MFNR 素材不足，生成合成图\n");
        cv::Mat base(360, 480, CV_8UC3, cv::Scalar(128, 128, 128));
        cv::RNG rng(17);
        for (int i = 0; i < 120; ++i) {
            const int bx = rng.uniform(0, 450);
            const int by = rng.uniform(0, 330);
            const int bw = rng.uniform(10, 40);
            const int bh = rng.uniform(10, 40);
            cv::rectangle(base, cv::Rect(bx, by, bw, bh),
                          cv::Scalar(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256)), -1);
        }
        mfnrFrames.clear();
        for (int i = 0; i < 8; ++i) {
            cv::RNG nrng(2000 + i * 7);
            cv::Mat noise(base.size(), CV_32FC3);
            nrng.fill(noise, cv::RNG::NORMAL, 0.0, 20.0);
            cv::Mat tmp;
            cv::Mat f32;
            base.convertTo(f32, CV_32FC3);
            cv::add(f32, noise, tmp);
            cv::Mat noisy;
            tmp.convertTo(noisy, CV_8UC3);
            mfnrFrames.push_back(noisy);
        }
    }

    if (hdrFrames.size() < 3) {
        std::printf("[FusionDemo] HDR 素材不足，生成不同曝光合成图\n");
        cv::Mat base(360, 480, CV_8UC3, cv::Scalar(128, 128, 128));
        cv::RNG rng(19);
        for (int i = 0; i < 120; ++i) {
            const int bx = rng.uniform(0, 450);
            const int by = rng.uniform(0, 330);
            const int bw = rng.uniform(10, 40);
            const int bh = rng.uniform(10, 40);
            cv::rectangle(base, cv::Rect(bx, by, bw, bh),
                          cv::Scalar(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256)), -1);
        }
        hdrFrames.clear();
        for (const double scale : {0.3, 1.0, 2.5}) {
            cv::Mat img;
            base.convertTo(img, -1, scale, 0.0);
            hdrFrames.push_back(img);
        }
    }

    // MFNR 降噪
    const cv::Mat mfnrResult = ComputationalPhotography::MfnrDenoise(mfnrFrames, 1);

    // HDR 融合
    const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());
    const cv::Mat hdrResult = ComputationalPhotography::MergeMertens(hdrFrames, cfg);

    const std::string windowName = "Multi-Frame Fusion Demo";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    // 拼接显示：上排 MFNR（单帧噪声 vs 降噪），下排 HDR（单曝光 vs 融合）
    cv::Mat mfnrSingle;
    cv::resize(mfnrFrames[0], mfnrSingle, cv::Size(480, 360));
    cv::Mat mfnrOut;
    cv::resize(mfnrResult, mfnrOut, cv::Size(480, 360));
    cv::Mat mfnrRow;
    cv::hconcat(std::vector<cv::Mat>{mfnrSingle, mfnrOut}, mfnrRow);

    cv::Mat hdrSingle;
    cv::resize(hdrFrames[1], hdrSingle, cv::Size(480, 360));
    cv::Mat hdrOut;
    cv::resize(hdrResult, hdrOut, cv::Size(480, 360));
    cv::Mat hdrRow;
    cv::hconcat(std::vector<cv::Mat>{hdrSingle, hdrOut}, hdrRow);

    cv::Mat display;
    cv::vconcat(std::vector<cv::Mat>{mfnrRow, hdrRow}, display);

    cv::putText(display, "top: MFNR (left=noisy single, right=denoised)", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    cv::putText(display, "bottom: HDR (left=single exposure, right=mertens)", cv::Point(10, 390),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    cv::putText(display, "q=quit", cv::Point(10, display.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200), 1,
                cv::LINE_AA);

    cv::imshow(windowName, display);
    int key = 0;
    while (key != 27 && key != 'q') {
        key = cv::waitKey(30);
    }

    cv::destroyAllWindows();
    return 0;
}

#endif // TESTS_COMPUTATIONALPHOTOGRAPHY_FUSIONDEMO_H
