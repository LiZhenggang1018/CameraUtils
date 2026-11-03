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
    inlier_matches.clear();
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
