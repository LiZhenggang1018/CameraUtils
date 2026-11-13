/*
 * @Description: HDR 融合（配准 + Mertens 曝光融合）
 *
 * 实现功能：
 *   1. Mertens 权重可配置（对比度 / 饱和度 / 曝光度）
 *   2. 配准后自动检测黑边并裁剪
 *   3. Unsharp Mask 后处理锐化
 *   4. （可选）detailEnhance 局部对比度增强
 *
 * 配置来源：configs/HdrConfig.json（见 HdrConfig.h）
 */

#ifndef HDR_MERGE_H
#define HDR_MERGE_H

#include "OpenCVCommon.h"
#include "HdrMerge/HdrConfig.h"
#include "LivePhotoSmoothPlay/ImageRegistrator.h"
#include "CommonEnv.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace hdr_merge {

// ============================================================
// 把 target 对齐到 reference 坐标系
// 失败时返回 resize 后的 target（保证下游尺寸一致）
// ============================================================
inline cv::Mat AlignToReference(
    const cv::Mat& reference,
    const cv::Mat& target,
    ImageRegistrator& registrator,
    FeatureDetectorType detector = FeatureDetectorType::ORB,
    TransformType transform = TransformType::HOMOGRAPHY)
{
    try
    {
        auto result = registrator.registerImages(reference, target, detector, transform);

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
        cv::Mat fallback;
        cv::resize(target, fallback, reference.size());
        return fallback;
    }
}

// ============================================================
// 检测图像中的"有效内容区域"（去掉 warp 后的黑边）
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
// 计算三张图的公共 ROI（有效内容区域）
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
// Unsharp Mask 锐化
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
// Mertens 曝光融合（带权重配置）
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

    cv::Mat fused;                // CV_32FC3, [0, 1]
    merger->process(images, fused);

    cv::Mat fused8u;
    fused.convertTo(fused8u, CV_8UC3, 255.0);
    return fused8u;
}

// ============================================================
// 主入口（带 config 版本）
// ============================================================
inline int HdrMergeFromFiles(
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const std::string& outDir,
    const HdrConfig& cfg)
{
    // 1. 读三张图
    cv::Mat low    = cv::imread(lowPath);
    cv::Mat normal = cv::imread(normalPath);
    cv::Mat high   = cv::imread(highPath);

    if (low.empty())    { std::cerr << "[HdrMerge] Cannot read: " << lowPath    << std::endl; return 1; }
    if (normal.empty()) { std::cerr << "[HdrMerge] Cannot read: " << normalPath << std::endl; return 2; }
    if (high.empty())   { std::cerr << "[HdrMerge] Cannot read: " << highPath   << std::endl; return 3; }

    std::cout << "[HdrMerge] low    : " << low.cols    << "x" << low.rows    << std::endl;
    std::cout << "[HdrMerge] normal : " << normal.cols << "x" << normal.rows << std::endl;
    std::cout << "[HdrMerge] high   : " << high.cols   << "x" << high.rows   << std::endl;

    // 2. 以 normal 为参考，对齐 low / high
    ImageRegistrator registrator;

    std::cout << "[HdrMerge] Aligning low -> normal..." << std::endl;
    cv::Mat alignedLow = AlignToReference(normal, low, registrator);

    std::cout << "[HdrMerge] Aligning high -> normal..." << std::endl;
    cv::Mat alignedHigh = AlignToReference(normal, high, registrator);

    // 3. 计算公共 ROI 并裁剪（修复黑边）
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

    // 4. Mertens 融合
    std::cout << "[HdrMerge] Merging Mertens..." << std::endl;
    std::vector<cv::Mat> inputs = { lowCropped, normalCropped, highCropped };
    cv::Mat merged = MergeMertens(inputs, cfg);

    if (merged.empty())
    {
        std::cerr << "[HdrMerge] Mertens failed" << std::endl;
        return 4;
    }

    // 5. 可选 detailEnhance
    cv::Mat detailEnhanced = merged;
    if (cfg.enableDetailEnhance)
    {
        std::cout << "[HdrMerge] detailEnhance..." << std::endl;
        cv::detailEnhance(merged, detailEnhanced,
                          static_cast<float>(cfg.detailSigma_s),
                          static_cast<float>(cfg.detailSigma_r));
    }

    // 6. 可选 Unsharp Mask
    cv::Mat finalResult = detailEnhanced;
    if (cfg.enableUnsharpMask)
    {
        std::cout << "[HdrMerge] Unsharp Mask (amount=" << cfg.unsharpAmount
                  << ", radius=" << cfg.unsharpRadius
                  << ", threshold=" << cfg.unsharpThreshold << ")..." << std::endl;
        finalResult = UnsharpMask(detailEnhanced,
                                  cfg.unsharpAmount,
                                  cfg.unsharpRadius,
                                  cfg.unsharpThreshold);
    }

    // 7. 保存
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec)
    {
        std::cerr << "[HdrMerge] Cannot create out dir: " << outDir
                  << " (" << ec.message() << ")" << std::endl;
        return 5;
    }

    cv::imwrite(outDir + "/01_low_registered.jpg",   lowCropped);
    cv::imwrite(outDir + "/02_normal_reference.jpg", normalCropped);
    cv::imwrite(outDir + "/03_high_registered.jpg",  highCropped);

    if (cfg.saveIntermediate)
    {
        cv::imwrite(outDir + "/04_hdr_merged_raw.jpg", merged);
    }

    if (cfg.enableDetailEnhance)
    {
        cv::imwrite(outDir + "/05_hdr_detail_enhanced.jpg", detailEnhanced);
    }

    cv::imwrite(outDir + "/hdr_merged.jpg", finalResult);

    std::cout << "[HdrMerge] Done. Output: " << outDir << std::endl;
    return 0;
}

// ============================================================
// 主入口（默认 config 版本，从 JSON 读）
// ============================================================
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