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

#ifndef IMAGEREGISTRATOR_H_
#define IMAGEREGISTRATOR_H_

#include "OpenCVCommon.h"

enum class FeatureDetectorType
{
    ORB,   // ORB是免费的特征检测器
    BRISK, // BRISK也是免费的
    KAZE,  // KAZE是主库中的免费算法
    AKAZE  // AKAZE也是主库中的免费算法
};

enum class TransformType {
    HOMOGRAPHY,   // 默认，8 自由度，3×3
    AFFINE,       // 6 自由度，2×3
    EUCLIDEAN,    // 4 自由度（旋转+平移），2×3
    TRANSLATION   // 2 自由度（纯平移），3×3
};

struct RegistrationResult
{
    cv::Mat registered_image {};
    cv::Mat transform_matrix {};
    std::vector<cv::KeyPoint> keypoints1 {};
    std::vector<cv::KeyPoint> keypoints2 {};
    std::vector<cv::DMatch> good_matches {};
    int num_inliers {};
    double rms_error {};
};

class ImageRegistrator
{
public:
    ImageRegistrator() = default;
    ~ImageRegistrator() = default;

    /**
     * @brief 配准两张图像
     * @param reference_img 参考图像
     * @param target_img 待配准图像
     * @param detector_type 特征检测器类型
     * @param transform_type 变换类型
     * @return 配准结果
     */
    RegistrationResult registerImages(
        const cv::Mat &reference_img,
        const cv::Mat &target_img,
        FeatureDetectorType detector_type = FeatureDetectorType::ORB,
        TransformType transform_type = TransformType::HOMOGRAPHY);

    /**
     * @brief 绘制匹配结果
     * @param img1 第一张图像
     * @param img2 第二张图像
     * @param keypoints1 第一张图像的特征点
     * @param keypoints2 第二张图像的特征点
     * @param matches 匹配点对
     * @return 绘制好的图像
     */
    cv::Mat drawMatches(
        const cv::Mat &img1,
        const cv::Mat &img2,
        const std::vector<cv::KeyPoint> &keypoints1,
        const std::vector<cv::KeyPoint> &keypoints2,
        const std::vector<cv::DMatch> &matches);

    /**
     * @brief 保存配准结果
     * @param result 配准结果
     * @param output_dir 输出目录
     */
    void saveRegistrationResults(
        const RegistrationResult &result,
        const std::string &output_dir);

private:
    /**
     * @brief 检测特征点和描述符
     */
    std::tuple<std::vector<cv::KeyPoint>, cv::Mat> detectFeatures(
        const cv::Mat &image,
        FeatureDetectorType detector_type);

    /**
     * @brief 匹配特征点
     */
    std::vector<cv::DMatch> matchFeatures(
        const cv::Mat &descriptors1,
        const cv::Mat &descriptors2,
        FeatureDetectorType detector_type);

    /**
     * @brief 计算变换矩阵
     */
    cv::Mat estimateTransform(
        const std::vector<cv::KeyPoint> &keypoints1,
        const std::vector<cv::KeyPoint> &keypoints2,
        const std::vector<cv::DMatch> &matches,
        TransformType transform_type,
        std::vector<cv::DMatch> &inlier_matches);

    /**
     * @brief 应用变换矩阵
     */
    cv::Mat applyTransform(
        const cv::Mat &image,
        const cv::Mat &transform,
        const cv::Size &output_size);

    // 创建特征检测器
    cv::Ptr<cv::Feature2D> createDetector(FeatureDetectorType type);
};

#endif // IMAGEREGISTRATOR_H_