/*
 * @Description: HDR 融合（配准 + Mertens / 物理 HDR 双管线）
 */
#ifndef HDR_MERGE_H
#define HDR_MERGE_H

#include "OpenCVCommon.h"
#include "HdrMerge/HdrConfig.h"
#include "LivePhotoSmoothPlay/ImageRegistrator.h"
#include "Exiv2Common.h"
#include "CommonEnv.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace hdr_merge {

// ============================================================
// 配准（不变）
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
// 物理 HDR：sRGB De-gamma LUT
//   输入 8-bit sRGB，输出 [0,1] 线性浮点
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

// 单通道 LUT 对多通道输入逐通道应用，输出 CV_32FC3
inline cv::Mat SrgbToLinear(const cv::Mat& bgr8u, const cv::Mat& lut)
{
    cv::Mat linear;
    cv::LUT(bgr8u, lut, linear);
    return linear;
}

// ============================================================
// 物理 HDR：解析等效曝光量
//   effectiveExposure = timeSeconds * iso / baseIso
//   - EXIF 三帧齐全 → 用 EXIF
//   - 否则若手动配置了三对 time/iso → 用手动值
//   - 都不可用 → 返回空 vector，调用方回退 Mertens
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
// 物理 HDR：色调映射
//   hdr32f: CV_32FC3 线性辐射图 → CV_8UC3 LDR
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
// 物理 HDR：完整子流程
//   返回 0 = 成功；1/2/3 = 曝光信息/MergeDebevec/Tonemap 失败
// ============================================================
inline int MergePhysicalHdr(
    const cv::Mat& lowCropped,
    const cv::Mat& normalCropped,
    const cv::Mat& highCropped,
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const HdrConfig& cfg,
    cv::Mat& outHdr32f,
    cv::Mat& outLdr8u)
{
    // 1. 等效曝光量
    std::vector<float> effectiveExposures =
        ResolveEffectiveExposures(lowPath, normalPath, highPath, cfg);
    if (effectiveExposures.size() != 3)
    {
        std::cerr << "[HdrMerge] Physical HDR: no valid exposure info" << std::endl;
        return 1;
    }

    // 2. De-gamma 到线性域
    cv::Mat lut = BuildSrgbDegammaLUT();
    std::vector<cv::Mat> linearImages = {
        SrgbToLinear(lowCropped,    lut),
        SrgbToLinear(normalCropped, lut),
        SrgbToLinear(highCropped,   lut)
    };

    // 3. MergeDebevec
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

    // 4. 色调映射
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

    // 2. 配准
    ImageRegistrator registrator;
    std::cout << "[HdrMerge] Aligning low -> normal..." << std::endl;
    cv::Mat alignedLow = AlignToReference(normal, low, registrator);

    std::cout << "[HdrMerge] Aligning high -> normal..." << std::endl;
    cv::Mat alignedHigh = AlignToReference(normal, high, registrator);

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

    // 4. 融合分流
    cv::Mat finalResult;
    cv::Mat hdr32f;          // 物理 HDR 线性辐射图
    cv::Mat merged;          // Mertens 结果（仅 mertens 路径）
    cv::Mat detailEnhanced;  // detailEnhance 结果（仅 mertens 路径）
    bool physicalUsed = false;

    if (cfg.pipelineMode == "physical")
    {
        std::cout << "[HdrMerge] Physical HDR pipeline..." << std::endl;
        cv::Mat ldr8u;
        int rcPhys = MergePhysicalHdr(lowCropped, normalCropped, highCropped,
                                      lowPath, normalPath, highPath,
                                      cfg, hdr32f, ldr8u);
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
        std::vector<cv::Mat> inputs = { lowCropped, normalCropped, highCropped };
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

    // 5. 可选 UnsharpMask（两条路径共用）
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

    // 6. 保存
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
        if (physicalUsed && !hdr32f.empty())
        {
            // 物理 HDR：写 Radiance HDR（32F 线性辐射图）
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
              << " (pipeline=" << (physicalUsed ? "physical" : "mertens") << ")"
              << std::endl;
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