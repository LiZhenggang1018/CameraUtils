/*
 * @Description: HDR 融合（Mertens / 物理 HDR 双管线 + 鬼影抑制）
 */
#ifndef HDR_MERGE_H
#define HDR_MERGE_H

#include "OpenCVCommon.h"
#include "HdrMerge/HdrConfig.h"
#include "LivePhotoSmoothPlay/ImageRegistrator.h"
#include "OpticalFlow/BiDirectionalLkFilter.h"
#include "Exiv2Common.h"
#include "CommonEnv.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace hdr_merge {

// ============================================================
// 配准（新增可选 outResult 输出配准质量指标）
// ============================================================
inline cv::Mat AlignToReference(
    const cv::Mat& reference,
    const cv::Mat& target,
    ImageRegistrator& registrator,
    FeatureDetectorType detector = FeatureDetectorType::ORB,
    TransformType transform = TransformType::HOMOGRAPHY,
    RegistrationResult* outResult = nullptr)
{
    try
    {
        auto result = registrator.registerImages(reference, target, detector, transform);

        if (outResult)
        {
            *outResult = result;
        }

        if (result.registered_image.empty())
        {
            std::cerr << "[HdrMerge] Align failed (empty registered_image), use resized original"
                      << std::endl;
            cv::Mat fallback;
            cv::resize(target, fallback, reference.size());
            return fallback;
        }

        if (result.num_inliers < 30 || result.rms_error > 5.0)
        {
            std::cerr << "[HdrMerge] Warning: poor alignment (inliers=" << result.num_inliers
                      << ", rms=" << result.rms_error << "), result may have artifacts"
                      << std::endl;
        }
        else
        {
            std::cout << "[HdrMerge] Align OK: inliers=" << result.num_inliers
                      << ", rms=" << result.rms_error << " px" << std::endl;
        }

        return result.registered_image;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HdrMerge] Align exception: " << e.what()
                  << ", use resized original" << std::endl;
        if (outResult)
        {
            *outResult = RegistrationResult{};
        }
        cv::Mat fallback;
        cv::resize(target, fallback, reference.size());
        return fallback;
    }
}

// ============================================================
// 黑边检测（不变）
// ============================================================
inline cv::Rect DetectContentRoi(
    const cv::Mat& img,
    int blackThreshold = 5,
    double blackRatio = 0.95)
{
    if (img.empty())
    {
        return {};
    }

    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    const int H = gray.rows;
    const int W = gray.cols;

    auto isBlackRow = [&](int y) -> bool
    {
        int blackCount = 0;
        const uint8_t* row = gray.ptr<uint8_t>(y);
        for (int x = 0; x < W; ++x)
        {
            if (row[x] < blackThreshold) ++blackCount;
        }
        return static_cast<double>(blackCount) / W >= blackRatio;
    };

    auto isBlackCol = [&](int x) -> bool
    {
        int blackCount = 0;
        for (int y = 0; y < H; ++y)
        {
            if (gray.at<uint8_t>(y, x) < blackThreshold) ++blackCount;
        }
        return static_cast<double>(blackCount) / H >= blackRatio;
    };

    int top = 0;
    while (top < H && isBlackRow(top)) ++top;

    int bottom = H - 1;
    while (bottom >= top && isBlackRow(bottom)) --bottom;

    int left = 0;
    while (left < W && isBlackCol(left)) ++left;

    int right = W - 1;
    while (right >= left && isBlackCol(right)) --right;

    if (top >= bottom || left >= right)
    {
        return cv::Rect(0, 0, W, H);
    }

    return cv::Rect(left, top, right - left + 1, bottom - top + 1);
}

// ============================================================
// 公共 ROI（不变）
// ============================================================
inline cv::Rect ComputeCommonRoi(
    const cv::Mat& alignedLow,
    const cv::Mat& alignedHigh,
    const cv::Mat& reference,
    const HdrConfig& cfg)
{
    if (!cfg.autoDetectBorder)
    {
        const int W = reference.cols;
        const int H = reference.rows;
        const int bx = static_cast<int>(W * cfg.cropRatio);
        const int by = static_cast<int>(H * cfg.cropRatio);
        return cv::Rect(bx, by, W - 2 * bx, H - 2 * by);
    }

    cv::Rect roiLow  = DetectContentRoi(alignedLow,  cfg.blackThreshold, cfg.blackRatio);
    cv::Rect roiHigh = DetectContentRoi(alignedHigh, cfg.blackThreshold, cfg.blackRatio);

    const int x1 = std::min(roiLow.x, roiHigh.x);
    const int y1 = std::min(roiLow.y, roiHigh.y);
    const int x2 = std::max(roiLow.x + roiLow.width,  roiHigh.x + roiHigh.width);
    const int y2 = std::max(roiLow.y + roiLow.height, roiHigh.y + roiHigh.height);
    cv::Rect common(x1, y1, x2 - x1, y2 - y1);

    const double areaRatio = static_cast<double>(common.area()) / reference.total();
    if (areaRatio < 0.25)
    {
        std::cerr << "[HdrMerge] Content ROI too small (" << (areaRatio * 100) << "%), "
                  << "fallback to fixed crop " << (cfg.cropRatio * 100) << "%" << std::endl;

        const int W = reference.cols;
        const int H = reference.rows;
        const int bx = static_cast<int>(W * cfg.cropRatio);
        const int by = static_cast<int>(H * cfg.cropRatio);
        return cv::Rect(bx, by, W - 2 * bx, H - 2 * by);
    }

    std::cout << "[HdrMerge] Content ROI: " << common
              << " (" << (areaRatio * 100) << "% of frame)" << std::endl;
    return common;
}

// ============================================================
// Unsharp Mask（不变）
// ============================================================
inline cv::Mat UnsharpMask(
    const cv::Mat& src,
    double amount,
    double radius,
    double threshold)
{
    if (src.empty() || amount <= 0.0)
    {
        return src.clone();
    }

    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(0, 0), radius);

    cv::Mat sharpened;
    cv::addWeighted(src, 1.0 + amount, blurred, -amount, 0.0, sharpened);

    if (threshold > 0.0)
    {
        cv::Mat diff;
        cv::absdiff(src, sharpened, diff);

        cv::Mat diffGray;
        if (diff.channels() == 3)
        {
            cv::cvtColor(diff, diffGray, cv::COLOR_BGR2GRAY);
        }
        else
        {
            diffGray = diff;
        }

        cv::Mat mask;
        cv::compare(diffGray, cv::Scalar(threshold), mask, cv::CMP_GT);

        cv::Mat result = src.clone();
        sharpened.copyTo(result, mask);
        return result;
    }

    return sharpened;
}

// ============================================================
// Mertens 融合（不变）
// ============================================================
inline cv::Mat MergeMertens(
    const std::vector<cv::Mat>& images,
    const HdrConfig& cfg)
{
    if (images.empty())
    {
        return {};
    }

    cv::Ptr<cv::MergeMertens> merger = cv::createMergeMertens();

    merger->setContrastWeight(static_cast<float>(cfg.contrastWeight));
    merger->setSaturationWeight(static_cast<float>(cfg.saturationWeight));
    merger->setExposureWeight(static_cast<float>(cfg.exposureWeight));

    std::cout << "[HdrMerge] Mertens weights: contrast=" << cfg.contrastWeight
              << ", saturation=" << cfg.saturationWeight
              << ", exposure=" << cfg.exposureWeight << std::endl;

    cv::Mat fused;
    merger->process(images, fused);

    cv::Mat fused8u;
    fused.convertTo(fused8u, CV_8UC3, 255.0);
    return fused8u;
}

// ============================================================
// 物理 HDR 基础：sRGB De-gamma LUT
// ============================================================
inline cv::Mat BuildSrgbDegammaLUT()
{
    cv::Mat lut(1, 256, CV_32FC1);
    for (int i = 0; i < 256; ++i)
    {
        float v = static_cast<float>(i) / 255.0f;
        if (v <= 0.04045f)
        {
            lut.at<float>(0, i) = v / 12.92f;
        }
        else
        {
            lut.at<float>(0, i) = std::pow((v + 0.055f) / 1.055f, 2.4f);
        }
    }
    return lut;
}

inline cv::Mat SrgbToLinear(const cv::Mat& bgr8u, const cv::Mat& lut)
{
    cv::Mat linear;
    cv::LUT(bgr8u, lut, linear);
    return linear;
}

// ============================================================
// 物理 HDR：等效曝光量解析（不变）
// ============================================================
inline std::vector<float> ResolveEffectiveExposures(
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const HdrConfig& cfg)
{
    std::vector<double> times;
    std::vector<double> isos;

    if (cfg.readExposureFromExif)
    {
        auto iLow    = CameraUtils::ReadExposureInfo(lowPath);
        auto iNormal = CameraUtils::ReadExposureInfo(normalPath);
        auto iHigh   = CameraUtils::ReadExposureInfo(highPath);

        if (iLow.valid() && iNormal.valid() && iHigh.valid())
        {
            times = { iLow.timeSeconds, iNormal.timeSeconds, iHigh.timeSeconds };
            isos  = { iLow.iso,         iNormal.iso,         iHigh.iso };
            std::cout << "[HdrMerge] EXIF exposure: "
                      << "low="    << iLow.timeSeconds    << "s/" << iLow.iso    << "ISO, "
                      << "normal=" << iNormal.timeSeconds << "s/" << iNormal.iso << "ISO, "
                      << "high="   << iHigh.timeSeconds   << "s/" << iHigh.iso   << "ISO"
                      << std::endl;
        }
        else
        {
            std::cout << "[HdrMerge] EXIF exposure incomplete (low="
                      << (iLow.valid() ? "ok" : "missing")
                      << ", normal=" << (iNormal.valid() ? "ok" : "missing")
                      << ", high="   << (iHigh.valid() ? "ok" : "missing")
                      << ")" << std::endl;
        }
    }

    if (times.size() != 3 || isos.size() != 3)
    {
        if (cfg.manualExposureTimes.size() == 3 && cfg.manualIsos.size() == 3)
        {
            times = cfg.manualExposureTimes;
            isos  = cfg.manualIsos;
            std::cout << "[HdrMerge] Using manual exposure config" << std::endl;
        }
        else
        {
            return {};
        }
    }

    const double baseIso = cfg.baseIso > 0.0 ? cfg.baseIso : 100.0;
    std::vector<float> result(3);
    for (size_t i = 0; i < 3; ++i)
    {
        result[i] = static_cast<float>(times[i] * isos[i] / baseIso);
    }

    std::cout << "[HdrMerge] Effective exposures: "
              << result[0] << ", " << result[1] << ", " << result[2] << std::endl;
    return result;
}

// ============================================================
// 物理 HDR：色调映射（不变）
// ============================================================
inline cv::Mat TonemapHdr(const cv::Mat& hdr32f, const HdrConfig& cfg)
{
    if (hdr32f.empty())
    {
        return {};
    }

    cv::Mat ldr32f;
    try
    {
        if (cfg.tonemapType == "reinhard")
        {
            auto t = cv::createTonemapReinhard(
                static_cast<float>(cfg.reinhardGamma),
                static_cast<float>(cfg.reinhardIntensity),
                static_cast<float>(cfg.reinhardLightAdapt),
                0.0f);
            t->process(hdr32f, ldr32f);
        }
        else
        {
            if (cfg.tonemapType != "drago")
            {
                std::cerr << "[HdrMerge] Unknown tonemapType=\"" << cfg.tonemapType
                          << "\", fallback to drago" << std::endl;
            }
            auto t = cv::createTonemapDrago(
                static_cast<float>(cfg.dragoGamma),
                static_cast<float>(cfg.dragoSaturation));
            t->process(hdr32f, ldr32f);
        }
    }
    catch (const cv::Exception& e)
    {
        std::cerr << "[HdrMerge] Tonemap exception: " << e.what() << std::endl;
        return {};
    }

    if (ldr32f.empty())
    {
        return {};
    }

    cv::Mat ldr8u;
    ldr32f.convertTo(ldr8u, CV_8UC3, 255.0);
    return ldr8u;
}

// ============================================================
// 鬼影抑制：工具函数
// ============================================================

// 灰度图的中位数（用于亮度归一化）
inline double ComputeGrayMedian(const cv::Mat& gray)
{
    if (gray.empty()) return 0.0;

    cv::Mat flat = gray.reshape(1, 1);
    if (!flat.isContinuous())
    {
        flat = flat.clone();
    }
    if (flat.total() == 0) return 0.0;

    std::vector<uint8_t> data(flat.data, flat.data + flat.total());
    const size_t mid = data.size() / 2;
    std::nth_element(data.begin(), data.begin() + mid, data.end());
    return static_cast<double>(data[mid]);
}

// 亮度归一化后的灰度差分
inline cv::Mat NormalizedAbsDiff(
    const cv::Mat& refBgr,
    const cv::Mat& otherBgr)
{
    cv::Mat gRef, gOther;
    cv::cvtColor(refBgr,   gRef,   cv::COLOR_BGR2GRAY);
    cv::cvtColor(otherBgr, gOther, cv::COLOR_BGR2GRAY);

    const double mRef   = ComputeGrayMedian(gRef);
    const double mOther = ComputeGrayMedian(gOther);
    double s = mRef / std::max(mOther, 1.0);
    s = std::clamp(s, 0.2, 5.0);   // 防爆

    cv::Mat scaled;
    gOther.convertTo(scaled, CV_8U, s);

    cv::Mat diff;
    cv::absdiff(gRef, scaled, diff);
    return diff;
}

inline cv::Mat DetectMotionDiff(
    const cv::Mat& lowCropped,
    const cv::Mat& normalCropped,
    const cv::Mat& highCropped,
    const HdrConfig& cfg)
{
    cv::Mat lut = BuildSrgbDegammaLUT();
    cv::Mat linL = SrgbToLinear(lowCropped,    lut);
    cv::Mat linN = SrgbToLinear(normalCropped, lut);
    cv::Mat linH = SrgbToLinear(highCropped,   lut);

    // 线性域中位数比值对齐
    double mL = LinearMedian(linL), mN = LinearMedian(linN), mH = LinearMedian(linH);
    linL *= (mN / std::max(mL, 1e-6));
    linH *= (mN / std::max(mH, 1e-6));

    cv::Mat diffL = LinearAbsDiff(linN, linL);
    cv::Mat diffH = LinearAbsDiff(linN, linH);
    cv::Mat diff  = cv::max(diffL, diffH);   // 可以是逐通道 max，再转灰度

    // 自适应阈值（Otsu 或局部）
    cv::Mat mask;
    cv::threshold(diff, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    return mask;
}

// ============================================================
// Level 2：双向 LK 反推不稳定点 → mask
// ============================================================
inline cv::Mat DetectMotionLk(
    const cv::Mat& lowCropped,
    const cv::Mat& normalCropped,
    const cv::Mat& highCropped,
    const HdrConfig& cfg)
{
    cv::Mat gL, gN, gH;
    cv::cvtColor(lowCropped,    gL, cv::COLOR_BGR2GRAY);
    cv::cvtColor(normalCropped, gN, cv::COLOR_BGR2GRAY);
    cv::cvtColor(highCropped,   gH, cv::COLOR_BGR2GRAY);

    // 检测角点：按面积自适应，保持点密度合理
    const int maxCorners = std::clamp(
        static_cast<int>(gN.total() / 2000), 200, 800);

    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(gN, pts, maxCorners, 0.01, 10.0);
    if (pts.empty())
    {
        std::cerr << "[HdrMerge] LK ghost: no features detected" << std::endl;
        return cv::Mat::zeros(gN.size(), CV_8UC1);
    }

    BiDirectionalLkFilter filter;
    filter.SetFbThreshold(cfg.lkFbThreshold);

    std::vector<cv::Point2f> stablePrevL, stableCurrL;
    std::vector<float>       errL;
    filter.Filter(gN, gL, pts, stablePrevL, stableCurrL, errL);

    std::vector<cv::Point2f> stablePrevH, stableCurrH;
    std::vector<float>       errH;
    filter.Filter(gN, gH, pts, stablePrevH, stableCurrH, errH);

    auto isStable = [](const cv::Point2f& p,
                       const std::vector<cv::Point2f>& stable) -> bool
    {
        for (const auto& s : stable)
        {
            if (std::abs(p.x - s.x) < 1.0f &&
                std::abs(p.y - s.y) < 1.0f)
            {
                return true;
            }
        }
        return false;
    };

    cv::Mat sparseMask = cv::Mat::zeros(gN.size(), CV_8UC1);
    int unstableCount = 0;
    for (const auto& p : pts)
    {
        if (!isStable(p, stablePrevL) || !isStable(p, stablePrevH))
        {
            cv::circle(sparseMask, p, 5, cv::Scalar(255), -1);
            ++unstableCount;
        }
    }

    std::cout << "[HdrMerge] LK ghost: " << pts.size() << " features, "
              << unstableCount << " unstable" << std::endl;

    // 稀疏 → 稠密
    const int r = std::max(5, cfg.motionDilateRadius);
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(2 * r + 1, 2 * r + 1));
    cv::dilate(sparseMask, sparseMask, kernel);

    cv::Mat dense;
    cv::GaussianBlur(sparseMask, dense, cv::Size(2 * r + 1, 2 * r + 1), 0);
    return dense;
}

// ============================================================
// 综合运动 mask 构建
// ============================================================
inline cv::Mat BuildMotionMask(
    const cv::Mat& lowCropped,
    const cv::Mat& normalCropped,
    const cv::Mat& highCropped,
    const RegistrationResult& regLow,
    const RegistrationResult& regHigh,
    const HdrConfig& cfg)
{
    // 配准质量门控
    const bool poorLow  = regLow.num_inliers  < 100 || regLow.rms_error  > 3.0;
    const bool poorHigh = regHigh.num_inliers < 100 || regHigh.rms_error > 3.0;
    if (poorLow || poorHigh)
    {
        std::cerr << "[HdrMerge] Registration too poor for ghost suppression "
                  << "(low: inliers=" << regLow.num_inliers
                  << " rms=" << regLow.rms_error
                  << "; high: inliers=" << regHigh.num_inliers
                  << " rms=" << regHigh.rms_error
                  << "), skip" << std::endl;
        return cv::Mat::zeros(normalCropped.size(), CV_8UC1);
    }

    cv::Mat mask = cv::Mat::zeros(normalCropped.size(), CV_8UC1);
    const std::string& mode = cfg.ghostDetectionMode;

    bool useDiff = (mode == "diff" || mode == "flow+diff");
    bool useFlow = (mode == "flow" || mode == "flow+diff");

    if (!useDiff && !useFlow)
    {
        std::cerr << "[HdrMerge] Unknown ghostDetectionMode=\"" << mode
                  << "\", fallback to diff" << std::endl;
        useDiff = true;
    }

    if (useDiff)
    {
        cv::Mat diffMask = DetectMotionDiff(lowCropped, normalCropped, highCropped, cfg);
        mask = cv::max(mask, diffMask);
    }
    if (useFlow)
    {
        cv::Mat flowMask = DetectMotionLk(lowCropped, normalCropped, highCropped, cfg);
        mask = cv::max(mask, flowMask);
    }

    // 形态学清理
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // 边缘平滑
    const int r = std::max(3, cfg.motionDilateRadius);
    cv::GaussianBlur(mask, mask, cv::Size(2 * r + 1, 2 * r + 1), 0);
    const double nonzeroRatio = static_cast<double>(cv::countNonZero(mask)) / mask.total();
    if (nonzeroRatio > 0.5)
    {
        std::cerr << "[HdrMerge] Motion mask coverage too high ("
                << (nonzeroRatio * 100.0) << "%), skip ghost suppression" << std::endl;
        return cv::Mat::zeros(normalCropped.size(), CV_8UC1);
    }
    return mask;
}

// ============================================================
// 鬼影抑制：Mertens 路径（8UC3 亮度缩放回填）
// ============================================================
inline void ApplyGhostSuppressionMertens(
    cv::Mat& lowCropped,
    cv::Mat& normalCropped,
    cv::Mat& highCropped,
    const cv::Mat& motionMask,
    const HdrConfig& cfg)
{
    if (motionMask.empty()) return;

    cv::Mat gLow, gNormal, gHigh;
    cv::cvtColor(lowCropped,    gLow,    cv::COLOR_BGR2GRAY);
    cv::cvtColor(normalCropped, gNormal, cv::COLOR_BGR2GRAY);
    cv::cvtColor(highCropped,   gHigh,   cv::COLOR_BGR2GRAY);

    const double mN = std::max(ComputeGrayMedian(gNormal), 1.0);
    const double sLow  = std::clamp(ComputeGrayMedian(gLow)  / mN, 0.2, 5.0);
    const double sHigh = std::clamp(ComputeGrayMedian(gHigh) / mN, 0.2, 5.0);

    const float strength = static_cast<float>(
        std::clamp(cfg.ghostSuppressStrength, 0.0, 1.0));

    for (int y = 0; y < motionMask.rows; ++y)
    {
        const uint8_t* m = motionMask.ptr<uint8_t>(y);
        cv::Vec3b* l = lowCropped.ptr<cv::Vec3b>(y);
        cv::Vec3b* h = highCropped.ptr<cv::Vec3b>(y);
        const cv::Vec3b* n = normalCropped.ptr<cv::Vec3b>(y);

        for (int x = 0; x < motionMask.cols; ++x)
        {
            if (m[x] == 0) continue;

            const float alpha = (m[x] / 255.0f) * strength;

            for (int c = 0; c < 3; ++c)
            {
                const uint8_t fillL = cv::saturate_cast<uint8_t>(n[x][c] * sLow);
                const uint8_t fillH = cv::saturate_cast<uint8_t>(n[x][c] * sHigh);
                l[x][c] = cv::saturate_cast<uint8_t>(l[x][c] * (1.0f - alpha) + fillL * alpha);
                h[x][c] = cv::saturate_cast<uint8_t>(h[x][c] * (1.0f - alpha) + fillH * alpha);
            }
        }
    }

    std::cout << "[HdrMerge] Ghost suppression applied (Mertens, scaleLow="
              << sLow << ", scaleHigh=" << sHigh << ")" << std::endl;
}

// ============================================================
// 鬼影抑制：物理路径（32FC3 线性域直接回填 normal 的辐射）
// ============================================================
inline void ApplyGhostSuppressionPhysicalLinear(
    std::vector<cv::Mat>& linearImages,
    const cv::Mat& motionMask,
    const HdrConfig& cfg)
{
    if (motionMask.empty() || linearImages.size() != 3) return;

    const float strength = static_cast<float>(
        std::clamp(cfg.ghostSuppressStrength, 0.0, 1.0));

    cv::Mat& linLow    = linearImages[0];
    const cv::Mat& linNormal = linearImages[1];
    cv::Mat& linHigh   = linearImages[2];

    for (int y = 0; y < motionMask.rows; ++y)
    {
        const uint8_t* m = motionMask.ptr<uint8_t>(y);
        cv::Vec3f* l = linLow.ptr<cv::Vec3f>(y);
        cv::Vec3f* h = linHigh.ptr<cv::Vec3f>(y);
        const cv::Vec3f* n = linNormal.ptr<cv::Vec3f>(y);

        for (int x = 0; x < motionMask.cols; ++x)
        {
            if (m[x] == 0) continue;

            const float alpha = (m[x] / 255.0f) * strength;
            for (int c = 0; c < 3; ++c)
            {
                l[x][c] = l[x][c] * (1.0f - alpha) + n[x][c] * alpha;
                h[x][c] = h[x][c] * (1.0f - alpha) + n[x][c] * alpha;
            }
        }
    }

    std::cout << "[HdrMerge] Ghost suppression applied (Physical linear)" << std::endl;
}

// ============================================================
// 物理 HDR 子流程（新增 motionMask 参数）
// ============================================================
inline int MergePhysicalHdr(
    const cv::Mat& lowCropped,
    const cv::Mat& normalCropped,
    const cv::Mat& highCropped,
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const HdrConfig& cfg,
    const cv::Mat& motionMask,
    cv::Mat& outHdr32f,
    cv::Mat& outLdr8u)
{
    std::vector<float> effectiveExposures =
        ResolveEffectiveExposures(lowPath, normalPath, highPath, cfg);
    if (effectiveExposures.size() != 3)
    {
        std::cerr << "[HdrMerge] Physical HDR: no valid exposure info" << std::endl;
        return 1;
    }

    cv::Mat lut = BuildSrgbDegammaLUT();
    std::vector<cv::Mat> linearImages = {
        SrgbToLinear(lowCropped,    lut),
        SrgbToLinear(normalCropped, lut),
        SrgbToLinear(highCropped,   lut)
    };

    if (!motionMask.empty())
    {
        ApplyGhostSuppressionPhysicalLinear(linearImages, motionMask, cfg);
    }

    try
    {
        cv::Ptr<cv::MergeDebevec> merge = cv::createMergeDebevec();
        if (cfg.calibrateResponse)
        {
            std::cout << "[HdrMerge] CalibrateDebevec..." << std::endl;
            cv::Mat response(256, 1, CV_32FC3);
            cv::Ptr<cv::CalibrateDebevec> calibrate = cv::createCalibrateDebevec();
            calibrate->process(linearImages, response, effectiveExposures);
            merge->process(linearImages, outHdr32f, effectiveExposures, response);
        }
        else
        {
            merge->process(linearImages, outHdr32f, effectiveExposures);
        }
    }
    catch (const cv::Exception& e)
    {
        std::cerr << "[HdrMerge] MergeDebevec exception: " << e.what() << std::endl;
        return 2;
    }
    if (outHdr32f.empty())
    {
        std::cerr << "[HdrMerge] MergeDebevec returned empty" << std::endl;
        return 2;
    }

    outLdr8u = TonemapHdr(outHdr32f, cfg);
    if (outLdr8u.empty())
    {
        std::cerr << "[HdrMerge] Tonemap returned empty" << std::endl;
        return 3;
    }

    return 0;
}

// ============================================================
// 主入口（带 cfg）
// ============================================================
inline int HdrMergeFromFiles(
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const std::string& outDir,
    const HdrConfig& cfg)
{
    // 1. 读图
    cv::Mat low    = cv::imread(lowPath);
    cv::Mat normal = cv::imread(normalPath);
    cv::Mat high   = cv::imread(highPath);

    if (low.empty())    { std::cerr << "[HdrMerge] Cannot read: " << lowPath    << std::endl; return 1; }
    if (normal.empty()) { std::cerr << "[HdrMerge] Cannot read: " << normalPath << std::endl; return 2; }
    if (high.empty())   { std::cerr << "[HdrMerge] Cannot read: " << highPath   << std::endl; return 3; }

    std::cout << "[HdrMerge] low    : " << low.cols    << "x" << low.rows    << std::endl;
    std::cout << "[HdrMerge] normal : " << normal.cols << "x" << normal.rows << std::endl;
    std::cout << "[HdrMerge] high   : " << high.cols   << "x" << high.rows   << std::endl;
    std::cout << "[HdrMerge] pipelineMode = " << cfg.pipelineMode << std::endl;

    // 2. 配准（保留 RegistrationResult 用于鬼影门控）
    ImageRegistrator registrator;
    RegistrationResult regLow;
    RegistrationResult regHigh;

    std::cout << "[HdrMerge] Aligning low -> normal..." << std::endl;
    cv::Mat alignedLow = AlignToReference(normal, low, registrator,
                                          FeatureDetectorType::ORB,
                                          TransformType::HOMOGRAPHY,
                                          &regLow);

    std::cout << "[HdrMerge] Aligning high -> normal..." << std::endl;
    cv::Mat alignedHigh = AlignToReference(normal, high, registrator,
                                           FeatureDetectorType::ORB,
                                           TransformType::HOMOGRAPHY,
                                           &regHigh);

    // 3. ROI 裁剪
    cv::Rect roi = ComputeCommonRoi(alignedLow, alignedHigh, normal, cfg);
    if (roi.area() <= 0)
    {
        std::cerr << "[HdrMerge] Invalid ROI" << std::endl;
        return 6;
    }

    cv::Mat lowCropped    = alignedLow(roi).clone();
    cv::Mat normalCropped = normal(roi).clone();
    cv::Mat highCropped   = alignedHigh(roi).clone();

    std::cout << "[HdrMerge] Cropped size: " << lowCropped.cols << "x"
              << lowCropped.rows << std::endl;

    // 4. 建目录（写原始配准结果需要在融合之前）
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec)
    {
        std::cerr << "[HdrMerge] Cannot create out dir: " << outDir
                  << " (" << ec.message() << ")" << std::endl;
        return 5;
    }

    // 原始配准结果（未回填）
    cv::imwrite(outDir + "/01_low_registered.jpg",   lowCropped);
    cv::imwrite(outDir + "/02_normal_reference.jpg", normalCropped);
    cv::imwrite(outDir + "/03_high_registered.jpg",  highCropped);

    // 5. 运动检测
    cv::Mat motionMask;
    if (cfg.enableGhostSuppression)
    {
        std::cout << "[HdrMerge] Ghost detection (mode="
                  << cfg.ghostDetectionMode << ")..." << std::endl;
        motionMask = BuildMotionMask(lowCropped, normalCropped, highCropped,
                                     regLow, regHigh, cfg);

        if (cfg.saveMotionMask && !motionMask.empty())
        {
            cv::imwrite(outDir + "/06_motion_mask.png", motionMask);
            std::cout << "[HdrMerge] Wrote motion mask: "
                      << outDir << "/06_motion_mask.png" << std::endl;
        }
    }

    // 6. 融合分流
    cv::Mat finalResult;
    cv::Mat hdr32f;
    cv::Mat merged;
    cv::Mat detailEnhanced;
    bool physicalUsed = false;

    if (cfg.pipelineMode == "physical")
    {
        std::cout << "[HdrMerge] Physical HDR pipeline..." << std::endl;
        cv::Mat ldr8u;
        int rcPhys = MergePhysicalHdr(lowCropped, normalCropped, highCropped,
                                      lowPath, normalPath, highPath,
                                      cfg, motionMask, hdr32f, ldr8u);
        if (rcPhys == 0)
        {
            finalResult = ldr8u;
            physicalUsed = true;
            std::cout << "[HdrMerge] Physical HDR succeeded" << std::endl;
        }
        else
        {
            std::cerr << "[HdrMerge] Physical HDR failed (rc=" << rcPhys
                      << "), fallback to Mertens" << std::endl;
        }
    }

    if (!physicalUsed)
    {
        std::cout << "[HdrMerge] Mertens pipeline..." << std::endl;

        // 回填在副本上做，避免污染 01/02/03 的原始结果
        cv::Mat lowFused    = lowCropped.clone();
        cv::Mat normalFused = normalCropped.clone();
        cv::Mat highFused   = highCropped.clone();

        if (!motionMask.empty())
        {
            ApplyGhostSuppressionMertens(lowFused, normalFused, highFused,
                                         motionMask, cfg);
        }

        std::vector<cv::Mat> inputs = { lowFused, normalFused, highFused };
        merged = MergeMertens(inputs, cfg);
        if (merged.empty())
        {
            std::cerr << "[HdrMerge] Mertens failed" << std::endl;
            return 4;
        }

        detailEnhanced = merged;
        if (cfg.enableDetailEnhance)
        {
            std::cout << "[HdrMerge] detailEnhance..." << std::endl;
            cv::detailEnhance(merged, detailEnhanced,
                              static_cast<float>(cfg.detailSigma_s),
                              static_cast<float>(cfg.detailSigma_r));
        }

        finalResult = detailEnhanced;
    }

    // 7. 可选 UnsharpMask（两条路径共用）
    if (cfg.enableUnsharpMask)
    {
        std::cout << "[HdrMerge] Unsharp Mask (amount=" << cfg.unsharpAmount
                  << ", radius=" << cfg.unsharpRadius
                  << ", threshold=" << cfg.unsharpThreshold << ")..." << std::endl;
        finalResult = UnsharpMask(finalResult,
                                  cfg.unsharpAmount,
                                  cfg.unsharpRadius,
                                  cfg.unsharpThreshold);
    }

    // 8. 中间产物
    if (cfg.saveIntermediate)
    {
        if (physicalUsed && !hdr32f.empty())
        {
            std::string hdrPath = outDir + "/04_hdr_linear_32f.hdr";
            if (cv::imwrite(hdrPath, hdr32f))
            {
                std::cout << "[HdrMerge] Wrote linear HDR: " << hdrPath << std::endl;
            }
            else
            {
                std::cerr << "[HdrMerge] Warning: failed to write " << hdrPath << std::endl;
            }
        }
        else if (!merged.empty())
        {
            cv::imwrite(outDir + "/04_hdr_merged_raw.jpg", merged);
        }
    }

    if (!physicalUsed && cfg.enableDetailEnhance && !detailEnhanced.empty())
    {
        cv::imwrite(outDir + "/05_hdr_detail_enhanced.jpg", detailEnhanced);
    }

    cv::imwrite(outDir + "/hdr_merged.jpg", finalResult);

    std::cout << "[HdrMerge] Done. Output: " << outDir
              << " (pipeline=" << (physicalUsed ? "physical" : "mertens")
              << ", ghost=" << (cfg.enableGhostSuppression ? "on" : "off")
              << ")" << std::endl;
    return 0;
}

// 主入口（默认 cfg）
inline int HdrMergeFromFiles(
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const std::string& outDir)
{
    const HdrConfig cfg = HdrConfig::LoadFromFile(HdrConfig::DefaultPath());
    return HdrMergeFromFiles(lowPath, normalPath, highPath, outDir, cfg);
}

} // namespace hdr_merge

#endif // HDR_MERGE_H