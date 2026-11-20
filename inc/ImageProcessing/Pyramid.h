#ifndef INC_IMAGEPROCESSING_PYRAMID_H
#define INC_IMAGEPROCESSING_PYRAMID_H

#include <algorithm>
#include <vector>

#include "OpenCVCommon.h"

// 金字塔模块（高斯/拉普拉斯）。
// 定位：读懂原理 + 调用 OpenCV，封装成统一语义的可复用接口。
// 高斯金字塔：输入 CV_8UC1/CV_8UC3，输出 CV_8U 层，第 0 层 == src。
// 拉普拉斯金字塔：残差有正有负，层用 CV_32F 存储；重构输出 CV_8U。
namespace ImageProcessing {

namespace Detail {

// 计算可构建的最大层数：每层尺寸减半，直到任一维小于等于 1。
inline int MaxLevels(const cv::Mat& src)
{
    int levels = 0;
    int cols = src.cols;
    int rows = src.rows;
    while (cols > 1 && rows > 1) {
        ++levels;
        cols = (cols + 1) / 2;
        rows = (rows + 1) / 2;
    }
    return levels;
}

// float 域高斯金字塔（内部工具，供拉普拉斯构建使用）。
inline std::vector<cv::Mat> GaussianPyramidFloat(const cv::Mat& srcF, int levels)
{
    std::vector<cv::Mat> pyr;
    pyr.reserve(levels);
    pyr.push_back(srcF.clone());
    for (int i = 1; i < levels; ++i) {
        cv::Mat down;
        cv::pyrDown(pyr[i - 1], down);
        pyr.push_back(down);
    }
    return pyr;
}

} // namespace Detail

// 高斯金字塔：levels 层，第 0 层 == src，后续层逐层 pyrDown。
inline std::vector<cv::Mat> BuildGaussianPyramid(const cv::Mat& src, int levels)
{
    if (src.empty() || levels <= 0) {
        return {};
    }
    const int actualLevels = std::min(levels, Detail::MaxLevels(src));

    std::vector<cv::Mat> pyr;
    pyr.reserve(actualLevels);
    pyr.push_back(src.clone());
    for (int i = 1; i < actualLevels; ++i) {
        cv::Mat down;
        cv::pyrDown(pyr[i - 1], down);
        pyr.push_back(down);
    }
    return pyr;
}

// 拉普拉斯金字塔：levels 层（CV_32F），每层 = 高斯层 - pyrUp(下一高斯层)，顶层为最粗高斯层。
inline std::vector<cv::Mat> BuildLaplacianPyramid(const cv::Mat& src, int levels)
{
    if (src.empty() || levels <= 0) {
        return {};
    }
    const int actualLevels = std::min(levels, Detail::MaxLevels(src));

    cv::Mat srcF;
    src.convertTo(srcF, CV_32F);
    const auto gaussian = Detail::GaussianPyramidFloat(srcF, actualLevels);

    std::vector<cv::Mat> pyr;
    pyr.reserve(actualLevels);
    for (int i = 0; i < actualLevels; ++i) {
        if (i + 1 < actualLevels) {
            cv::Mat up;
            cv::pyrUp(gaussian[i + 1], up, gaussian[i].size());
            cv::Mat lap;
            cv::subtract(gaussian[i], up, lap);
            pyr.push_back(lap);
        } else {
            pyr.push_back(gaussian[i].clone());
        }
    }
    return pyr;
}

// 从拉普拉斯金字塔重构原图（输出 CV_8U）。
inline cv::Mat ReconstructFromLaplacian(const std::vector<cv::Mat>& pyr)
{
    if (pyr.empty()) {
        return {};
    }
    cv::Mat recon = pyr.back().clone();
    for (int i = static_cast<int>(pyr.size()) - 2; i >= 0; --i) {
        cv::Mat up;
        cv::pyrUp(recon, up, pyr[i].size());
        cv::add(up, pyr[i], recon);
    }
    cv::Mat out;
    recon.convertTo(out, CV_8U);
    return out;
}

} // namespace ImageProcessing

#endif // INC_IMAGEPROCESSING_PYRAMID_H
