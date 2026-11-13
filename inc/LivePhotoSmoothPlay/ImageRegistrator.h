/*
ImageRegistrator 是 LivePhotoSmoothPlay 模块的图像配准引擎
——输入两张图（参考图 + 目标图），输出"目标图对齐到参考图"的结果，以及它们之间的变换矩阵。

流程图
┌─────────────────────────────────────────────────────────────┐
│ 1. detectFeatures(reference_img)  → keypoints1, descriptors1 │
│ 2. detectFeatures(target_img)     → keypoints2, descriptors2 │
├─────────────────────────────────────────────────────────────┤
│ 3. matchFeatures(descriptors1, descriptors2)                 │
│      → knnMatch(k=2) → Lowe's ratio 0.75 → good_matches      │
├─────────────────────────────────────────────────────────────┤
│ 4. estimateTransform(keypoints1, keypoints2, good_matches)   │
│      → findHomography(RANSAC) → transform (reference→target) │
│      → inlier_matches（RANSAC 内点）                          │
├─────────────────────────────────────────────────────────────┤
│ 5. cv::invert(transform, warp_to_ref, DECOMP_SVD)            │
│      → warp_to_ref（target→reference）                       │
├─────────────────────────────────────────────────────────────┤
│ 6. applyTransform(target_img, warp_to_ref, reference.size()) │
│      → registered_image                                     │
├─────────────────────────────────────────────────────────────┤
│ 7. 对每个 inlier 计算 RMS 误差                               │
│      → rms_error                                            │
└─────────────────────────────────────────────────────────────┘

在工程中的调用链
ImageRegistrator被 LpReg.h 的 RegCoverFrameToLastVidFrame 封装调用

LpFovTransitionOpt.h::GenFromLpToOptLp()
  └─ RegCoverToVid()                                    ← LpReg.h
       └─ RegCoverFrameToLastVidFrame(720, 1584,
              LAST_VID_FRAME_YUV,     ← 参考图 = 最后视频帧
              COVER_FRAME_YUV,        ← 目标图 = 封面帧
              REG_RESULT_DIR)         ← 输出目录
            ├─ YUVImageReader::readYUV420P × 2         ← YUV → BGR
            ├─ ImageRegistrator::registerImages(       ← ★ 本文档的主角
            │       reference_img = 视频帧,
            │       target_img    = 封面帧,
            │       ORB, HOMOGRAPHY)
            │     └─ 内部 7 步流程
            ├─ g_hMatrixVidToCover = result.transform_matrix.clone()
            ├─ DumpXYScaleFroMatrixVidToCover()         ← 从 H 矩阵取 (0,0) / (1,1) → txt
            └─ saveRegistrationResults + 8 个调试 jpg
*/

/*
 * @Description: Simple Image Registration Header
 */

#ifndef IMAGEREGISTRATOR_H_
#define IMAGEREGISTRATOR_H_

#include "OpenCVCommon.h"

#include <unordered_map>

// ============================================================
// 特征检测器类型
//   - 全部免费 / 主库可用（不依赖 xfeatures2d）
//   - SIFT 从 OpenCV 4.4 起在主库 features2d，如需高精度可扩展
// ============================================================
enum class FeatureDetectorType
{
    ORB,   // 默认；5000 个特征点；二值描述符 → NORM_HAMMING
    SIFT,
    BRISK, // 二值描述符 → NORM_HAMMING
    KAZE,  // 浮点描述符 → NORM_L2 / FLANN
    AKAZE  // 默认二值描述符 → NORM_HAMMING
};

// ============================================================
// 变换类型
//   - HOMOGRAPHY：8 自由度，3×3，默认
//   - AFFINE   ：6 自由度，内部补成 3×3 统一处理
//   - EUCLIDEAN：4 自由度，内部补成 3×3
//   - TRANSLATION：2 自由度，中位数平移，不做 RANSAC
// ============================================================
enum class TransformType
{
    HOMOGRAPHY,
    AFFINE,
    EUCLIDEAN,
    TRANSLATION
};

// ============================================================
// 配准结果
//
// 【重要约定】（避免误用）
//
//   1. transform_matrix 的方向是 reference → target：
//        points2 ≈ transform_matrix * points1
//      要把 target 对齐到 reference 坐标系，必须求逆后 warp。
//      ✅ 直接用 registered_image 即可——内部已求逆并 warp，无需再处理。
//      ❌ 不要拿 transform_matrix 直接 warpPerspective(target, ...)，
//         那会把 target warp 到错误方向。
//
//   2. rms_error 是"内点的重投影均方根误差"（像素），
//      不是"两张图的像素级差异"。即使两图完全对齐，rms_error
//      也可能因特征点定位误差而 > 0.5。
//
//   3. num_inliers 是 RANSAC 内点数。判断标准（720×1584 场景参考）：
//        > 500 + rms < 2.0  → 完美
//        200~500 + rms < 3.0 → 可靠
//        50~200  + rms < 5.0 → 可用，建议检查 overlay_result.jpg
//        < 50               → 不可靠，应拒绝使用
//
//   4. good_matches 是 Lowe's ratio 过滤后的匹配（可能含误匹配）；
//      inlier_matches 是 RANSAC 验证过的真正好匹配。调试时优先看后者。
//
//   5. 坐标精度：内部使用 cv::Point2f（float）。720×1584 下足够；
//      若换 4K 分辨率且要求亚像素精度，可扩展为 Point2d。
// ============================================================
struct RegistrationResult
{
    cv::Mat registered_image {};         // target 对齐到 reference 的结果（已求逆 warp）
    cv::Mat transform_matrix {};         // ⚠️ reference → target 方向
    std::vector<cv::KeyPoint> keypoints1 {};
    std::vector<cv::KeyPoint> keypoints2 {};
    std::vector<cv::DMatch> good_matches {};    // Lowe's ratio 过滤后（含误匹配）
    std::vector<cv::DMatch> inlier_matches {};  // RANSAC 验证后的内点（更可信）
    int num_inliers {};                  // = inlier_matches.size()
    double rms_error {};                 // 内点重投影均方根误差（像素）
};

// ============================================================
// 图像配准器
//
// 无状态（除检测器缓存外）；一行调用：
//     ImageRegistrator reg;
//     auto result = reg.registerImages(ref, tgt, ORB, HOMOGRAPHY);
// ============================================================
class ImageRegistrator
{
public:
    ImageRegistrator() = default;
    ~ImageRegistrator() = default;

    /**
     * @brief 配准两张图像
     * @param reference_img 参考图像（BGR/BGRA/GRAY 均可）
     * @param target_img    待配准图像（同 reference 尺寸）
     * @param detector_type 特征检测器类型
     * @param transform_type 变换类型
     * @return 配准结果（见 RegistrationResult 注释）
     *
     * @note registered_image 是"target 对齐到 reference"的结果，可直接使用。
     * @note transform_matrix 是 reference→target，需用 registered_image 或自行求逆。
     */
    RegistrationResult registerImages(
        const cv::Mat &reference_img,
        const cv::Mat &target_img,
        FeatureDetectorType detector_type = FeatureDetectorType::ORB,
        TransformType transform_type = TransformType::HOMOGRAPHY);

    /**
     * @brief 绘制匹配结果可视化
     */
    cv::Mat drawMatches(
        const cv::Mat &img1,
        const cv::Mat &img2,
        const std::vector<cv::KeyPoint> &keypoints1,
        const std::vector<cv::KeyPoint> &keypoints2,
        const std::vector<cv::DMatch> &matches);

    /**
     * @brief 保存配准结果到目录：
     *        - registered_image.jpg
     *        - transform_matrix.txt
     *        - matches.txt（good_matches，兼容旧工具）
     *        - inlier_matches.txt（RANSAC 内点，精确调试用）
     */
    void saveRegistrationResults(
        const RegistrationResult &result,
        const std::string &output_dir);

private:
    std::tuple<std::vector<cv::KeyPoint>, cv::Mat> detectFeatures(
        const cv::Mat &image,
        FeatureDetectorType detector_type);

    std::vector<cv::DMatch> matchFeatures(
        const cv::Mat &descriptors1,
        const cv::Mat &descriptors2,
        FeatureDetectorType detector_type);

    cv::Mat estimateTransform(
        const std::vector<cv::KeyPoint> &keypoints1,
        const std::vector<cv::KeyPoint> &keypoints2,
        const std::vector<cv::DMatch> &matches,
        TransformType transform_type,
        std::vector<cv::DMatch> &inlier_matches);

    cv::Mat applyTransform(
        const cv::Mat &image,
        const cv::Mat &transform,
        const cv::Size &output_size);

    // 修复坑 5：检测器缓存（首次创建后复用，避免每次 detectFeatures 都 new）
    // 注：mutable 允许在 const 方法里更新；当前单线程使用，若将来多线程共享
    //     同一 ImageRegistrator 实例，需要加锁或改为每线程独立实例。
    cv::Ptr<cv::Feature2D> createDetector(FeatureDetectorType type) const;
    mutable std::unordered_map<int, cv::Ptr<cv::Feature2D>> m_detectorCache;
};


        /**
 * @brief 对两个 YUV420P 文件做配准，结果保存到 output_dir
 * @param curYuvFile  待配准的 YUV（目标图）
 * @param refYuvFile  参考 YUV（参考图）
 * @param output_dir  输出目录
 * @param width       YUV 宽（默认 720，对应 YUV_W）
 * @param height      YUV 高（默认 1584，对应 YUV_H）
 * @param detector    特征检测器（默认 ORB）
 * @param transform   变换类型（默认 HOMOGRAPHY）
 * @return 0 成功，非 0 失败
 */
int RegYuv(const std::string& curYuvFile,
           const std::string& refYuvFile,
           const std::string& output_dir,
           int width = 720,
           int height = 1584,
           FeatureDetectorType detector = FeatureDetectorType::ORB,
           TransformType transform = TransformType::HOMOGRAPHY);

/**
 * @brief 对两个图像文件做配准，结果保存到 output_dir
 *
 * 支持格式：
 *   - cv::imread 支持的格式：jpg / png / bmp / tiff 等
 *   - Windows WIC 支持的格式：heif / heic / avif 等（需系统安装对应解码器）
 *
 * 读取策略：先尝试 cv::imread，失败时降级到 WIC
 */
int RegImg(const std::string& curImgFile,
           const std::string& refImgFile,
           const std::string& output_dir,
           FeatureDetectorType detector = FeatureDetectorType::ORB,
           TransformType transform = TransformType::HOMOGRAPHY);

#endif // IMAGEREGISTRATOR_H_