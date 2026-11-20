#ifndef INC_ALIGNMENT_BLENDING_H
#define INC_ALIGNMENT_BLENDING_H

#include <vector>

#include "OpenCVCommon.h"

#include "ImageProcessing/Pyramid.h"

// 图像融合（多波段 blending）模块。
// 定位：读懂原理 + 调用 OpenCV，复用第 3 章拉普拉斯金字塔做多波段融合。
// 多波段融合思想：分别在低频（高斯顶层）和高频（拉普拉斯各层）上按 mask 加权，
// 避免在单分辨率上直接混合造成的明显接缝。
namespace Alignment {

// 多波段融合：img1/img2 按 mask1/mask2 加权融合（mask1 + mask2 = 1 的区域）。
// levels 控制金字塔层数。输出 CV_8U。
inline cv::Mat MultiBandBlend(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask1, const cv::Mat& mask2,
                              int levels)
{
    if (img1.empty() || img2.empty() || mask1.empty() || mask2.empty() || levels <= 0) {
        return {};
    }
    if (img1.size() != img2.size()) {
        return {};
    }

    // 转 float 处理
    cv::Mat f1;
    cv::Mat f2;
    img1.convertTo(f1, CV_32F);
    img2.convertTo(f2, CV_32F);
    cv::Mat m1;
    cv::Mat m2;
    mask1.convertTo(m1, CV_32F, 1.0 / 255.0);
    mask2.convertTo(m2, CV_32F, 1.0 / 255.0);

    const int actualLevels = std::min(levels, ImageProcessing::Detail::MaxLevels(img1));
    if (actualLevels <= 0) {
        return {};
    }

    // 两图的拉普拉斯金字塔（顶层为高斯低频层）
    const auto lap1 = ImageProcessing::BuildLaplacianPyramid(img1, actualLevels);
    const auto lap2 = ImageProcessing::BuildLaplacianPyramid(img2, actualLevels);

    // mask 的高斯金字塔（用于各层加权）
    const auto maskPyr1 = ImageProcessing::BuildGaussianPyramid(m1, actualLevels);
    const auto maskPyr2 = ImageProcessing::BuildGaussianPyramid(m2, actualLevels);

    // 逐层加权融合
    std::vector<cv::Mat> blendedPyr;
    blendedPyr.reserve(actualLevels);
    for (int i = 0; i < actualLevels; ++i) {
        const cv::Mat blended = lap1[i].mul(maskPyr1[i]) + lap2[i].mul(maskPyr2[i]);
        blendedPyr.push_back(blended);
    }

    // 重构
    cv::Mat recon = blendedPyr.back().clone();
    for (int i = actualLevels - 2; i >= 0; --i) {
        cv::Mat up;
        cv::pyrUp(recon, up, blendedPyr[i].size());
        cv::add(up, blendedPyr[i], recon);
    }

    cv::Mat out;
    recon.convertTo(out, CV_8U);
    return out;
}

} // namespace Alignment

#endif // INC_ALIGNMENT_BLENDING_H
