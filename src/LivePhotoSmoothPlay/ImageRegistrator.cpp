#include "ImageRegistrator.h"
#include "YUVImageReader.h"
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincodec.h>
#include <objbase.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

cv::Ptr<cv::Feature2D> ImageRegistrator::createDetector(FeatureDetectorType type) const
{
    const int key = static_cast<int>(type);
    auto it = m_detectorCache.find(key);
    if (it != m_detectorCache.end())
    {
        return it->second;
    }

    cv::Ptr<cv::Feature2D> detector;
    switch (type)
    {
    case FeatureDetectorType::ORB:
        detector = cv::ORB::create(5000);   // 5000 个点，保证 RANSAC 有足够内点
        break;
    case FeatureDetectorType::SIFT:      //
        detector = cv::SIFT::create(5000);
        break;
    case FeatureDetectorType::BRISK:
        detector = cv::BRISK::create();
        break;
    case FeatureDetectorType::KAZE:
        detector = cv::KAZE::create();
        break;
    case FeatureDetectorType::AKAZE:
        detector = cv::AKAZE::create();
        break;
    default:
        detector = cv::ORB::create(5000);
        break;
    }

    m_detectorCache[key] = detector;
    return detector;
}

std::tuple<std::vector<cv::KeyPoint>, cv::Mat>
ImageRegistrator::detectFeatures(const cv::Mat &image, FeatureDetectorType detector_type)
{
    // 修复坑 6（兼容性）：支持 BGR / BGRA / GRAY 三种输入
    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image;
    }
    else if (image.channels() == 3)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        throw std::runtime_error("不支持的图像通道数: " + std::to_string(image.channels()));
    }

    auto detector = createDetector(detector_type);   // 缓存命中
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;

    detector->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);

    return {keypoints, descriptors};
}

std::vector<cv::DMatch>
ImageRegistrator::matchFeatures(const cv::Mat &descriptors1, const cv::Mat &descriptors2,
                                FeatureDetectorType detector_type)
{
    if (descriptors1.empty() || descriptors2.empty())
    {
        return {};
    }

    cv::Ptr<cv::DescriptorMatcher> matcher;
    bool use_flann = false;

    // ORB/BRISK/AKAZE 默认二值描述符 → NORM_HAMMING
    // KAZE 是浮点描述符 → NORM_L2
    if (detector_type == FeatureDetectorType::ORB ||
        detector_type == FeatureDetectorType::BRISK ||
        detector_type == FeatureDetectorType::AKAZE)
    {
        matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
        use_flann = false;
    }
    else
    {
        matcher = cv::BFMatcher::create(cv::NORM_L2);
        use_flann = true;
    }

    // 浮点描述符尝试 FLANN（快 2~3 倍），失败回退 BFMatcher
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
            matcher = cv::BFMatcher::create(cv::NORM_L2);   // 回退
        }
    }

    // KNN + Lowe's ratio test（0.75 是经典阈值）
    std::vector<std::vector<cv::DMatch>> knn_matches;
    matcher->knnMatch(descriptors1, descriptors2, knn_matches, 2);

    std::vector<cv::DMatch> good_matches;
    good_matches.reserve(knn_matches.size());

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

    // 提取匹配点坐标（Point2f，720×1584 下足够精度）
    std::vector<cv::Point2f> points1, points2;
    points1.reserve(matches.size());
    points2.reserve(matches.size());
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
            transform = cv::findHomography(points1, points2, cv::RANSAC, 3.0, inliers_mask, 2000, 0.995);
            break;

        case TransformType::AFFINE:
            transform = cv::estimateAffine2D(points1, points2, inliers_mask, cv::RANSAC, 3.0, 2000, 0.995);
            if (!transform.empty())
            {
                cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
                transform.copyTo(h.rowRange(0, 2));
                h.at<double>(2, 2) = 1.0;
                transform = h;
            }
            break;

        case TransformType::EUCLIDEAN:
        {
            cv::Mat rigid_transform = cv::estimateAffinePartial2D(points1, points2, inliers_mask, cv::RANSAC, 3.0);
            if (!rigid_transform.empty())
            {
                cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
                rigid_transform.copyTo(h.rowRange(0, 2));
                h.at<double>(2, 2) = 1.0;
                transform = h;
            }
            break;
        }

        case TransformType::TRANSLATION:
        {
            // 中位数平移，不做 RANSAC，所有匹配都算内点
            std::vector<float> dx, dy;
            dx.reserve(points1.size());
            dy.reserve(points1.size());
            for (size_t i = 0; i < points1.size(); ++i)
            {
                dx.push_back(points2[i].x - points1[i].x);
                dy.push_back(points2[i].y - points1[i].y);
            }
            auto mid = dx.size() / 2;
            std::nth_element(dx.begin(), dx.begin() + mid, dx.end());
            std::nth_element(dy.begin(), dy.begin() + mid, dy.end());
            float tx = dx[mid];
            float ty = dy[mid];

            transform = (cv::Mat_<double>(3, 3) << 1, 0, tx, 0, 1, ty, 0, 0, 1);
            inliers_mask.assign(points1.size(), 1);
            break;
        }
        }
    }
    catch (const cv::Exception &e)
    {
        std::cerr << "OpenCV异常: " << e.what() << std::endl;
        throw std::runtime_error("估计变换矩阵失败");
    }

    // 修复（之前 #10）：inliers_mask 空检查，避免越界
    if (transform.empty() || inliers_mask.size() != matches.size())
    {
        std::cerr << "Warning: estimateTransform failed or inliers_mask size mismatch"
                  << " (matches=" << matches.size()
                  << ", mask=" << inliers_mask.size() << ")" << std::endl;
        inlier_matches.clear();
        return cv::Mat();
    }

    // 提取内点匹配（修复坑 4：现在通过引用传回，最终存进 RegistrationResult::inlier_matches）
    inlier_matches.clear();
    inlier_matches.reserve(matches.size());
    for (size_t i = 0; i < matches.size(); ++i)
    {
        if (inliers_mask[i])
        {
            inlier_matches.push_back(matches[i]);
        }
    }

    return transform;
}

cv::Mat ImageRegistrator::applyTransform(const cv::Mat &image, const cv::Mat &transform,
                                         const cv::Size &output_size)
{
    if (transform.empty())
    {
        throw std::runtime_error("变换矩阵为空");
    }

    cv::Mat registered_image;

    if (transform.rows == 3 && transform.cols == 3)
    {
        cv::warpPerspective(image, registered_image, transform, output_size, cv::INTER_LINEAR);
    }
    else if (transform.rows == 2 && transform.cols == 3)
    {
        cv::warpAffine(image, registered_image, transform, output_size, cv::INTER_LINEAR);
    }
    else
    {
        throw std::runtime_error("不支持的变换矩阵维度");
    }

    return registered_image;
}

RegistrationResult ImageRegistrator::registerImages(
    const cv::Mat &reference_img,
    const cv::Mat &target_img,
    FeatureDetectorType detector_type,
    TransformType transform_type)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    RegistrationResult result;

    // 1. 检测特征点
    std::cout << "检测特征点..." << std::endl;
    auto [keypoints1, descriptors1] = detectFeatures(reference_img, detector_type);
    auto [keypoints2, descriptors2] = detectFeatures(target_img, detector_type);

    std::cout << "参考图像特征点数: " << keypoints1.size() << std::endl;
    std::cout << "目标图像特征点数: " << keypoints2.size() << std::endl;

    // 2. 匹配特征点
    std::cout << "匹配特征点..." << std::endl;
    result.good_matches = matchFeatures(descriptors1, descriptors2, detector_type);
    std::cout << "匹配点数: " << result.good_matches.size() << std::endl;

    // 3. 估计变换矩阵（修复坑 4：inlier_matches 存进 result）
    std::cout << "估计变换矩阵..." << std::endl;
    result.transform_matrix = estimateTransform(
        keypoints1, keypoints2, result.good_matches, transform_type, result.inlier_matches);

    result.num_inliers = static_cast<int>(result.inlier_matches.size());
    std::cout << "内点数: " << result.num_inliers << std::endl;

    // ⚠️ 重要约定（修复坑 1）：
    //   transform_matrix 是 reference → target（points1 → points2）
    //   要把 target 对齐到 reference，必须求逆后再 warp
    cv::Mat warp_to_ref;
    if (!result.transform_matrix.empty())
    {
        if (result.transform_matrix.rows == 3 && result.transform_matrix.cols == 3)
        {
            cv::Mat inv;
            if (!cv::invert(result.transform_matrix, inv, cv::DECOMP_SVD))
            {
                throw std::runtime_error("无法求逆变换矩阵");
            }
            warp_to_ref = inv;
        }
        else if (result.transform_matrix.rows == 2 && result.transform_matrix.cols == 3)
        {
            cv::Mat h(3, 3, CV_64F, cv::Scalar(0));
            result.transform_matrix.copyTo(h.rowRange(0, 2));
            h.at<double>(2, 2) = 1.0;
            cv::Mat inv;
            if (!cv::invert(h, inv, cv::DECOMP_SVD))
            {
                throw std::runtime_error("无法求逆仿射矩阵");
            }
            warp_to_ref = inv;
        }
        else
        {
            throw std::runtime_error("不支持的变换矩阵维度");
        }
    }
    else
    {
        throw std::runtime_error("估计出的变换矩阵为空");
    }

    // 4. 应用变换
    std::cout << "应用变换..." << std::endl;
    result.registered_image = applyTransform(target_img, warp_to_ref, reference_img.size());

    // 5. RMS 误差（修复坑 2：内部已说明是"内点重投影误差"，不是"整图像素差"）
    result.rms_error = 0.0;
    if (!result.inlier_matches.empty() && !warp_to_ref.empty())
    {
        for (const auto &match : result.inlier_matches)
        {
            cv::Point2f pt_ref = keypoints1[match.queryIdx].pt;
            cv::Point2f pt_tar = keypoints2[match.trainIdx].pt;

            cv::Mat pt_tar_homog = (cv::Mat_<double>(3, 1) << pt_tar.x, pt_tar.y, 1);
            cv::Mat pt_tar_transformed = warp_to_ref * pt_tar_homog;
            pt_tar_transformed /= pt_tar_transformed.at<double>(2, 0);

            double dx = pt_ref.x - pt_tar_transformed.at<double>(0, 0);
            double dy = pt_ref.y - pt_tar_transformed.at<double>(1, 0);
            result.rms_error += dx * dx + dy * dy;
        }
        result.rms_error = std::sqrt(result.rms_error / result.inlier_matches.size());
    }

    result.keypoints1 = std::move(keypoints1);
    result.keypoints2 = std::move(keypoints2);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "配准完成，耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "RMS误差: " << result.rms_error << " 像素" << std::endl;

    return result;
}

cv::Mat ImageRegistrator::drawMatches(
    const cv::Mat &img1,
    const cv::Mat &img2,
    const std::vector<cv::KeyPoint> &keypoints1,
    const std::vector<cv::KeyPoint> &keypoints2,
    const std::vector<cv::DMatch> &matches)
{
    cv::Mat matches_img;
    cv::drawMatches(img1, keypoints1, img2, keypoints2, matches,
                    matches_img, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    return matches_img;
}

void ImageRegistrator::saveRegistrationResults(const RegistrationResult &result,
                                               const std::string &output_dir)
{
    std::filesystem::create_directories(output_dir);

    // 1. 保存配准后的图像
    {
        std::string output_path = output_dir + "/registered_image.jpg";
        cv::imwrite(output_path, result.registered_image);
    }

    // 2. 保存变换矩阵
    {
        std::string transform_path = output_dir + "/transform_matrix.txt";
        std::ofstream transform_file(transform_path);
        if (transform_file.is_open())
        {
            transform_file << "变换矩阵 (reference -> target):\n";
            transform_file << result.transform_matrix << std::endl;
            transform_file << "\n配准参数:\n";
            transform_file << "内点数: " << result.num_inliers << std::endl;
            transform_file << "RMS误差: " << result.rms_error << " 像素" << std::endl;
            transform_file.close();
        }
    }

    // 3. 保存 good_matches（兼容旧工具）
    {
        std::string matches_path = output_dir + "/matches.txt";
        std::ofstream matches_file(matches_path);
        if (matches_file.is_open())
        {
            matches_file << "匹配点数量: " << result.good_matches.size() << std::endl;
            matches_file << "\n匹配点详细信息:\n";
            matches_file << "索引, 参考图像X, 参考图像Y, 目标图像X, 目标图像Y, 距离\n";

            for (size_t i = 0; i < result.good_matches.size(); ++i)
            {
                const auto &match = result.good_matches[i];
                cv::Point2f pt1 = result.keypoints1[match.queryIdx].pt;
                cv::Point2f pt2 = result.keypoints2[match.trainIdx].pt;

                matches_file << i << ", " << pt1.x << ", " << pt1.y << ", "
                             << pt2.x << ", " << pt2.y << ", "
                             << match.distance << std::endl;
            }
            matches_file.close();
        }
    }

    // 4. 保存 inlier_matches（修复坑 4：RANSAC 内点，更可信）
    {
        std::string inliers_path = output_dir + "/inlier_matches.txt";
        std::ofstream inliers_file(inliers_path);
        if (inliers_file.is_open())
        {
            inliers_file << "RANSAC 内点数量: " << result.inlier_matches.size() << std::endl;
            inliers_file << "\n内点详细信息:\n";
            inliers_file << "索引, 参考图像X, 参考图像Y, 目标图像X, 目标图像Y, 距离\n";

            for (size_t i = 0; i < result.inlier_matches.size(); ++i)
            {
                const auto &match = result.inlier_matches[i];
                cv::Point2f pt1 = result.keypoints1[match.queryIdx].pt;
                cv::Point2f pt2 = result.keypoints2[match.trainIdx].pt;

                inliers_file << i << ", " << pt1.x << ", " << pt1.y << ", "
                             << pt2.x << ", " << pt2.y << ", "
                             << match.distance << std::endl;
            }
            inliers_file.close();
        }
    }
}