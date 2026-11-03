/*
 * @Description: Simple Image Registration Source File
 */

#include "ImageRegistrator.h"

cv::Ptr<cv::Feature2D> ImageRegistrator::createDetector(FeatureDetectorType type)
{
    switch (type)
    {
    case FeatureDetectorType::ORB:
        return cv::ORB::create(5000); // 增加特征点数量
    case FeatureDetectorType::BRISK:
        return cv::BRISK::create();
    case FeatureDetectorType::KAZE:
        return cv::KAZE::create();
    case FeatureDetectorType::AKAZE:
        return cv::AKAZE::create();
    default:
        return cv::ORB::create();
    }
}

std::tuple<std::vector<cv::KeyPoint>, cv::Mat>
ImageRegistrator::detectFeatures(const cv::Mat &image, FeatureDetectorType detector_type)
{
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    auto detector = createDetector(detector_type);
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;

    detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);

    return {keypoints, descriptors};
}

std::vector<cv::DMatch>
ImageRegistrator::matchFeatures(const cv::Mat &descriptors1, const cv::Mat &descriptors2, FeatureDetectorType detector_type)
{
    if (descriptors1.empty() || descriptors2.empty())
    {
        return {};
    }

    // 根据描述符类型选择匹配器
    cv::Ptr<cv::DescriptorMatcher> matcher;
    bool use_flann = false;

    // ORB和BRISK使用二进制描述符，需要汉明距离
    if (detector_type == FeatureDetectorType::ORB || detector_type == FeatureDetectorType::BRISK)
    {
        matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
        use_flann = false;
    }
    else
    { // KAZE和AKAZE使用浮点描述符
        matcher = cv::BFMatcher::create(cv::NORM_L2);
        use_flann = true;
    }

    // 尝试使用FLANN（对于浮点描述符）
    if (use_flann && !descriptors1.empty() && !descriptors2.empty())
    {
        try
        {
            cv::Ptr<cv::flann::IndexParams> indexParams;
            if (descriptors1.type() == CV_32F)
            {
                indexParams = cv::makePtr<cv::flann::KDTreeIndexParams>(5);
            }
            else
            {
                indexParams = cv::makePtr<cv::flann::LshIndexParams>(20, 10, 2);
            }

            cv::Ptr<cv::flann::SearchParams> searchParams = cv::makePtr<cv::flann::SearchParams>(50);

            matcher = cv::makePtr<cv::FlannBasedMatcher>(indexParams, searchParams);
        }
        catch (...)
        {
            // 如果FLANN失败，回退到暴力匹配
            matcher = cv::BFMatcher::create(cv::NORM_L2);
        }
    }

    // 进行KNN匹配
    std::vector<std::vector<cv::DMatch>> knn_matches;
    matcher->knnMatch(descriptors1, descriptors2, knn_matches, 2);

    // 应用Lowe's ratio test
    std::vector<cv::DMatch> good_matches;
    const float ratio_thresh = 0.75f;

    for (const auto &knn_match : knn_matches)
    {
        if (knn_match.size() == 2)
        {
            if (knn_match[0].distance < ratio_thresh * knn_match[1].distance)
            {
                good_matches.push_back(knn_match[0]);
            }
        }
    }

    return good_matches;
}

cv::Mat ImageRegistrator::estimateTransform(
    const std::vector<cv::KeyPoint> &keypoints1,
    const std::vector<cv::KeyPoint> &keypoints2,
    const std::vector<cv::DMatch> &matches,
    TransformType transform_type,
    std::vector<cv::DMatch> &inlier_matches)
{

    if (matches.size() < 4)
    {
        throw std::runtime_error("匹配点不足4个，无法估计变换矩阵");
    }

    // 提取匹配点坐标
    std::vector<cv::Point2f> points1, points2;
    for (const auto &match : matches)
    {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    cv::Mat transform;
    std::vector<uchar> inliers_mask;

    try
    {
        switch (transform_type)
        {
        case TransformType::HOMOGRAPHY:
        {
            transform = cv::findHomography(points1, points2, cv::RANSAC, 3.0, inliers_mask, 2000, 0.995);
            break;
        }
        case TransformType::AFFINE:
        {
            transform = cv::estimateAffine2D(points1, points2, inliers_mask, cv::RANSAC, 3.0, 2000, 0.995);
            // 转换为3x3矩阵
            if (!transform.empty())
            {
                cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
