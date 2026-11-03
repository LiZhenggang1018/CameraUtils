/*
 * @Description: Simple Image Registration Header
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

enum class TransformType
{
    HOMOGRAPHY,
    AFFINE,
    EUCLIDEAN,
    TRANSLATION
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