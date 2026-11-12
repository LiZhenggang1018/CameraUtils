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
    // 修复 #11：兼容 BGR / BGRA / GRAY 三种输入
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
                transform.copyTo(h.rowRange(0, 2));
                h.at<double>(2, 2) = 1.0;
                transform = h;
            }
            break;
        }
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
            // 计算平移向量
            std::vector<float> dx, dy;
            for (size_t i = 0; i < points1.size(); ++i)
            {
                dx.push_back(points2[i].x - points1[i].x);
                dy.push_back(points2[i].y - points1[i].y);
            }

            // 使用中位数作为平移量
            std::nth_element(dx.begin(), dx.begin() + dx.size() / 2, dx.end());
            std::nth_element(dy.begin(), dy.begin() + dy.size() / 2, dy.end());
            float tx = dx[dx.size() / 2];
            float ty = dy[dy.size() / 2];

            transform = (cv::Mat_<double>(3, 3) << 1, 0, tx, 0, 1, ty, 0, 0, 1);
            inliers_mask = std::vector<uchar>(points1.size(), 1);
            break;
        }
        }
    }
    catch (const cv::Exception &e)
    {
        std::cerr << "OpenCV异常: " << e.what() << std::endl;
        throw std::runtime_error("估计变换矩阵失败");
    }

    // 提取内点匹配
    // 修复 #10：inliers_mask 可能为空（RANSAC 失败），先检查
    if (transform.empty() || inliers_mask.size() != matches.size())
    {
        std::cerr << "Warning: estimateTransform failed or inliers_mask size mismatch"
                << " (matches=" << matches.size()
                << ", mask=" << inliers_mask.size() << ")" << std::endl;
        inlier_matches.clear();
        return transform;   // 返回空矩阵，由调用方检查
    }

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

cv::Mat ImageRegistrator::applyTransform(const cv::Mat &image, const cv::Mat &transform, const cv::Size &output_size)
{
    if (transform.empty())
    {
        throw std::runtime_error("变换矩阵为空");
    }

    cv::Mat registered_image;

    if (transform.rows == 3 && transform.cols == 3)
    {
        // 单应性变换
        cv::warpPerspective(image, registered_image, transform, output_size, cv::INTER_LINEAR);
    }
    else if (transform.rows == 2 && transform.cols == 3)
    {
        // 仿射变换
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

    // 3. 估计变换矩阵
    std::cout << "估计变换矩阵..." << std::endl;
    std::vector<cv::DMatch> inlier_matches;
    result.transform_matrix = estimateTransform(keypoints1, keypoints2,
                                                result.good_matches,
                                                transform_type,
                                                inlier_matches);

    result.num_inliers = static_cast<int>(inlier_matches.size());
    std::cout << "内点数: " << result.num_inliers << std::endl;

    // 重要：估计得到的矩阵 result.transform_matrix是从 reference（视频帧） -> target（封面帧）（points1 -> points2）把reference.img映射到target.img的变换
    // 也是我们打印在transform_matrix.txt里的矩阵含义。表示缩小reference（视频帧）之后，会对齐到target（封面帧）。

    // 我们为了把 target（封面帧） 对齐到 reference（视频帧），需要使用该矩阵的逆（或在估计时反向求解）。
    cv::Mat warp_to_ref; // warp_to_ref表示 target -> reference 的变换（把目标图像的点/像素映射到参考图坐标系）
    if (!result.transform_matrix.empty())
    {
        // 如果是 3x3 矩阵，直接求逆
        if (result.transform_matrix.rows == 3 && result.transform_matrix.cols == 3)
        {
            cv::Mat inv;
            if (!cv::invert(result.transform_matrix, inv, cv::DECOMP_SVD))
            {
                throw std::runtime_error("无法求逆变换矩阵");
            }
            warp_to_ref = inv;
        }
        // 如果 estimate 返回 2x3 仿射（已被转换为 3x3 的情形也会走上面分支）
        else if (result.transform_matrix.rows == 2 && result.transform_matrix.cols == 3)
        {
            // 转为 3x3 再求逆
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

    // 4. 应用（对目标图像应用 target->reference 的变换）
    std::cout << "应用变换..." << std::endl;
    result.registered_image = applyTransform(target_img, warp_to_ref, reference_img.size());

    // 计算RMS误差（使用相同方向：把目标点通过 warp_to_ref 映射到参考图像坐标，与参考点比较）
    result.rms_error = 0.0;
    if (!inlier_matches.empty() && !warp_to_ref.empty())
    {
        for (const auto &match : inlier_matches)
        {
            cv::Point2f pt_ref = keypoints1[match.queryIdx].pt; // reference
            cv::Point2f pt_tar = keypoints2[match.trainIdx].pt; // target

            cv::Mat pt_tar_homog = (cv::Mat_<double>(3, 1) << pt_tar.x, pt_tar.y, 1);
            cv::Mat pt_tar_transformed = warp_to_ref * pt_tar_homog;
            pt_tar_transformed /= pt_tar_transformed.at<double>(2, 0);

            double dx = pt_ref.x - pt_tar_transformed.at<double>(0, 0);
            double dy = pt_ref.y - pt_tar_transformed.at<double>(1, 0);
            result.rms_error += dx * dx + dy * dy;
        }
        result.rms_error = std::sqrt(result.rms_error / inlier_matches.size());
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

void ImageRegistrator::saveRegistrationResults(const RegistrationResult &result, const std::string &output_dir)
{

    // 创建输出目录
    std::filesystem::create_directories(output_dir);

    // 保存配准后的图像
    std::string output_path = output_dir + "/registered_image.jpg";
    cv::imwrite(output_path, result.registered_image);

    // 保存变换矩阵
    std::string transform_path = output_dir + "/transform_matrix.txt";
    std::ofstream transform_file(transform_path);
    if (transform_file.is_open())
    {
        transform_file << "变换矩阵:\n";
        transform_file << result.transform_matrix << std::endl;
        transform_file << "\n配准参数:\n";
        transform_file << "内点数: " << result.num_inliers << std::endl;
        transform_file << "RMS误差: " << result.rms_error << " 像素" << std::endl;
        transform_file.close();
    }

    // 保存匹配点信息
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