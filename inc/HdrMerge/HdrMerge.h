/*
 * @Description: HDR 融合（配准 + Mertens 曝光融合）
 *
 * 输入：低曝光 / 正常曝光 / 高曝光 三张同场景照片
 * 输出：对齐后的三张图 + 一张 HDR 融合图
 *
 * 算法：
 *   1. 以 normal 为参考图，用 ImageRegistrator 把 low / high 对齐到 normal
 *   2. 用 cv::MergeMertens（拉普拉斯金字塔曝光融合）合成 HDR
 *   3. MergeMertens 不需要相机的曝光时间（EV），直接对图像做质量评估
 */

#ifndef HDR_MERGE_H
#define HDR_MERGE_H

#include "OpenCVCommon.h"
#include "LivePhotoSmoothPlay/ImageRegistrator.h"
#include "CommonEnv.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace hdr_merge {

// ------------------------------------------------------------
// 把 target 对齐到 reference 坐标系
// 失败时返回 target 的克隆（保证下游尺寸一致，避免崩溃）
// ------------------------------------------------------------
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
            std::cerr << "[HdrMerge] Align failed (empty registered_image), use original target"
                      << std::endl;
            // 回退：把 target 缩放到 reference 尺寸，保证三张图同尺寸
            cv::Mat fallback;
            cv::resize(target, fallback, reference.size());
            return fallback;
        }

        // 质量检查
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

// ------------------------------------------------------------
// Mertens 曝光融合
//   输入：vector<cv::Mat>（CV_8UC3 或 CV_32FC3）
//   输出：CV_8UC3 [0,255]
// ------------------------------------------------------------
inline cv::Mat MergeMertens(const std::vector<cv::Mat>& images)
{
    if (images.empty())
    {
        return {};
    }

    cv::Ptr<cv::MergeMertens> merger = cv::createMergeMertens();

    // 可调权重：对比度 / 饱和度 / 良好曝光度（默认 1.0）
    //   提高 contrast_weight   → 更锐利，但可能过曝
    //   提高 saturation_weight → 色彩更艳
    //   提高 exposure_weight   → 更均衡曝光
    // merger->setContrastWeight(1.0f);
    // merger->setSaturationWeight(1.0f);
    // merger->setExposureWeight(1.0f);

    cv::Mat fused;                 // CV_32FC3, 范围 [0, 1]
    merger->process(images, fused);

    cv::Mat fused8u;
    fused.convertTo(fused8u, CV_8UC3, 255.0);
    return fused8u;
}

// ------------------------------------------------------------
// 主入口：从 3 个文件路径生成 HDR 融合图
// ------------------------------------------------------------
inline int HdrMergeFromFiles(
    const std::string& lowPath,
    const std::string& normalPath,
    const std::string& highPath,
    const std::string& outDir)
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

    // 3. Mertens 曝光融合（顺序：低曝光 → 正常 → 高曝光）
    std::cout << "[HdrMerge] Merging Mertens..." << std::endl;
    std::vector<cv::Mat> inputs = { alignedLow, normal, alignedHigh };
    cv::Mat merged = MergeMertens(inputs);

    if (merged.empty())
    {
        std::cerr << "[HdrMerge] Mertens failed" << std::endl;
        return 4;
    }

    // 4. 保存
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec)
    {
        std::cerr << "[HdrMerge] Cannot create out dir: " << outDir
                  << " (" << ec.message() << ")" << std::endl;
        return 5;
    }

    cv::imwrite(outDir + "/01_low_registered.jpg",  alignedLow);
    cv::imwrite(outDir + "/02_normal_reference.jpg", normal);
    cv::imwrite(outDir + "/03_high_registered.jpg", alignedHigh);
    cv::imwrite(outDir + "/hdr_merged.jpg",         merged);

    std::cout << "[HdrMerge] Done. Output: " << outDir << std::endl;
    return 0;
}

} // namespace hdr_merge

#endif // HDR_MERGE_H